using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace TorGames.Database.Entities;

/// <summary>
/// Represents a single connection session from a client.
/// </summary>
public class ClientConnection
{
    [Key]
    public int Id { get; set; }

    /// <summary>
    /// Foreign key to the Client.
    /// </summary>
    [MaxLength(64)]
    public string ClientId { get; set; } = string.Empty;

    /// <summary>
    /// Navigation property to the Client.
    /// </summary>
    [ForeignKey(nameof(ClientId))]
    public Client? Client { get; set; }

    /// <summary>
    /// IP address used for this connection.
    /// </summary>
    [MaxLength(45)]
    public string IpAddress { get; set; } = string.Empty;

    /// <summary>
    /// Country code for this connection (may differ from previous).
    /// </summary>
    [MaxLength(8)]
    public string CountryCode { get; set; } = string.Empty;

    /// <summary>
    /// Client version during this connection.
    /// </summary>
    [MaxLength(32)]
    public string ClientVersion { get; set; } = string.Empty;

    /// <summary>
    /// When the connection was established.
    /// </summary>
    public DateTime ConnectedAt { get; set; } = DateTime.UtcNow;

    /// <summary>
    /// When the connection was closed (null if still connected).
    /// </summary>
    public DateTime? DisconnectedAt { get; set; }

    /// <summary>
    /// Duration of the connection in seconds.
    /// </summary>
    public long? DurationSeconds { get; set; }

    /// <summary>
    /// Reason for disconnection (timeout, manual, server_shutdown, etc.).
    /// </summary>
    [MaxLength(64)]
    public string? DisconnectReason { get; set; }
}
