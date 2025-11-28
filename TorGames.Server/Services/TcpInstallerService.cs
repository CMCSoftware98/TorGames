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
                _clientManager.NotifyCommandResult(connection.Client.ConnectionKey, result);
                break;

            default:
                _logger.LogDebug("Unknown message type '{Type}' from {Key}",
                    message.Type, connection.Client.ConnectionKey);
                break;
        }
    }

    /// <summary>
    /// Automatically sends a download command to an installer client.
    /// </summary>
    private async Task SendAutoInstallCommandAsync(TcpClientConnection connection, CancellationToken ct)
    {
        try
        {
            // Get the latest client version info
            var latestVersion = _updateService.GetLatestVersion();
            if (latestVersion == null)
            {
                _logger.LogWarning("No client update available to send to installer {Key}",
                    connection.Client.ConnectionKey);
                return;
            }

            // Build download URL
            var downloadUrl = $"{_serverBaseUrl}/api/updates/download/{latestVersion}";

            _logger.LogInformation("Sending auto-install command to {Key}: {Url}",
                connection.Client.ConnectionKey, downloadUrl);

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
            _logger.LogDebug(ex, "Failed to send command to {Key}", connectionKey);
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
            IsAdmin = false,
            CountryCode = "",
            IsUacEnabled = true
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

    // Heartbeat fields
    public long? Uptime { get; set; }
    public long? AvailMemory { get; set; }

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
