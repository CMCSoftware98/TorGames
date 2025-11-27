using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.SignalR;
using TorGames.Common.Protos;
using TorGames.Server.Models;
using TorGames.Server.Services;

namespace TorGames.Server.Hubs;

/// <summary>
/// SignalR hub for web app communication.
/// Handles real-time updates about client connections and command execution.
/// </summary>
[Authorize]
public class ClientHub : Hub
{
    private readonly ClientManager _clientManager;
    private readonly ILogger<ClientHub> _logger;

    public ClientHub(ClientManager clientManager, ILogger<ClientHub> logger)
    {
        _clientManager = clientManager;
        _logger = logger;
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
    /// Gets all currently connected clients.
    /// </summary>
    public IEnumerable<ClientDto> GetAllClients()
    {
        _logger.LogDebug("GetAllClients requested by {ConnectionId}", Context.ConnectionId);
        return _clientManager.GetAllClients().Select(ClientDto.FromConnectedClient);
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

        return await _clientManager.SendCommandAsync(request.ConnectionKey, command);
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
