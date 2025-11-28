using Microsoft.EntityFrameworkCore;
using TorGames.Database.Entities;

namespace TorGames.Database;

/// <summary>
/// Entity Framework Core database context for TorGames.
/// </summary>
public class TorGamesDbContext : DbContext
{
    public TorGamesDbContext(DbContextOptions<TorGamesDbContext> options) : base(options)
    {
    }

    /// <summary>
    /// All registered clients.
    /// </summary>
    public DbSet<Client> Clients => Set<Client>();

    /// <summary>
    /// Client connection history.
    /// </summary>
    public DbSet<ClientConnection> ClientConnections => Set<ClientConnection>();

    /// <summary>
    /// Command audit log.
    /// </summary>
    public DbSet<CommandLog> CommandLogs => Set<CommandLog>();

    protected override void OnModelCreating(ModelBuilder modelBuilder)
    {
        base.OnModelCreating(modelBuilder);

        // Client configuration
        modelBuilder.Entity<Client>(entity =>
        {
            entity.HasKey(e => e.ClientId);

            entity.HasIndex(e => e.LastSeenAt);
            entity.HasIndex(e => e.MachineName);
            entity.HasIndex(e => e.Username);
            entity.HasIndex(e => e.CountryCode);
            entity.HasIndex(e => e.IsFlagged);
            entity.HasIndex(e => e.IsBlocked);
        });

        // ClientConnection configuration
        modelBuilder.Entity<ClientConnection>(entity =>
        {
            entity.HasKey(e => e.Id);

            entity.HasIndex(e => e.ClientId);
            entity.HasIndex(e => e.ConnectedAt);
            entity.HasIndex(e => e.IpAddress);

            entity.HasOne(e => e.Client)
                .WithMany(c => c.Connections)
                .HasForeignKey(e => e.ClientId)
                .OnDelete(DeleteBehavior.Cascade);
        });

        // CommandLog configuration
        modelBuilder.Entity<CommandLog>(entity =>
        {
            entity.HasKey(e => e.Id);

            entity.HasIndex(e => e.CommandId);
            entity.HasIndex(e => e.ClientId);
            entity.HasIndex(e => e.CommandType);
            entity.HasIndex(e => e.SentAt);

            entity.HasOne(e => e.Client)
                .WithMany(c => c.Commands)
                .HasForeignKey(e => e.ClientId)
                .OnDelete(DeleteBehavior.SetNull);
        });
    }
}
