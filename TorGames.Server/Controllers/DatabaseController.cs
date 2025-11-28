using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using Microsoft.EntityFrameworkCore;
using TorGames.Database;
using TorGames.Database.Entities;
using TorGames.Database.Services;

namespace TorGames.Server.Controllers;

/// <summary>
/// Controller for database operations and management.
/// </summary>
[ApiController]
[Route("api/[controller]")]
[Authorize]
public class DatabaseController : ControllerBase
{
    private readonly TorGamesDbContext _context;
    private readonly ClientRepository _repository;
    private readonly ILogger<DatabaseController> _logger;
    private readonly IConfiguration _configuration;

    public DatabaseController(
        TorGamesDbContext context,
        ClientRepository repository,
        ILogger<DatabaseController> logger,
        IConfiguration configuration)
    {
        _context = context;
        _repository = repository;
        _logger = logger;
        _configuration = configuration;
    }

    /// <summary>
    /// Gets database statistics.
    /// </summary>
    [HttpGet("stats")]
    public async Task<ActionResult<DatabaseStats>> GetStats()
    {
        try
        {
            var stats = await _repository.GetStatsAsync();
            return Ok(stats);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting database stats");
            return StatusCode(500, "Failed to get database statistics");
        }
    }

    /// <summary>
    /// Gets all clients from the database.
    /// </summary>
    [HttpGet("clients")]
    public async Task<ActionResult<List<ClientDto>>> GetClients(
        [FromQuery] bool? flagged = null,
        [FromQuery] bool? blocked = null,
        [FromQuery] string? country = null,
        [FromQuery] int limit = 1000)
    {
        try
        {
            var clients = await _repository.GetClientsAsync(flagged, blocked, country, limit);
            var dtos = clients.Select(c => new ClientDto
            {
                ClientId = c.ClientId,
                ClientType = c.ClientType,
                MachineName = c.MachineName,
                Username = c.Username,
                OsVersion = c.OsVersion,
                OsArchitecture = c.OsArchitecture,
                CpuCount = c.CpuCount,
                TotalMemoryBytes = c.TotalMemoryBytes,
                MacAddress = c.MacAddress,
                LastIpAddress = c.LastIpAddress,
                CountryCode = c.CountryCode,
                IsAdmin = c.IsAdmin,
                IsUacEnabled = c.IsUacEnabled,
                ClientVersion = c.ClientVersion,
                Label = c.Label,
                FirstSeenAt = c.FirstSeenAt,
                LastSeenAt = c.LastSeenAt,
                TotalConnections = c.TotalConnections,
                IsFlagged = c.IsFlagged,
                IsBlocked = c.IsBlocked
            }).ToList();

            return Ok(dtos);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting clients");
            return StatusCode(500, "Failed to get clients");
        }
    }

    /// <summary>
    /// Gets a specific client with connection history.
    /// </summary>
    [HttpGet("clients/{clientId}")]
    public async Task<ActionResult<ClientDetailDto>> GetClient(string clientId)
    {
        try
        {
            var client = await _repository.GetClientWithHistoryAsync(clientId);
            if (client == null)
                return NotFound("Client not found");

            var dto = new ClientDetailDto
            {
                ClientId = client.ClientId,
                ClientType = client.ClientType,
                MachineName = client.MachineName,
                Username = client.Username,
                OsVersion = client.OsVersion,
                OsArchitecture = client.OsArchitecture,
                CpuCount = client.CpuCount,
                TotalMemoryBytes = client.TotalMemoryBytes,
                MacAddress = client.MacAddress,
                LastIpAddress = client.LastIpAddress,
                CountryCode = client.CountryCode,
                IsAdmin = client.IsAdmin,
                IsUacEnabled = client.IsUacEnabled,
                ClientVersion = client.ClientVersion,
                Label = client.Label,
                FirstSeenAt = client.FirstSeenAt,
                LastSeenAt = client.LastSeenAt,
                TotalConnections = client.TotalConnections,
                IsFlagged = client.IsFlagged,
                IsBlocked = client.IsBlocked,
                Connections = client.Connections.Select(c => new ConnectionDto
                {
                    Id = c.Id,
                    IpAddress = c.IpAddress,
                    CountryCode = c.CountryCode,
                    ClientVersion = c.ClientVersion,
                    ConnectedAt = c.ConnectedAt,
                    DisconnectedAt = c.DisconnectedAt,
                    DurationSeconds = c.DurationSeconds,
                    DisconnectReason = c.DisconnectReason
                }).ToList()
            };

            return Ok(dto);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting client {ClientId}", clientId);
            return StatusCode(500, "Failed to get client");
        }
    }

    /// <summary>
    /// Updates client flags (flagged, blocked, label).
    /// </summary>
    [HttpPatch("clients/{clientId}")]
    public async Task<ActionResult> UpdateClient(string clientId, [FromBody] UpdateClientRequest request)
    {
        try
        {
            var result = await _repository.UpdateClientFlagsAsync(
                clientId,
                request.IsFlagged,
                request.IsBlocked,
                request.Label);

            if (!result)
                return NotFound("Client not found");

            return Ok();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error updating client {ClientId}", clientId);
            return StatusCode(500, "Failed to update client");
        }
    }

    /// <summary>
    /// Deletes a client from the database.
    /// </summary>
    [HttpDelete("clients/{clientId}")]
    public async Task<ActionResult> DeleteClient(string clientId)
    {
        try
        {
            var result = await _repository.DeleteClientAsync(clientId);
            if (!result)
                return NotFound("Client not found");

            return Ok();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error deleting client {ClientId}", clientId);
            return StatusCode(500, "Failed to delete client");
        }
    }

    /// <summary>
    /// Gets command logs.
    /// </summary>
    [HttpGet("commands")]
    public async Task<ActionResult<List<CommandLogDto>>> GetCommands(
        [FromQuery] string? clientId = null,
        [FromQuery] string? commandType = null,
        [FromQuery] int limit = 100)
    {
        try
        {
            var logs = await _repository.GetCommandLogsAsync(clientId, commandType, limit);
            var dtos = logs.Select(l => new CommandLogDto
            {
                Id = l.Id,
                CommandId = l.CommandId,
                ClientId = l.ClientId,
                CommandType = l.CommandType,
                CommandText = l.CommandText,
                SentAt = l.SentAt,
                WasDelivered = l.WasDelivered,
                ResultReceivedAt = l.ResultReceivedAt,
                Success = l.Success,
                ResultOutput = l.ResultOutput,
                ErrorMessage = l.ErrorMessage,
                IsBroadcast = l.IsBroadcast,
                InitiatorIp = l.InitiatorIp
            }).ToList();

            return Ok(dtos);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting command logs");
            return StatusCode(500, "Failed to get command logs");
        }
    }

    /// <summary>
    /// Downloads the database file as a backup.
    /// </summary>
    [HttpGet("backup")]
    public async Task<ActionResult> DownloadBackup()
    {
        try
        {
            var dataFolder = Path.Combine(AppContext.BaseDirectory, "Data");
            var databasePath = Path.Combine(dataFolder, "torgames.db");

            if (!System.IO.File.Exists(databasePath))
                return NotFound("Database file not found");

            // Create a temporary copy to avoid file locking issues
            var backupPath = Path.Combine(dataFolder, $"torgames_backup_{DateTime.UtcNow:yyyyMMdd_HHmmss}.db");

            // Checkpoint the WAL file first to ensure all data is in the main database file
            await _context.Database.ExecuteSqlRawAsync("PRAGMA wal_checkpoint(TRUNCATE);");

            // Copy the database file
            System.IO.File.Copy(databasePath, backupPath, overwrite: true);

            var fileBytes = await System.IO.File.ReadAllBytesAsync(backupPath);

            // Clean up the temporary backup file
            System.IO.File.Delete(backupPath);

            var fileName = $"torgames_backup_{DateTime.UtcNow:yyyyMMdd_HHmmss}.db";

            _logger.LogInformation("Database backup downloaded");

            return File(fileBytes, "application/octet-stream", fileName);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error creating database backup");
            return StatusCode(500, "Failed to create database backup");
        }
    }

    /// <summary>
    /// Gets database file information.
    /// </summary>
    [HttpGet("info")]
    public ActionResult<DatabaseInfoDto> GetDatabaseInfo()
    {
        try
        {
            var dataFolder = Path.Combine(AppContext.BaseDirectory, "Data");
            var databasePath = Path.Combine(dataFolder, "torgames.db");

            if (!System.IO.File.Exists(databasePath))
                return NotFound("Database file not found");

            var fileInfo = new FileInfo(databasePath);

            return Ok(new DatabaseInfoDto
            {
                FilePath = databasePath,
                FileSizeBytes = fileInfo.Length,
                CreatedAt = fileInfo.CreationTimeUtc,
                ModifiedAt = fileInfo.LastWriteTimeUtc
            });
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting database info");
            return StatusCode(500, "Failed to get database info");
        }
    }
}

#region DTOs

public class ClientDto
{
    public string ClientId { get; set; } = string.Empty;
    public string ClientType { get; set; } = string.Empty;
    public string MachineName { get; set; } = string.Empty;
    public string Username { get; set; } = string.Empty;
    public string OsVersion { get; set; } = string.Empty;
    public string OsArchitecture { get; set; } = string.Empty;
    public int CpuCount { get; set; }
    public long TotalMemoryBytes { get; set; }
    public string MacAddress { get; set; } = string.Empty;
    public string LastIpAddress { get; set; } = string.Empty;
    public string CountryCode { get; set; } = string.Empty;
    public bool IsAdmin { get; set; }
    public bool IsUacEnabled { get; set; }
    public string ClientVersion { get; set; } = string.Empty;
    public string? Label { get; set; }
    public DateTime FirstSeenAt { get; set; }
    public DateTime LastSeenAt { get; set; }
    public int TotalConnections { get; set; }
    public bool IsFlagged { get; set; }
    public bool IsBlocked { get; set; }
}

public class ClientDetailDto : ClientDto
{
    public List<ConnectionDto> Connections { get; set; } = new();
}

public class ConnectionDto
{
    public int Id { get; set; }
    public string IpAddress { get; set; } = string.Empty;
    public string CountryCode { get; set; } = string.Empty;
    public string ClientVersion { get; set; } = string.Empty;
    public DateTime ConnectedAt { get; set; }
    public DateTime? DisconnectedAt { get; set; }
    public long? DurationSeconds { get; set; }
    public string? DisconnectReason { get; set; }
}

public class CommandLogDto
{
    public int Id { get; set; }
    public string CommandId { get; set; } = string.Empty;
    public string? ClientId { get; set; }
    public string CommandType { get; set; } = string.Empty;
    public string CommandText { get; set; } = string.Empty;
    public DateTime SentAt { get; set; }
    public bool WasDelivered { get; set; }
    public DateTime? ResultReceivedAt { get; set; }
    public bool? Success { get; set; }
    public string? ResultOutput { get; set; }
    public string? ErrorMessage { get; set; }
    public bool IsBroadcast { get; set; }
    public string? InitiatorIp { get; set; }
}

public class UpdateClientRequest
{
    public bool? IsFlagged { get; set; }
    public bool? IsBlocked { get; set; }
    public string? Label { get; set; }
}

public class DatabaseInfoDto
{
    public string FilePath { get; set; } = string.Empty;
    public long FileSizeBytes { get; set; }
    public DateTime CreatedAt { get; set; }
    public DateTime ModifiedAt { get; set; }
}

#endregion
