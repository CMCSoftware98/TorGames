using System.ComponentModel.DataAnnotations;
using System.ComponentModel.DataAnnotations.Schema;

namespace TorGames.Database.Entities;

/// <summary>
/// Represents a command sent to a client for audit purposes.
/// </summary>
public class CommandLog
{
    [Key]
    public int Id { get; set; }

    /// <summary>
    /// Unique command identifier.
    /// </summary>
    [MaxLength(64)]
    public string CommandId { get; set; } = string.Empty;

    /// <summary>
    /// Foreign key to the Client (null if broadcast to all).
    /// </summary>
    [MaxLength(64)]
    public string? ClientId { get; set; }

    /// <summary>
    /// Navigation property to the Client.
    /// </summary>
    [ForeignKey(nameof(ClientId))]
    public Client? Client { get; set; }

    /// <summary>
    /// Type of command (e.g., "shell", "restart", "update", "uninstall").
    /// </summary>
    [MaxLength(64)]
    public string CommandType { get; set; } = string.Empty;

    /// <summary>
    /// The command text/parameters.
    /// </summary>
    public string CommandText { get; set; } = string.Empty;

    /// <summary>
    /// When the command was sent.
    /// </summary>
    public DateTime SentAt { get; set; } = DateTime.UtcNow;

    /// <summary>
    /// Whether the command was successfully delivered.
    /// </summary>
    public bool WasDelivered { get; set; }

    /// <summary>
    /// When the result was received (null if no result yet).
    /// </summary>
    public DateTime? ResultReceivedAt { get; set; }

    /// <summary>
    /// Whether the command executed successfully.
    /// </summary>
    public bool? Success { get; set; }

    /// <summary>
    /// Result output from the command (may be truncated for storage).
    /// </summary>
    public string? ResultOutput { get; set; }

    /// <summary>
    /// Error message if the command failed.
    /// </summary>
    [MaxLength(1024)]
    public string? ErrorMessage { get; set; }

    /// <summary>
    /// Whether this was a broadcast command to multiple clients.
    /// </summary>
    public bool IsBroadcast { get; set; }

    /// <summary>
    /// IP address of the admin who initiated this command.
    /// </summary>
    [MaxLength(45)]
    public string? InitiatorIp { get; set; }
}
