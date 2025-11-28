using System.ComponentModel.DataAnnotations;

namespace TorGames.Database.Entities;

/// <summary>
/// Represents a persistent client record in the database.
/// The ClientId (hardware fingerprint) is the primary key.
/// </summary>
public class Client
{
    /// <summary>
    /// Hardware fingerprint - unique identifier for the client machine.
    /// </summary>
    [Key]
    [MaxLength(64)]
    public string ClientId { get; set; } = string.Empty;

    /// <summary>
    /// Client type (e.g., "WORKER", "ADMIN").
    /// </summary>
    [MaxLength(32)]
    public string ClientType { get; set; } = string.Empty;

    /// <summary>
    /// Machine/computer name.
    /// </summary>
    [MaxLength(256)]
    public string MachineName { get; set; } = string.Empty;

    /// <summary>
    /// Username of the logged-in user.
    /// </summary>
    [MaxLength(256)]
    public string Username { get; set; } = string.Empty;

    /// <summary>
    /// Operating system version string.
    /// </summary>
    [MaxLength(256)]
    public string OsVersion { get; set; } = string.Empty;

    /// <summary>
    /// OS architecture (x64, x86, ARM64).
    /// </summary>
    [MaxLength(32)]
    public string OsArchitecture { get; set; } = string.Empty;

    /// <summary>
    /// Number of CPU cores.
    /// </summary>
    public int CpuCount { get; set; }

    /// <summary>
    /// Total RAM in bytes.
    /// </summary>
    public long TotalMemoryBytes { get; set; }

    /// <summary>
    /// MAC address of the primary network adapter.
    /// </summary>
    [MaxLength(64)]
    public string MacAddress { get; set; } = string.Empty;

    /// <summary>
    /// Last known IP address.
    /// </summary>
    [MaxLength(45)]
    public string LastIpAddress { get; set; } = string.Empty;

    /// <summary>
    /// Country code based on IP geolocation.
    /// </summary>
    [MaxLength(8)]
    public string CountryCode { get; set; } = string.Empty;

    /// <summary>
    /// Whether the client has admin privileges.
    /// </summary>
    public bool IsAdmin { get; set; }

    /// <summary>
    /// Whether UAC is enabled on the client.
    /// </summary>
    public bool IsUacEnabled { get; set; } = true;

    /// <summary>
    /// Current client software version.
    /// </summary>
    [MaxLength(32)]
    public string ClientVersion { get; set; } = string.Empty;

    /// <summary>
    /// Custom label/note for this client.
    /// </summary>
    [MaxLength(512)]
    public string? Label { get; set; }

    /// <summary>
    /// When this client was first seen.
    /// </summary>
    public DateTime FirstSeenAt { get; set; } = DateTime.UtcNow;

    /// <summary>
    /// When this client was last seen online.
    /// </summary>
    public DateTime LastSeenAt { get; set; } = DateTime.UtcNow;

    /// <summary>
    /// Total number of connections from this client.
    /// </summary>
    public int TotalConnections { get; set; } = 1;

    /// <summary>
    /// Whether this client is flagged/starred for attention.
    /// </summary>
    public bool IsFlagged { get; set; }

    /// <summary>
    /// Whether this client is blocked from connecting.
    /// </summary>
    public bool IsBlocked { get; set; }

    /// <summary>
    /// Connection history for this client.
    /// </summary>
    public ICollection<ClientConnection> Connections { get; set; } = new List<ClientConnection>();

    /// <summary>
    /// Command history for this client.
    /// </summary>
    public ICollection<CommandLog> Commands { get; set; } = new List<CommandLog>();
}
