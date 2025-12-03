using TorGames.Database;
using TorGames.Database.Services;
using TorGames.Server.Models;

namespace TorGames.Server.Services;

/// <summary>
/// Background service that syncs client events to the database.
/// </summary>
public class DatabaseSyncService : BackgroundService
{
    private readonly IServiceProvider _serviceProvider;
    private readonly ClientManager _clientManager;
    private readonly ILogger<DatabaseSyncService> _logger;

    public DatabaseSyncService(
        IServiceProvider serviceProvider,
        ClientManager clientManager,
        ILogger<DatabaseSyncService> logger)
    {
        _serviceProvider = serviceProvider;
        _clientManager = clientManager;
        _logger = logger;
    }

    protected override Task ExecuteAsync(CancellationToken stoppingToken)
    {
        // Subscribe to client events
        _clientManager.ClientConnected += OnClientConnected;
        _clientManager.ClientDisconnected += OnClientDisconnected;
        _clientManager.CommandResultReceived += OnCommandResultReceived;

        _logger.LogInformation("Database sync service started");

        return Task.CompletedTask;
    }

    public override Task StopAsync(CancellationToken cancellationToken)
    {
        // Unsubscribe from events
        _clientManager.ClientConnected -= OnClientConnected;
        _clientManager.ClientDisconnected -= OnClientDisconnected;
        _clientManager.CommandResultReceived -= OnCommandResultReceived;

        _logger.LogInformation("Database sync service stopped");

        return base.StopAsync(cancellationToken);
    }

    private async void OnClientConnected(object? sender, ClientEventArgs e)
    {
        try
        {
            var client = e.Client;

            // Skip INSTALLER type clients - they are transient and should not be persisted
            // Once the installer completes, the installed client will register with its own type
            if (client.ClientType.Equals("INSTALLER", StringComparison.OrdinalIgnoreCase))
            {
                _logger.LogDebug("Skipping database persistence for INSTALLER client {ClientId}", client.ClientId);
                return;
            }

            using var scope = _serviceProvider.CreateScope();
            var repository = scope.ServiceProvider.GetRequiredService<ClientRepository>();

            // Update or create client record
            await repository.UpdateClientFromConnectionAsync(
                client.ClientId,
                client.ClientType,
                client.MachineName,
                client.Username,
                client.OsVersion,
                client.OsArchitecture,
                client.CpuCount,
                client.TotalMemoryBytes,
                client.MacAddress,
                client.IpAddress,
                client.CountryCode,
                client.IsAdmin,
                client.IsUacEnabled,
                client.ClientVersion);

            // Record the connection
            await repository.RecordConnectionAsync(
                client.ClientId,
                client.IpAddress,
                client.CountryCode,
                client.ClientVersion);

            _logger.LogDebug("Client {ClientId} connection persisted to database", client.ClientId);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error persisting client connection to database");
        }
    }

    private async void OnClientDisconnected(object? sender, ClientEventArgs e)
    {
        try
        {
            // Skip INSTALLER type clients - they are not persisted
            if (e.Client.ClientType.Equals("INSTALLER", StringComparison.OrdinalIgnoreCase))
            {
                return;
            }

            using var scope = _serviceProvider.CreateScope();
            var repository = scope.ServiceProvider.GetRequiredService<ClientRepository>();

            await repository.RecordDisconnectionAsync(e.Client.ClientId, "disconnected");

            _logger.LogDebug("Client {ClientId} disconnection recorded in database", e.Client.ClientId);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error recording client disconnection in database");
        }
    }

    private async void OnCommandResultReceived(object? sender, CommandResultEventArgs e)
    {
        try
        {
            using var scope = _serviceProvider.CreateScope();
            var repository = scope.ServiceProvider.GetRequiredService<ClientRepository>();

            await repository.UpdateCommandResultAsync(
                e.Result.CommandId,
                e.Result.Success,
                string.IsNullOrEmpty(e.Result.Stdout) ? e.Result.Stderr : e.Result.Stdout,
                e.Result.ErrorMessage);

            _logger.LogDebug("Command result {CommandId} recorded in database", e.Result.CommandId);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error recording command result in database");
        }
    }
}
