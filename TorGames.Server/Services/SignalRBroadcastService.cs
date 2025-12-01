using Microsoft.AspNetCore.SignalR;
using Microsoft.EntityFrameworkCore;
using TorGames.Database;
using TorGames.Server.Hubs;
using TorGames.Server.Models;

namespace TorGames.Server.Services;

/// <summary>
/// Background service that subscribes to ClientManager events and broadcasts them via SignalR.
/// </summary>
public class SignalRBroadcastService : IHostedService
{
    private readonly ClientManager _clientManager;
    private readonly IHubContext<ClientHub> _hubContext;
    private readonly IServiceProvider _serviceProvider;
    private readonly ILogger<SignalRBroadcastService> _logger;

    public SignalRBroadcastService(
        ClientManager clientManager,
        IHubContext<ClientHub> hubContext,
        IServiceProvider serviceProvider,
        ILogger<SignalRBroadcastService> logger)
    {
        _clientManager = clientManager;
        _hubContext = hubContext;
        _serviceProvider = serviceProvider;
        _logger = logger;
    }

    public Task StartAsync(CancellationToken cancellationToken)
    {
        _clientManager.ClientConnected += OnClientConnected;
        _clientManager.ClientDisconnected += OnClientDisconnected;
        _clientManager.ClientHeartbeatReceived += OnClientHeartbeat;
        _clientManager.CommandResultReceived += OnCommandResult;

        _logger.LogInformation("SignalR broadcast service started");
        return Task.CompletedTask;
    }

    public Task StopAsync(CancellationToken cancellationToken)
    {
        _clientManager.ClientConnected -= OnClientConnected;
        _clientManager.ClientDisconnected -= OnClientDisconnected;
        _clientManager.ClientHeartbeatReceived -= OnClientHeartbeat;
        _clientManager.CommandResultReceived -= OnCommandResult;

        _logger.LogInformation("SignalR broadcast service stopped");
        return Task.CompletedTask;
    }

    private async void OnClientConnected(object? sender, ClientEventArgs e)
    {
        try
        {
            // Get database info for the client
            using var scope = _serviceProvider.CreateScope();
            var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();
            var dbClient = await dbContext.Clients.AsNoTracking()
                .FirstOrDefaultAsync(c => c.ClientId == e.Client.ClientId);

            var dto = dbClient != null
                ? ClientDto.FromDatabaseClient(dbClient, e.Client)
                : ClientDto.FromConnectedClient(e.Client);

            await _hubContext.Clients.All.SendAsync("ClientConnected", dto);
            _logger.LogDebug("Broadcasted ClientConnected: {ConnectionKey}", e.Client.ConnectionKey);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error broadcasting ClientConnected");
        }
    }

    private async void OnClientDisconnected(object? sender, ClientEventArgs e)
    {
        try
        {
            // Get database info for the client (now offline)
            using var scope = _serviceProvider.CreateScope();
            var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();
            var dbClient = await dbContext.Clients.AsNoTracking()
                .FirstOrDefaultAsync(c => c.ClientId == e.Client.ClientId);

            var dto = dbClient != null
                ? ClientDto.FromDatabaseClient(dbClient, null) // null = offline
                : ClientDto.FromConnectedClient(e.Client);

            dto.IsOnline = false; // Ensure offline status
            await _hubContext.Clients.All.SendAsync("ClientDisconnected", dto);
            _logger.LogDebug("Broadcasted ClientDisconnected: {ConnectionKey}", e.Client.ConnectionKey);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error broadcasting ClientDisconnected");
        }
    }

    private async void OnClientHeartbeat(object? sender, ClientEventArgs e)
    {
        try
        {
            // Get database info for the client
            using var scope = _serviceProvider.CreateScope();
            var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();
            var dbClient = await dbContext.Clients.AsNoTracking()
                .FirstOrDefaultAsync(c => c.ClientId == e.Client.ClientId);

            var dto = dbClient != null
                ? ClientDto.FromDatabaseClient(dbClient, e.Client)
                : ClientDto.FromConnectedClient(e.Client);

            await _hubContext.Clients.All.SendAsync("ClientHeartbeat", dto);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error broadcasting ClientHeartbeat");
        }
    }

    private async void OnCommandResult(object? sender, CommandResultEventArgs e)
    {
        try
        {
            // Reset client activity status back to Idling
            var client = _clientManager.GetClient(e.ConnectionKey);
            if (client != null)
            {
                client.SetActivityStatus("Idling");
            }

            var dto = new CommandResultDto
            {
                ConnectionKey = e.ConnectionKey,
                CommandId = e.Result.CommandId,
                Success = e.Result.Success,
                ExitCode = e.Result.ExitCode,
                Stdout = e.Result.Stdout,
                Stderr = e.Result.Stderr,
                ErrorMessage = e.Result.ErrorMessage
            };
            await _hubContext.Clients.All.SendAsync("CommandResult", dto);
            _logger.LogDebug("Broadcasted CommandResult: {CommandId}", e.Result.CommandId);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error broadcasting CommandResult");
        }
    }
}
