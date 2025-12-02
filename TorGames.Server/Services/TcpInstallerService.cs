using System.Collections.Concurrent;
using System.Net;
using System.Net.Sockets;
using System.Text;
using System.Text.Json;
using System.Text.Json.Serialization;
using TorGames.Common.Protos;
using TorGames.Server.Models;

namespace TorGames.Server.Services;

/// <summary>
/// Ultra-robust TCP listener for lightweight InstallerPlus clients.
/// Designed to NEVER shut down - all errors are caught and handled gracefully.
/// Uses simple JSON-over-TCP protocol: [4-byte length BE] + [JSON payload]
/// </summary>
public class TcpInstallerService : BackgroundService
{
    private readonly ILogger<TcpInstallerService> _logger;
    private readonly ClientManager _clientManager;
    private readonly IConfiguration _configuration;
    private readonly UpdateService _updateService;

    private readonly ConcurrentDictionary<string, TcpClientConnection> _tcpClients = new();
    private TcpListener? _listener;
    private readonly int _port;
    private readonly string _serverBaseUrl;
    private readonly JsonSerializerOptions _jsonOptions;

    // Restart tracking
    private int _restartCount;
    private DateTime _lastRestartTime = DateTime.MinValue;
    private const int MaxRestartsPerMinute = 10;

    public TcpInstallerService(
        ILogger<TcpInstallerService> logger,
        ClientManager clientManager,
        IConfiguration configuration,
        UpdateService updateService)
    {
        _logger = logger;
        _clientManager = clientManager;
        _configuration = configuration;
        _updateService = updateService;
        _port = configuration.GetValue("TcpInstaller:Port", 5050);
        _serverBaseUrl = configuration.GetValue("TcpInstaller:ServerBaseUrl", "http://144.91.111.101:5001");

        _jsonOptions = new JsonSerializerOptions
        {
            PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
            PropertyNameCaseInsensitive = true,
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("TcpInstallerService starting on port {Port}", _port);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await RunListenerAsync(stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                _logger.LogInformation("TcpInstallerService shutting down gracefully");
                break;
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "TcpInstallerService listener crashed - restarting...");

                // Rate limit restarts
                if (!ShouldRestart())
                {
                    _logger.LogCritical("Too many restarts - waiting 30 seconds before retry");
                    await SafeDelay(TimeSpan.FromSeconds(30), stoppingToken);
                }
                else
                {
                    await SafeDelay(TimeSpan.FromSeconds(1), stoppingToken);
                }
            }
            finally
            {
                SafeStopListener();
            }
        }

        // Cleanup all connections
        await CleanupAllConnectionsAsync();
        _logger.LogInformation("TcpInstallerService stopped");
    }

    private async Task RunListenerAsync(CancellationToken stoppingToken)
    {
        _listener = new TcpListener(IPAddress.Any, _port);
        _listener.Start();
        _logger.LogInformation("TCP listener started on port {Port}", _port);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                var client = await _listener.AcceptTcpClientAsync(stoppingToken);
                _ = HandleClientAsync(client, stoppingToken);
            }
            catch (OperationCanceledException) when (stoppingToken.IsCancellationRequested)
            {
                break;
            }
            catch (SocketException ex) when (ex.SocketErrorCode == SocketError.Interrupted)
            {
                // Listener was stopped
                break;
            }
            catch (Exception ex)
            {
                _logger.LogWarning(ex, "Error accepting TCP client - continuing...");
                await SafeDelay(TimeSpan.FromMilliseconds(100), stoppingToken);
            }
        }
    }

    private async Task HandleClientAsync(TcpClient tcpClient, CancellationToken stoppingToken)
    {
        string? connectionKey = null;
        var remoteEndpoint = "unknown";

        try
        {
            remoteEndpoint = tcpClient.Client.RemoteEndPoint?.ToString() ?? "unknown";
            _logger.LogDebug("TCP client connected from {Endpoint}", remoteEndpoint);

            // Configure socket
            tcpClient.NoDelay = true;
            tcpClient.ReceiveTimeout = 60000; // 60 seconds
            tcpClient.SendTimeout = 30000;    // 30 seconds

            using var stream = tcpClient.GetStream();
            using var cts = CancellationTokenSource.CreateLinkedTokenSource(stoppingToken);

            // Wait for registration (first message must be registration)
            var registrationJson = await ReadMessageAsync(stream, cts.Token);
            if (registrationJson == null)
            {
                _logger.LogDebug("Client {Endpoint} disconnected before registration", remoteEndpoint);
                return;
            }

            var registration = ParseMessage(registrationJson);
            if (registration?.Type != "registration" || string.IsNullOrEmpty(registration.ClientId))
            {
                _logger.LogWarning("Invalid registration from {Endpoint}", remoteEndpoint);
                await SendErrorAndClose(stream, "Invalid registration message");
                return;
            }

            connectionKey = $"{registration.ClientId}:{registration.ClientType?.ToUpperInvariant() ?? "INSTALLER"}";

            // Create and register client
            var connectedClient = CreateConnectedClient(registration, remoteEndpoint);
            var tcpConnection = new TcpClientConnection(tcpClient, stream, connectedClient, cts);

            // Set the command sender delegate so commands can be sent via TCP
            connectedClient.CommandSender = async (command) => await SendCommandToConnectionAsync(tcpConnection, command);

            var (success, rejectionReason) = _clientManager.TryRegisterClient(connectedClient);
            if (!success)
            {
                _logger.LogWarning("Registration rejected for {ConnectionKey}: {Reason}",
                    connectionKey, rejectionReason);
                await SendErrorAndClose(stream, rejectionReason ?? "Registration rejected");
                return;
            }

            _tcpClients[connectionKey] = tcpConnection;
            _logger.LogInformation("TCP client registered: {ConnectionKey} ({MachineName})",
                connectionKey, connectedClient.MachineName);

            // Send acceptance
            await SendMessageAsync(stream, new TcpMessage
            {
                Type = "accepted",
                ClientId = registration.ClientId
            }, cts.Token);

            // Auto-send download command for INSTALLER clients
            if (connectedClient.ClientType == "INSTALLER")
            {
                await SendAutoInstallCommandAsync(tcpConnection, cts.Token);
            }

            // Main message loop
            await ProcessMessagesAsync(tcpConnection, cts.Token);
        }
        catch (OperationCanceledException)
        {
            _logger.LogDebug("TCP client {Key} operation cancelled", connectionKey ?? remoteEndpoint);
        }
        catch (IOException ex)
        {
            _logger.LogDebug("TCP client {Key} IO error: {Message}", connectionKey ?? remoteEndpoint, ex.Message);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Error handling TCP client {Key}", connectionKey ?? remoteEndpoint);
        }
        finally
        {
            if (connectionKey != null)
            {
                _tcpClients.TryRemove(connectionKey, out _);
                _clientManager.RemoveClient(connectionKey);
                _logger.LogInformation("TCP client disconnected: {ConnectionKey}", connectionKey);
            }

            SafeDispose(tcpClient);
        }
    }

    private async Task ProcessMessagesAsync(TcpClientConnection connection, CancellationToken ct)
    {
        var lastHeartbeat = DateTime.UtcNow;
        var heartbeatTimeout = TimeSpan.FromSeconds(60);

        while (!ct.IsCancellationRequested)
        {
            try
            {
                // Read with timeout to allow heartbeat checking
                var json = await ReadMessageWithTimeoutAsync(connection.Stream, TimeSpan.FromSeconds(5), ct);

                if (json == null)
                {
                    // Check for heartbeat timeout
                    if (DateTime.UtcNow - lastHeartbeat > heartbeatTimeout)
                    {
                        _logger.LogDebug("TCP client {Key} heartbeat timeout", connection.Client.ConnectionKey);
                        break;
                    }
                    continue;
                }

                lastHeartbeat = DateTime.UtcNow;
                var message = ParseMessage(json);

                if (message == null)
                {
                    _logger.LogDebug("Invalid JSON from {Key}", connection.Client.ConnectionKey);
                    continue;
                }

                await HandleMessageAsync(connection, message, ct);
            }
            catch (IOException)
            {
                // Connection lost
                break;
            }
            catch (Exception ex)
            {
                _logger.LogDebug(ex, "Error processing message from {Key}", connection.Client.ConnectionKey);
            }
        }
    }

    private async Task HandleMessageAsync(TcpClientConnection connection, TcpMessage message, CancellationToken ct)
    {
        switch (message.Type?.ToLowerInvariant())
        {
            case "heartbeat":
                // Check if client was removed from ClientManager (e.g., by cleanup service)
                // If so, re-register it since the TCP connection is still active
                if (_clientManager.GetClient(connection.Client.ConnectionKey) == null)
                {
                    _logger.LogInformation(
                        "Re-registering client {ConnectionKey} after cleanup",
                        connection.Client.ConnectionKey);

                    var (success, _) = _clientManager.TryRegisterClient(connection.Client);
                    if (!success)
                    {
                        _logger.LogWarning(
                            "Failed to re-register client {ConnectionKey}",
                            connection.Client.ConnectionKey);
                    }
                }

                var heartbeat = new Heartbeat
                {
                    UptimeSeconds = message.Uptime ?? 0,
                    CpuUsagePercent = 0,
                    AvailableMemoryBytes = message.AvailMemory ?? 0
                };
                _clientManager.UpdateHeartbeat(connection.Client.ConnectionKey, heartbeat);
                break;

            case "result":
                var result = new CommandResult
                {
                    CommandId = message.CommandId ?? "",
                    Success = message.Success ?? false,
                    ExitCode = message.ExitCode ?? -1,
                    Stdout = message.Stdout ?? "",
                    Stderr = "",
                    ErrorMessage = message.Error ?? ""
                };

                // Check if this is a systeminfo response by looking for pending request
                var client = _clientManager.GetClient(connection.Client.ConnectionKey);
                if (client != null && result.Success && !string.IsNullOrEmpty(result.Stdout))
                {
                    // Try to parse as detailed system info JSON
                    var detailedInfo = TryParseDetailedSystemInfo(result.Stdout);
                    if (detailedInfo != null)
                    {
                        _logger.LogDebug("Parsed detailed system info for command {CommandId}", result.CommandId);
                        client.OnDetailedSystemInfoReceived(result.CommandId, detailedInfo);
                    }
                }

                _clientManager.NotifyCommandResult(connection.Client.ConnectionKey, result);
                break;

            case "check_update":
                await HandleCheckUpdateAsync(connection, message, ct);
                break;

            default:
                _logger.LogDebug("Unknown message type '{Type}' from {Key}",
                    message.Type, connection.Client.ConnectionKey);
                break;
        }
    }

    /// <summary>
    /// Handles update check request from client.
    /// </summary>
    private async Task HandleCheckUpdateAsync(TcpClientConnection connection, TcpMessage message, CancellationToken ct)
    {
        try
        {
            var currentVersion = message.CurrentVersion ?? "";

            // For now, assume non-test client - test mode is stored in database, not in-memory client
            var isTestClient = false;

            _logger.LogInformation("Client {Key} checking for update (current: {Version})",
                connection.Client.ConnectionKey, currentVersion);

            var updateCheck = _updateService.CheckForUpdate(currentVersion, isTestClient);

            if (updateCheck.UpdateAvailable)
            {
                _logger.LogInformation("Update available for {Key}: {Version}",
                    connection.Client.ConnectionKey, updateCheck.LatestVersion);

                // Send update command
                var downloadUrl = $"{_serverBaseUrl}/api/update/download/latest";

                var updateCommand = new TcpMessage
                {
                    Type = "command",
                    CommandId = Guid.NewGuid().ToString(),
                    CommandType = "update",
                    CommandText = downloadUrl,
                    Timeout = 300 // 5 minutes for download
                };

                await SendMessageAsync(connection.Stream, updateCommand, ct);
            }
            else
            {
                _logger.LogDebug("No update available for {Key} (current: {Version})",
                    connection.Client.ConnectionKey, currentVersion);
            }
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to handle update check from {Key}",
                connection.Client.ConnectionKey);
        }
    }

    /// <summary>
    /// Automatically sends a download command to an installer client.
    /// </summary>
    private async Task SendAutoInstallCommandAsync(TcpClientConnection connection, CancellationToken ct)
    {
        try
        {
            // Check if there's a version available
            var latestVersionInfo = _updateService.GetLatestVersion();
            if (latestVersionInfo == null)
            {
                _logger.LogWarning("No client update available to send to installer {Key}",
                    connection.Client.ConnectionKey);
                return;
            }

            // Build download URL - use /latest endpoint so installer doesn't need version number
            var downloadUrl = $"{_serverBaseUrl}/api/update/download/latest";

            _logger.LogInformation("Sending auto-install command to {Key}: {Url} (version: {Version})",
                connection.Client.ConnectionKey, downloadUrl, latestVersionInfo.Version);

            var message = new TcpMessage
            {
                Type = "command",
                CommandId = Guid.NewGuid().ToString(),
                CommandType = "download",
                CommandText = downloadUrl,
                Timeout = 300 // 5 minutes for download
            };

            await SendMessageAsync(connection.Stream, message, ct);
            _logger.LogInformation("Auto-install command sent to {Key}", connection.Client.ConnectionKey);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to send auto-install command to {Key}",
                connection.Client.ConnectionKey);
        }
    }

    /// <summary>
    /// Sends a command to a TCP client by connection key.
    /// </summary>
    public async Task<bool> SendCommandAsync(string connectionKey, Command command)
    {
        if (!_tcpClients.TryGetValue(connectionKey, out var connection))
            return false;

        return await SendCommandToConnectionAsync(connection, command);
    }

    /// <summary>
    /// Sends a command to a specific TCP connection.
    /// </summary>
    private async Task<bool> SendCommandToConnectionAsync(TcpClientConnection connection, Command command)
    {
        try
        {
            var message = new TcpMessage
            {
                Type = "command",
                CommandId = command.CommandId,
                CommandType = command.CommandType,
                CommandText = command.CommandText,
                Timeout = command.TimeoutSeconds
            };

            await SendMessageAsync(connection.Stream, message, connection.Cts.Token);
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogDebug(ex, "Failed to send command to {Key}", connection.Client.ConnectionKey);
            return false;
        }
    }

    /// <summary>
    /// Checks if a client is connected via TCP.
    /// </summary>
    public bool IsTcpClient(string connectionKey) => _tcpClients.ContainsKey(connectionKey);

    #region Message I/O

    private async Task<string?> ReadMessageAsync(NetworkStream stream, CancellationToken ct)
    {
        return await ReadMessageWithTimeoutAsync(stream, TimeSpan.FromSeconds(30), ct);
    }

    private async Task<string?> ReadMessageWithTimeoutAsync(NetworkStream stream, TimeSpan timeout, CancellationToken ct)
    {
        try
        {
            using var timeoutCts = new CancellationTokenSource(timeout);
            using var linkedCts = CancellationTokenSource.CreateLinkedTokenSource(ct, timeoutCts.Token);

            // Read 4-byte length header
            var header = new byte[4];
            var headerRead = 0;

            while (headerRead < 4)
            {
                if (!stream.DataAvailable)
                {
                    await Task.Delay(10, linkedCts.Token);
                    if (!stream.DataAvailable)
                    {
                        if (timeoutCts.IsCancellationRequested)
                            return null; // Timeout - not an error
                        continue;
                    }
                }

                var read = await stream.ReadAsync(header.AsMemory(headerRead, 4 - headerRead), linkedCts.Token);
                if (read == 0) return null; // Connection closed
                headerRead += read;
            }

            // Parse length (big-endian)
            var length = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
            if (length <= 0 || length > 1024 * 1024) // Max 1MB
            {
                _logger.LogWarning("Invalid message length: {Length}", length);
                return null;
            }

            // Read payload
            var payload = new byte[length];
            var payloadRead = 0;

            while (payloadRead < length)
            {
                var read = await stream.ReadAsync(payload.AsMemory(payloadRead, length - payloadRead), linkedCts.Token);
                if (read == 0) return null;
                payloadRead += read;
            }

            return Encoding.UTF8.GetString(payload);
        }
        catch (OperationCanceledException) when (!ct.IsCancellationRequested)
        {
            return null; // Timeout
        }
    }

    private async Task SendMessageAsync(NetworkStream stream, TcpMessage message, CancellationToken ct)
    {
        var json = JsonSerializer.Serialize(message, _jsonOptions);
        var payload = Encoding.UTF8.GetBytes(json);
        var length = payload.Length;

        var header = new byte[]
        {
            (byte)((length >> 24) & 0xFF),
            (byte)((length >> 16) & 0xFF),
            (byte)((length >> 8) & 0xFF),
            (byte)(length & 0xFF)
        };

        await stream.WriteAsync(header, ct);
        await stream.WriteAsync(payload, ct);
        await stream.FlushAsync(ct);
    }

    private async Task SendErrorAndClose(NetworkStream stream, string error)
    {
        try
        {
            await SendMessageAsync(stream, new TcpMessage
            {
                Type = "error",
                Error = error
            }, CancellationToken.None);
        }
        catch { /* Ignore */ }
    }

    private TcpMessage? ParseMessage(string json)
    {
        try
        {
            return JsonSerializer.Deserialize<TcpMessage>(json, _jsonOptions);
        }
        catch (Exception ex)
        {
            _logger.LogDebug(ex, "Failed to parse JSON message");
            return null;
        }
    }

    #endregion

    #region Helpers

    private ConnectedClient CreateConnectedClient(TcpMessage reg, string remoteEndpoint)
    {
        var clientIp = remoteEndpoint.Split(':')[0];

        return new ConnectedClient
        {
            ClientId = reg.ClientId ?? "",
            ClientType = reg.ClientType?.ToUpperInvariant() ?? "INSTALLER",
            MachineName = reg.MachineName ?? "Unknown",
            OsVersion = reg.OsVersion ?? "Unknown",
            OsArchitecture = reg.OsArch ?? "x64",
            CpuCount = (int)(reg.CpuCount ?? 0),
            TotalMemoryBytes = reg.TotalMemory ?? 0,
            Username = reg.Username ?? "Unknown",
            ClientVersion = reg.ClientVersion ?? "1.0.0",
            IpAddress = string.IsNullOrEmpty(reg.IpAddress) ? clientIp : reg.IpAddress,
            MacAddress = reg.MacAddress ?? "",
            CountryCode = reg.CountryCode ?? "",
            IsAdmin = reg.IsAdmin ?? false,
            IsUacEnabled = reg.IsUacEnabled ?? true
        };
    }

    private bool ShouldRestart()
    {
        var now = DateTime.UtcNow;
        if ((now - _lastRestartTime).TotalMinutes >= 1)
        {
            _restartCount = 0;
        }

        _lastRestartTime = now;
        _restartCount++;

        return _restartCount <= MaxRestartsPerMinute;
    }

    private async Task SafeDelay(TimeSpan delay, CancellationToken ct)
    {
        try
        {
            await Task.Delay(delay, ct);
        }
        catch (OperationCanceledException) { }
    }

    private void SafeStopListener()
    {
        try
        {
            _listener?.Stop();
            _listener = null;
        }
        catch (Exception ex)
        {
            _logger.LogDebug(ex, "Error stopping listener");
        }
    }

    private void SafeDispose(IDisposable? disposable)
    {
        try
        {
            disposable?.Dispose();
        }
        catch { /* Ignore */ }
    }

    private async Task CleanupAllConnectionsAsync()
    {
        _logger.LogInformation("Cleaning up {Count} TCP connections...", _tcpClients.Count);

        foreach (var (key, connection) in _tcpClients.ToArray())
        {
            try
            {
                connection.Cts.Cancel();
                SafeDispose(connection.TcpClient);
                _clientManager.RemoveClient(key);
            }
            catch { /* Ignore */ }
        }

        _tcpClients.Clear();
        await Task.CompletedTask;
    }

    /// <summary>
    /// Tries to parse a JSON string as DetailedSystemInfo.
    /// Returns null if parsing fails or JSON doesn't match expected format.
    /// </summary>
    private DetailedSystemInfo? TryParseDetailedSystemInfo(string json)
    {
        try
        {
            // Try to detect if this is system info JSON by checking for expected fields
            using var doc = JsonDocument.Parse(json);
            var root = doc.RootElement;

            // The C++ client sends: machineName, username, osVersion, architecture, cpuCount, totalMemory, availableMemory, localIp, isAdmin, uacEnabled, gpu, drives
            if (!root.TryGetProperty("machineName", out _) &&
                !root.TryGetProperty("osVersion", out _) &&
                !root.TryGetProperty("cpuCount", out _))
            {
                return null; // Not system info
            }

            var info = new DetailedSystemInfo();

            // Parse OS info
            info.Os = new OsInfo
            {
                Name = "Windows",
                Version = GetJsonString(root, "osVersion"),
                Architecture = GetJsonString(root, "architecture"),
                RegisteredUser = GetJsonString(root, "username")
            };

            // Parse CPU info
            info.Cpu = new CpuInfo
            {
                Cores = GetJsonInt(root, "cpuCount"),
                LogicalProcessors = GetJsonInt(root, "cpuCount"),
                Architecture = GetJsonString(root, "architecture")
            };

            // Parse Memory info
            info.Memory = new MemoryInfo
            {
                TotalPhysicalBytes = GetJsonLong(root, "totalMemory"),
                AvailablePhysicalBytes = GetJsonLong(root, "availableMemory"),
                MemoryLoadPercent = info.Memory?.TotalPhysicalBytes > 0
                    ? (int)((1 - (double)GetJsonLong(root, "availableMemory") / GetJsonLong(root, "totalMemory")) * 100)
                    : 0
            };

            // Parse GPU info
            var gpu = GetJsonString(root, "gpu");
            if (!string.IsNullOrEmpty(gpu))
            {
                info.Gpus.Add(new GpuInfo { Name = gpu });
            }

            // Parse drives
            if (root.TryGetProperty("drives", out var drivesElement) && drivesElement.ValueKind == JsonValueKind.Array)
            {
                foreach (var drive in drivesElement.EnumerateArray())
                {
                    var driveStr = drive.GetString();
                    if (!string.IsNullOrEmpty(driveStr))
                    {
                        // Format: "C: 42.5GB free / 120GB total"
                        var driveLetter = driveStr.Length > 0 ? driveStr.Substring(0, 2) : "";
                        info.Disks.Add(new DiskInfo
                        {
                            Name = driveStr,
                            DriveLetter = driveLetter
                        });
                    }
                }
            }

            // Parse performance info
            info.Performance = new PerformanceInfo
            {
                AvailableMemoryBytes = GetJsonLong(root, "availableMemory")
            };

            // Parse hardware info
            info.Hardware = new SystemHardwareInfo
            {
                SystemType = GetJsonString(root, "architecture")
            };

            // Network adapter (from localIp)
            var localIp = GetJsonString(root, "localIp");
            if (!string.IsNullOrEmpty(localIp))
            {
                info.NetworkAdapters.Add(new NetworkAdapterInfo
                {
                    IpAddress = localIp,
                    Name = "Local Network"
                });
            }

            _logger.LogDebug("Successfully parsed system info: OS={Os}, CPU cores={Cores}, Memory={Mem}MB",
                info.Os?.Version, info.Cpu?.Cores, info.Memory?.TotalPhysicalBytes / 1024 / 1024);

            return info;
        }
        catch (Exception ex)
        {
            _logger.LogDebug(ex, "Failed to parse JSON as DetailedSystemInfo");
            return null;
        }
    }

    private static string GetJsonString(JsonElement element, string propertyName)
    {
        return element.TryGetProperty(propertyName, out var prop) ? prop.GetString() ?? "" : "";
    }

    private static int GetJsonInt(JsonElement element, string propertyName)
    {
        return element.TryGetProperty(propertyName, out var prop) && prop.ValueKind == JsonValueKind.Number
            ? prop.GetInt32() : 0;
    }

    private static long GetJsonLong(JsonElement element, string propertyName)
    {
        return element.TryGetProperty(propertyName, out var prop) && prop.ValueKind == JsonValueKind.Number
            ? prop.GetInt64() : 0;
    }

    #endregion

    /// <summary>
    /// Internal class to track TCP client connections.
    /// </summary>
    private class TcpClientConnection
    {
        public TcpClient TcpClient { get; }
        public NetworkStream Stream { get; }
        public ConnectedClient Client { get; }
        public CancellationTokenSource Cts { get; }

        public TcpClientConnection(TcpClient tcpClient, NetworkStream stream,
            ConnectedClient client, CancellationTokenSource cts)
        {
            TcpClient = tcpClient;
            Stream = stream;
            Client = client;
            Cts = cts;
        }
    }
}

/// <summary>
/// JSON message structure for TCP protocol.
/// </summary>
public class TcpMessage
{
    public string? Type { get; set; }
    public string? ClientId { get; set; }
    public string? ClientType { get; set; }

    // Registration fields
    public string? MachineName { get; set; }
    public string? OsVersion { get; set; }
    public string? OsArch { get; set; }
    public long? CpuCount { get; set; }
    public long? TotalMemory { get; set; }
    public string? Username { get; set; }
    public string? ClientVersion { get; set; }
    public string? IpAddress { get; set; }
    public string? MacAddress { get; set; }
    public string? CountryCode { get; set; }
    public bool? IsAdmin { get; set; }
    public bool? IsUacEnabled { get; set; }

    // Heartbeat fields
    public long? Uptime { get; set; }
    public long? AvailMemory { get; set; }

    // Update check fields
    public string? CurrentVersion { get; set; }

    // Command fields
    public string? CommandId { get; set; }
    public string? CommandType { get; set; }
    public string? CommandText { get; set; }
    public int? Timeout { get; set; }

    // Result fields
    public bool? Success { get; set; }
    public int? ExitCode { get; set; }
    public string? Stdout { get; set; }
    public string? Error { get; set; }
}
