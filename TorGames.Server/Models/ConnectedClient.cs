using Grpc.Core;
using TorGames.Common.Protos;

namespace TorGames.Server.Models;

/// <summary>
/// Represents a connected client with its state and communication stream.
/// </summary>
public class ConnectedClient
{
    public string ClientId { get; init; } = string.Empty;
    public string ClientType { get; init; } = string.Empty;
    public string ConnectionKey => $"{ClientId}:{ClientType}";

    // Registration info
    public string MachineName { get; set; } = string.Empty;
    public string OsVersion { get; set; } = string.Empty;
    public string OsArchitecture { get; set; } = string.Empty;
    public int CpuCount { get; set; }
    public long TotalMemoryBytes { get; set; }
    public string Username { get; set; } = string.Empty;
    public string ClientVersion { get; set; } = string.Empty;
    public string IpAddress { get; set; } = string.Empty;
    public string MacAddress { get; set; } = string.Empty;
    public bool IsAdmin { get; set; }
    public string CountryCode { get; set; } = string.Empty;
    public bool IsUacEnabled { get; set; } = true;

    // Connection state
    public DateTime ConnectedAt { get; init; } = DateTime.UtcNow;
    public DateTime LastHeartbeat { get; set; } = DateTime.UtcNow;
    public bool IsOnline => (DateTime.UtcNow - LastHeartbeat).TotalSeconds < 30;

    // Latest metrics
    public double CpuUsagePercent { get; set; }
    public long AvailableMemoryBytes { get; set; }
    public long UptimeSeconds { get; set; }

    // Activity status (Idling, Executing Command, Shutting Down, Restarting, etc.)
    public string ActivityStatus { get; set; } = "Idling";
    public DateTime ActivityStatusUpdatedAt { get; set; } = DateTime.UtcNow;

    /// <summary>
    /// Updates the activity status of the client.
    /// </summary>
    public void SetActivityStatus(string status)
    {
        ActivityStatus = status;
        ActivityStatusUpdatedAt = DateTime.UtcNow;
    }

    // Detailed system info (cached from last request)
    public DetailedSystemInfo? LastDetailedSystemInfo { get; set; }
    public DateTime? LastDetailedSystemInfoTime { get; set; }

    // Pending system info requests (requestId -> TaskCompletionSource)
    private readonly Dictionary<string, TaskCompletionSource<DetailedSystemInfo>> _pendingSystemInfoRequests = new();
    private readonly object _requestLock = new();

    // Communication stream
    public IServerStreamWriter<ServerMessage>? ResponseStream { get; set; }
    public CancellationToken CancellationToken { get; set; }

    /// <summary>
    /// Updates client info from registration message.
    /// </summary>
    public void UpdateFromRegistration(Registration registration)
    {
        MachineName = registration.MachineName;
        OsVersion = registration.OsVersion;
        OsArchitecture = registration.OsArchitecture;
        CpuCount = registration.CpuCount;
        TotalMemoryBytes = registration.TotalMemoryBytes;
        Username = registration.Username;
        ClientVersion = registration.ClientVersion;
        IpAddress = registration.IpAddress;
        MacAddress = registration.MacAddress;
        IsAdmin = registration.IsAdmin;
        CountryCode = registration.CountryCode;
        IsUacEnabled = registration.IsUacEnabled;
        LastHeartbeat = DateTime.UtcNow;
    }

    /// <summary>
    /// Updates client state from heartbeat message.
    /// </summary>
    public void UpdateFromHeartbeat(Heartbeat heartbeat)
    {
        UptimeSeconds = heartbeat.UptimeSeconds;
        CpuUsagePercent = heartbeat.CpuUsagePercent;
        AvailableMemoryBytes = heartbeat.AvailableMemoryBytes;
        LastHeartbeat = DateTime.UtcNow;
    }

    /// <summary>
    /// Sends a message to this client.
    /// </summary>
    public async Task<bool> SendMessageAsync(ServerMessage message)
    {
        if (ResponseStream == null || CancellationToken.IsCancellationRequested)
            return false;

        try
        {
            await ResponseStream.WriteAsync(message, CancellationToken);
            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Sends a command to this client.
    /// </summary>
    public Task<bool> SendCommandAsync(Command command)
    {
        var message = new ServerMessage
        {
            MessageId = Guid.NewGuid().ToString(),
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            Command = command
        };
        return SendMessageAsync(message);
    }

    /// <summary>
    /// Requests detailed system information from the client.
    /// </summary>
    public async Task<DetailedSystemInfo?> RequestDetailedSystemInfoAsync(TimeSpan timeout)
    {
        if (ResponseStream == null || CancellationToken.IsCancellationRequested || !IsOnline)
            return LastDetailedSystemInfo;

        var requestId = Guid.NewGuid().ToString();
        var tcs = new TaskCompletionSource<DetailedSystemInfo>();

        lock (_requestLock)
        {
            _pendingSystemInfoRequests[requestId] = tcs;
        }

        try
        {
            // Send command to request system info
            var command = new Command
            {
                CommandId = requestId,
                CommandType = "systeminfo",
                CommandText = "detailed",
                TimeoutSeconds = (int)timeout.TotalSeconds
            };

            var sent = await SendCommandAsync(command);
            if (!sent)
            {
                lock (_requestLock)
                {
                    _pendingSystemInfoRequests.Remove(requestId);
                }
                return LastDetailedSystemInfo;
            }

            // Wait for response with timeout
            using var cts = new CancellationTokenSource(timeout);
            cts.Token.Register(() => tcs.TrySetCanceled());

            return await tcs.Task;
        }
        catch (OperationCanceledException)
        {
            // Timeout - return cached data if available
            return LastDetailedSystemInfo;
        }
        finally
        {
            lock (_requestLock)
            {
                _pendingSystemInfoRequests.Remove(requestId);
            }
        }
    }

    /// <summary>
    /// Called when detailed system info is received from the client.
    /// </summary>
    public void OnDetailedSystemInfoReceived(string commandId, DetailedSystemInfo info)
    {
        // Cache the info
        LastDetailedSystemInfo = info;
        LastDetailedSystemInfoTime = DateTime.UtcNow;

        // Complete any pending request
        lock (_requestLock)
        {
            if (_pendingSystemInfoRequests.TryGetValue(commandId, out var tcs))
            {
                tcs.TrySetResult(info);
                _pendingSystemInfoRequests.Remove(commandId);
            }
        }
    }
}
