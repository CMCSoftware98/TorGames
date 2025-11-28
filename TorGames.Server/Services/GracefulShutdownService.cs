using Microsoft.AspNetCore.SignalR;
using TorGames.Server.Hubs;

namespace TorGames.Server.Services;

/// <summary>
/// Handles graceful shutdown of the server, ensuring all connections are properly closed.
/// </summary>
public class GracefulShutdownService : IHostedService
{
    private readonly IHostApplicationLifetime _lifetime;
    private readonly ClientManager _clientManager;
    private readonly IHubContext<ClientHub> _hubContext;
    private readonly ILogger<GracefulShutdownService> _logger;

    public GracefulShutdownService(
        IHostApplicationLifetime lifetime,
        ClientManager clientManager,
        IHubContext<ClientHub> hubContext,
        ILogger<GracefulShutdownService> logger)
    {
        _lifetime = lifetime;
        _clientManager = clientManager;
        _hubContext = hubContext;
        _logger = logger;
    }

    public Task StartAsync(CancellationToken cancellationToken)
    {
        // Register shutdown handler
        _lifetime.ApplicationStopping.Register(OnShutdown);
        return Task.CompletedTask;
    }

    public Task StopAsync(CancellationToken cancellationToken)
    {
        return Task.CompletedTask;
    }

    private async void OnShutdown()
    {
        _logger.LogInformation("========================================");
        _logger.LogInformation("  Server shutdown initiated...");
        _logger.LogInformation("========================================");

        try
        {
            // Step 1: Notify SignalR clients (web app) about shutdown
            _logger.LogInformation("Notifying SignalR clients about shutdown...");
            await NotifySignalRClientsAsync();

            // Step 2: Disconnect all gRPC clients
            _logger.LogInformation("Disconnecting gRPC clients...");
            await _clientManager.DisconnectAllClientsAsync();

            // Small delay to ensure messages are sent
            await Task.Delay(500);

            _logger.LogInformation("========================================");
            _logger.LogInformation("  Graceful shutdown complete");
            _logger.LogInformation("========================================");
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during graceful shutdown");
        }
    }

    private async Task NotifySignalRClientsAsync()
    {
        try
        {
            // Send shutdown notification to all connected SignalR clients
            await _hubContext.Clients.All.SendAsync("ServerShutdown", new
            {
                Message = "Server is shutting down. Please reconnect later.",
                Timestamp = DateTime.UtcNow
            });

            _logger.LogInformation("SignalR shutdown notification sent");
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Error notifying SignalR clients about shutdown");
        }
    }
}
