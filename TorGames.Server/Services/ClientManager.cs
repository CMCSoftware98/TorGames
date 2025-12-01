using System.Collections.Concurrent;
using TorGames.Common.Protos;
using TorGames.Database.Services;
using TorGames.Server.Models;
using Command = TorGames.Common.Protos.Command;

namespace TorGames.Server.Services;

/// <summary>
/// Event args for client connection events.
/// </summary>
public class ClientEventArgs : EventArgs
{
    public required ConnectedClient Client { get; init; }
}

/// <summary>
/// Event args for command result events.
/// </summary>
public class CommandResultEventArgs : EventArgs
{
    public required string ConnectionKey { get; init; }
    public required CommandResult Result { get; init; }
}

/// <summary>
/// Event args for detailed system info events.
/// </summary>
public class DetailedSystemInfoEventArgs : EventArgs
{
    public required string ConnectionKey { get; init; }
    public required DetailedSystemInfo SystemInfo { get; init; }
}

/// <summary>
/// Manages all connected clients, providing thread-safe access and operations.
/// </summary>
public class ClientManager
{
    private readonly ConcurrentDictionary<string, ConnectedClient> _clients = new();
    private readonly ILogger<ClientManager> _logger;
    private readonly IServiceScopeFactory _scopeFactory;

    // Events for SignalR hub to subscribe to
    public event EventHandler<ClientEventArgs>? ClientConnected;
    public event EventHandler<ClientEventArgs>? ClientDisconnected;
    public event EventHandler<ClientEventArgs>? ClientHeartbeatReceived;
    public event EventHandler<CommandResultEventArgs>? CommandResultReceived;
    public event EventHandler<DetailedSystemInfoEventArgs>? DetailedSystemInfoReceived;

    public ClientManager(ILogger<ClientManager> logger, IServiceScopeFactory scopeFactory)
    {
        _logger = logger;
        _scopeFactory = scopeFactory;
    }

    /// <summary>
    /// Gets the count of currently connected clients.
    /// </summary>
    public int ConnectedCount => _clients.Count;

    /// <summary>
    /// Gets the count of online clients (received heartbeat within timeout, excluding installers).
    /// </summary>
    public int OnlineCount => _clients.Values.Count(c => c.IsOnline && !c.ShouldShowAsInstalling);

    /// <summary>
    /// Gets the count of clients currently installing/updating.
    /// </summary>
    public int InstallingCount => _clients.Values.Count(c => c.IsOnline && c.ShouldShowAsInstalling);

    /// <summary>
    /// Attempts to register a new client connection.
    /// If a client with the same connection key exists, it will be replaced (handles reconnection).
    /// </summary>
    public (bool Success, string? RejectionReason) TryRegisterClient(ConnectedClient client)
    {
        // Check if there's an existing client with the same key
        if (_clients.TryGetValue(client.ConnectionKey, out var existingClient))
        {
            // Replace the existing client (this handles reconnection scenarios)
            _logger.LogInformation(
                "Replacing existing client connection: {ConnectionKey} ({MachineName})",
                client.ConnectionKey,
                client.MachineName);

            // Remove the old client first
            _clients.TryRemove(client.ConnectionKey, out _);
            ClientDisconnected?.Invoke(this, new ClientEventArgs { Client = existingClient });
        }

        if (_clients.TryAdd(client.ConnectionKey, client))
        {
            _logger.LogInformation(
                "Client registered: {ConnectionKey} ({MachineName})",
                client.ConnectionKey,
                client.MachineName);

            // Raise event for SignalR hub
            ClientConnected?.Invoke(this, new ClientEventArgs { Client = client });

            return (true, null);
        }

        // This shouldn't happen after we removed the existing client, but handle it just in case
        _logger.LogWarning(
            "Client registration failed unexpectedly: {ConnectionKey}",
            client.ConnectionKey);
        return (false, "Failed to register client");
    }

    /// <summary>
    /// Removes a client from the manager.
    /// </summary>
    public bool RemoveClient(string connectionKey)
    {
        if (_clients.TryRemove(connectionKey, out var client))
        {
            _logger.LogInformation(
                "Client removed: {ConnectionKey} ({MachineName})",
                connectionKey,
                client.MachineName);

            // Raise event for SignalR hub
            ClientDisconnected?.Invoke(this, new ClientEventArgs { Client = client });

            return true;
        }
        return false;
    }

    /// <summary>
    /// Gets a client by connection key.
    /// </summary>
    public ConnectedClient? GetClient(string connectionKey)
    {
        _clients.TryGetValue(connectionKey, out var client);
        return client;
    }

    /// <summary>
    /// Gets a client by client ID (fingerprint) and type.
    /// </summary>
    public ConnectedClient? GetClient(string clientId, string clientType)
    {
        return GetClient($"{clientId}:{clientType.ToUpperInvariant()}");
    }

    /// <summary>
    /// Gets all connected clients.
    /// </summary>
    public IEnumerable<ConnectedClient> GetAllClients() => _clients.Values;

    /// <summary>
    /// Gets all clients of a specific type.
    /// </summary>
    public IEnumerable<ConnectedClient> GetClientsByType(string clientType)
    {
        return _clients.Values.Where(c =>
            c.ClientType.Equals(clientType, StringComparison.OrdinalIgnoreCase));
    }

    /// <summary>
    /// Gets all online clients (excluding those installing/updating).
    /// </summary>
    public IEnumerable<ConnectedClient> GetOnlineClients()
    {
        return _clients.Values.Where(c => c.IsOnline && !c.ShouldShowAsInstalling);
    }

    /// <summary>
    /// Gets all clients currently installing/updating.
    /// </summary>
    public IEnumerable<ConnectedClient> GetInstallingClients()
    {
        return _clients.Values.Where(c => c.IsOnline && c.ShouldShowAsInstalling);
    }

    /// <summary>
    /// Updates heartbeat for a client.
    /// </summary>
    public void UpdateHeartbeat(string connectionKey, Heartbeat heartbeat)
    {
        if (_clients.TryGetValue(connectionKey, out var client))
        {
            client.UpdateFromHeartbeat(heartbeat);

            // Raise event for SignalR hub
            ClientHeartbeatReceived?.Invoke(this, new ClientEventArgs { Client = client });
        }
    }

    /// <summary>
    /// Notifies that a command result was received from a client.
    /// </summary>
    public void NotifyCommandResult(string connectionKey, CommandResult result)
    {
        CommandResultReceived?.Invoke(this, new CommandResultEventArgs
        {
            ConnectionKey = connectionKey,
            Result = result
        });
    }

    /// <summary>
    /// Notifies that detailed system info was received from a client.
    /// </summary>
    public void NotifyDetailedSystemInfo(string connectionKey, DetailedSystemInfo systemInfo)
    {
        DetailedSystemInfoReceived?.Invoke(this, new DetailedSystemInfoEventArgs
        {
            ConnectionKey = connectionKey,
            SystemInfo = systemInfo
        });
    }

    /// <summary>
    /// Requests detailed system info from a client.
    /// </summary>
    public async Task<DetailedSystemInfo?> RequestDetailedSystemInfoAsync(string connectionKey, TimeSpan timeout)
    {
        if (_clients.TryGetValue(connectionKey, out var client))
        {
            return await client.RequestDetailedSystemInfoAsync(timeout);
        }
        return null;
    }

    /// <summary>
    /// Sends a command to a specific client.
    /// </summary>
    public async Task<bool> SendCommandAsync(string connectionKey, Command command)
    {
        if (_clients.TryGetValue(connectionKey, out var client))
        {
            return await client.SendCommandAsync(command);
        }
        return false;
    }

    /// <summary>
    /// Sends a command to all clients of a specific type.
    /// </summary>
    public async Task<int> BroadcastCommandAsync(string clientType, Command command)
    {
        var clients = GetClientsByType(clientType).ToList();
        var successCount = 0;

        foreach (var client in clients)
        {
            if (await client.SendCommandAsync(command))
                successCount++;
        }

        _logger.LogInformation(
            "Broadcast command {CommandType} to {ClientType}: {Success}/{Total}",
            command.CommandType,
            clientType,
            successCount,
            clients.Count);

        return successCount;
    }

    /// <summary>
    /// Sends a command to all connected clients.
    /// </summary>
    public async Task<int> BroadcastCommandToAllAsync(Command command)
    {
        var clients = _clients.Values.ToList();
        var successCount = 0;

        foreach (var client in clients)
        {
            if (await client.SendCommandAsync(command))
                successCount++;
        }

        _logger.LogInformation(
            "Broadcast command {CommandType} to all: {Success}/{Total}",
            command.CommandType,
            successCount,
            clients.Count);

        return successCount;
    }

    /// <summary>
    /// Removes clients that haven't sent a heartbeat within the timeout period.
    /// </summary>
    public int CleanupStaleClients(TimeSpan timeout)
    {
        var staleClients = _clients.Values
            .Where(c => (DateTime.UtcNow - c.LastHeartbeat) > timeout)
            .Select(c => c.ConnectionKey)
            .ToList();

        foreach (var key in staleClients)
        {
            RemoveClient(key);
        }

        if (staleClients.Count > 0)
        {
            _logger.LogInformation("Cleaned up {Count} stale clients", staleClients.Count);
        }

        return staleClients.Count;
    }

    /// <summary>
    /// Gracefully disconnects all clients. Sends a shutdown notification to each client.
    /// </summary>
    public async Task DisconnectAllClientsAsync()
    {
        var clients = _clients.Values.ToList();

        if (clients.Count == 0)
        {
            _logger.LogInformation("No clients to disconnect");
            return;
        }

        _logger.LogInformation("Gracefully disconnecting {Count} clients...", clients.Count);

        // Send shutdown notification to all clients in parallel
        var shutdownCommand = new Command
        {
            CommandId = Guid.NewGuid().ToString(),
            CommandType = "server_shutdown",
            CommandText = "Server is shutting down. Please reconnect later.",
            TimeoutSeconds = 0
        };

        var tasks = clients.Select(async client =>
        {
            try
            {
                await client.SendCommandAsync(shutdownCommand);
                _logger.LogDebug("Sent shutdown notification to {ConnectionKey}", client.ConnectionKey);
            }
            catch (Exception ex)
            {
                _logger.LogDebug(ex, "Error sending shutdown notification to {ConnectionKey}", client.ConnectionKey);
            }
        });

        // Wait for all notifications to be sent (with timeout)
        try
        {
            await Task.WhenAll(tasks).WaitAsync(TimeSpan.FromSeconds(5));
        }
        catch (TimeoutException)
        {
            _logger.LogWarning("Timeout waiting for shutdown notifications to complete");
        }

        // Clear all clients
        var connectionKeys = _clients.Keys.ToList();
        foreach (var key in connectionKeys)
        {
            RemoveClient(key);
        }

        _logger.LogInformation("All clients disconnected");
    }

    /// <summary>
    /// Checks if a client is in test mode by looking up the database.
    /// </summary>
    public async Task<bool> IsClientInTestModeAsync(string clientId)
    {
        try
        {
            using var scope = _scopeFactory.CreateScope();
            var repository = scope.ServiceProvider.GetRequiredService<ClientRepository>();
            return await repository.IsClientInTestModeAsync(clientId);
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Failed to check test mode for client {ClientId}", clientId);
            return false;
        }
    }

    /// <summary>
    /// Sends a command to all clients that are in test mode.
    /// </summary>
    public async Task<int> BroadcastCommandToTestClientsAsync(Command command)
    {
        var clients = _clients.Values.ToList();
        var successCount = 0;

        foreach (var client in clients)
        {
            // Check if this client is in test mode
            if (await IsClientInTestModeAsync(client.ClientId))
            {
                if (await client.SendCommandAsync(command))
                    successCount++;
            }
        }

        _logger.LogInformation(
            "Broadcast command {CommandType} to test clients: {Success} clients",
            command.CommandType,
            successCount);

        return successCount;
    }
}
