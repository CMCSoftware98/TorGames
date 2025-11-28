using Microsoft.EntityFrameworkCore;
using Microsoft.Extensions.DependencyInjection;

namespace TorGames.Database.Extensions;

/// <summary>
/// Extension methods for configuring database services.
/// </summary>
public static class ServiceCollectionExtensions
{
    /// <summary>
    /// Adds TorGames database services with SQLite.
    /// </summary>
    /// <param name="services">The service collection.</param>
    /// <param name="connectionString">SQLite connection string (e.g., "Data Source=torgames.db").</param>
    /// <returns>The service collection for chaining.</returns>
    public static IServiceCollection AddTorGamesDatabase(this IServiceCollection services, string connectionString)
    {
        services.AddDbContext<TorGamesDbContext>(options =>
        {
            options.UseSqlite(connectionString);
        });

        return services;
    }

    /// <summary>
    /// Adds TorGames database services with SQLite using default path.
    /// </summary>
    /// <param name="services">The service collection.</param>
    /// <param name="databasePath">Path to the SQLite database file.</param>
    /// <returns>The service collection for chaining.</returns>
    public static IServiceCollection AddTorGamesDatabase(this IServiceCollection services)
    {
        var databasePath = Path.Combine(AppContext.BaseDirectory, "Data", "torgames.db");
        var connectionString = $"Data Source={databasePath}";

        return services.AddTorGamesDatabase(connectionString);
    }
}
