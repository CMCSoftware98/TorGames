using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;

namespace TorGames.Common;

public class SimpleBackgroundService(ILogger<SimpleBackgroundService> logger) : BackgroundService
{
    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        logger.LogInformation("SimpleBackgroundService started");

        while (!stoppingToken.IsCancellationRequested)
        {
            logger.LogInformation("SimpleBackgroundService running at: {Time}", DateTimeOffset.Now);
            await Task.Delay(1000, stoppingToken);
        }

        logger.LogInformation("SimpleBackgroundService stopped");
    }
}
