namespace TorGames.Server.Services;

/// <summary>
/// Background service that periodically cleans up stale client connections.
/// </summary>
public class ClientCleanupService : BackgroundService
{
    private readonly ILogger<ClientCleanupService> _logger;
    private readonly ClientManager _clientManager;
    private readonly TimeSpan _cleanupInterval = TimeSpan.FromSeconds(10);
    private readonly TimeSpan _clientTimeout = TimeSpan.FromSeconds(30);

    public ClientCleanupService(ILogger<ClientCleanupService> logger, ClientManager clientManager)
    {
        _logger = logger;
        _clientManager = clientManager;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _logger.LogInformation("Client cleanup service started");

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                await Task.Delay(_cleanupInterval, stoppingToken);

                var removedCount = _clientManager.CleanupStaleClients(_clientTimeout);

                if (removedCount > 0)
                {
                    _logger.LogInformation(
                        "Cleanup complete. Removed: {Removed}, Online: {Online}, Total: {Total}",
                        removedCount,
                        _clientManager.OnlineCount,
                        _clientManager.ConnectedCount);
                }
            }
            catch (OperationCanceledException)
            {
                // Expected during shutdown
            }
            catch (Exception ex)
            {
                _logger.LogError(ex, "Error during client cleanup");
            }
        }

        _logger.LogInformation("Client cleanup service stopped");
    }
}
