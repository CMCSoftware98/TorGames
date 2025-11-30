using Microsoft.EntityFrameworkCore;
using TorGames.Database.Entities;

namespace TorGames.Database.Services;

/// <summary>
/// Repository for client database operations.
/// </summary>
public class ClientRepository
{
    private readonly TorGamesDbContext _context;

    public ClientRepository(TorGamesDbContext context)
    {
        _context = context;
    }

    /// <summary>
    /// Gets or creates a client record.
    /// </summary>
    public async Task<Client> GetOrCreateClientAsync(string clientId, string clientType)
    {
        var client = await _context.Clients.FindAsync(clientId);

        if (client == null)
        {
            client = new Client
            {
                ClientId = clientId,
                ClientType = clientType,
                FirstSeenAt = DateTime.UtcNow,
                LastSeenAt = DateTime.UtcNow,
                TotalConnections = 0
            };
            _context.Clients.Add(client);
        }

        return client;
    }

    /// <summary>
    /// Updates client information from a connection.
    /// </summary>
    public async Task UpdateClientFromConnectionAsync(
        string clientId,
        string clientType,
        string machineName,
        string username,
        string osVersion,
        string osArchitecture,
        int cpuCount,
        long totalMemoryBytes,
        string macAddress,
        string ipAddress,
        string countryCode,
        bool isAdmin,
        bool isUacEnabled,
        string clientVersion)
    {
        var client = await GetOrCreateClientAsync(clientId, clientType);

        client.MachineName = machineName;
        client.Username = username;
        client.OsVersion = osVersion;
        client.OsArchitecture = osArchitecture;
        client.CpuCount = cpuCount;
        client.TotalMemoryBytes = totalMemoryBytes;
        client.MacAddress = macAddress;
        client.LastIpAddress = ipAddress;
        client.CountryCode = countryCode;
        client.IsAdmin = isAdmin;
        client.IsUacEnabled = isUacEnabled;
        client.ClientVersion = clientVersion;
        client.LastSeenAt = DateTime.UtcNow;
        client.TotalConnections++;

        await _context.SaveChangesAsync();
    }

    /// <summary>
    /// Records a new connection for a client.
    /// </summary>
    public async Task<ClientConnection> RecordConnectionAsync(
        string clientId,
        string ipAddress,
        string countryCode,
        string clientVersion)
    {
        var connection = new ClientConnection
        {
            ClientId = clientId,
            IpAddress = ipAddress,
            CountryCode = countryCode,
            ClientVersion = clientVersion,
            ConnectedAt = DateTime.UtcNow
        };

        _context.ClientConnections.Add(connection);
        await _context.SaveChangesAsync();

        return connection;
    }

    /// <summary>
    /// Records a disconnection for a client.
    /// </summary>
    public async Task RecordDisconnectionAsync(string clientId, string? reason = null)
    {
        var connection = await _context.ClientConnections
            .Where(c => c.ClientId == clientId && c.DisconnectedAt == null)
            .OrderByDescending(c => c.ConnectedAt)
            .FirstOrDefaultAsync();

        if (connection != null)
        {
            connection.DisconnectedAt = DateTime.UtcNow;
            connection.DurationSeconds = (long)(connection.DisconnectedAt.Value - connection.ConnectedAt).TotalSeconds;
            connection.DisconnectReason = reason ?? "unknown";
            await _context.SaveChangesAsync();
        }
    }

    /// <summary>
    /// Logs a command sent to a client.
    /// </summary>
    public async Task<CommandLog> LogCommandAsync(
        string commandId,
        string? clientId,
        string commandType,
        string commandText,
        bool wasDelivered,
        bool isBroadcast = false,
        string? initiatorIp = null)
    {
        var log = new CommandLog
        {
            CommandId = commandId,
            ClientId = clientId,
            CommandType = commandType,
            CommandText = commandText,
            SentAt = DateTime.UtcNow,
            WasDelivered = wasDelivered,
            IsBroadcast = isBroadcast,
            InitiatorIp = initiatorIp
        };

        _context.CommandLogs.Add(log);
        await _context.SaveChangesAsync();

        return log;
    }

    /// <summary>
    /// Updates a command log with the result.
    /// </summary>
    public async Task UpdateCommandResultAsync(
        string commandId,
        bool success,
        string? output = null,
        string? errorMessage = null)
    {
        var log = await _context.CommandLogs
            .FirstOrDefaultAsync(c => c.CommandId == commandId);

        if (log != null)
        {
            log.ResultReceivedAt = DateTime.UtcNow;
            log.Success = success;
            log.ResultOutput = output?.Length > 10000 ? output[..10000] + "...(truncated)" : output;
            log.ErrorMessage = errorMessage;
            await _context.SaveChangesAsync();
        }
    }

    /// <summary>
    /// Gets all clients with optional filtering.
    /// </summary>
    public async Task<List<Client>> GetClientsAsync(
        bool? isFlagged = null,
        bool? isBlocked = null,
        string? countryCode = null,
        int? limit = null)
    {
        var query = _context.Clients.AsQueryable();

        if (isFlagged.HasValue)
            query = query.Where(c => c.IsFlagged == isFlagged.Value);

        if (isBlocked.HasValue)
            query = query.Where(c => c.IsBlocked == isBlocked.Value);

        if (!string.IsNullOrEmpty(countryCode))
            query = query.Where(c => c.CountryCode == countryCode);

        query = query.OrderByDescending(c => c.LastSeenAt);

        if (limit.HasValue)
            query = query.Take(limit.Value);

        return await query.ToListAsync();
    }

    /// <summary>
    /// Gets a client by ID with connection history.
    /// </summary>
    public async Task<Client?> GetClientWithHistoryAsync(string clientId, int connectionLimit = 50)
    {
        return await _context.Clients
            .Include(c => c.Connections.OrderByDescending(cc => cc.ConnectedAt).Take(connectionLimit))
            .FirstOrDefaultAsync(c => c.ClientId == clientId);
    }

    /// <summary>
    /// Gets recent command logs.
    /// </summary>
    public async Task<List<CommandLog>> GetCommandLogsAsync(
        string? clientId = null,
        string? commandType = null,
        int limit = 100)
    {
        var query = _context.CommandLogs.AsQueryable();

        if (!string.IsNullOrEmpty(clientId))
            query = query.Where(c => c.ClientId == clientId);

        if (!string.IsNullOrEmpty(commandType))
            query = query.Where(c => c.CommandType == commandType);

        return await query
            .OrderByDescending(c => c.SentAt)
            .Take(limit)
            .ToListAsync();
    }

    /// <summary>
    /// Updates client flags.
    /// </summary>
    public async Task<bool> UpdateClientFlagsAsync(
        string clientId,
        bool? isFlagged = null,
        bool? isBlocked = null,
        bool? isTestMode = null,
        string? label = null)
    {
        var client = await _context.Clients.FindAsync(clientId);
        if (client == null) return false;

        if (isFlagged.HasValue)
            client.IsFlagged = isFlagged.Value;

        if (isBlocked.HasValue)
            client.IsBlocked = isBlocked.Value;

        if (isTestMode.HasValue)
            client.IsTestMode = isTestMode.Value;

        if (label != null)
            client.Label = label;

        await _context.SaveChangesAsync();
        return true;
    }

    /// <summary>
    /// Checks if a client is in test mode.
    /// </summary>
    public async Task<bool> IsClientInTestModeAsync(string clientId)
    {
        var client = await _context.Clients.FindAsync(clientId);
        return client?.IsTestMode ?? false;
    }

    /// <summary>
    /// Gets database statistics.
    /// </summary>
    public async Task<DatabaseStats> GetStatsAsync()
    {
        var now = DateTime.UtcNow;
        var oneDayAgo = now.AddDays(-1);
        var oneWeekAgo = now.AddDays(-7);

        return new DatabaseStats
        {
            TotalClients = await _context.Clients.CountAsync(),
            FlaggedClients = await _context.Clients.CountAsync(c => c.IsFlagged),
            BlockedClients = await _context.Clients.CountAsync(c => c.IsBlocked),
            ClientsSeenToday = await _context.Clients.CountAsync(c => c.LastSeenAt >= oneDayAgo),
            ClientsSeenThisWeek = await _context.Clients.CountAsync(c => c.LastSeenAt >= oneWeekAgo),
            TotalConnections = await _context.ClientConnections.CountAsync(),
            ConnectionsToday = await _context.ClientConnections.CountAsync(c => c.ConnectedAt >= oneDayAgo),
            TotalCommands = await _context.CommandLogs.CountAsync(),
            CommandsToday = await _context.CommandLogs.CountAsync(c => c.SentAt >= oneDayAgo)
        };
    }

    /// <summary>
    /// Deletes a client and all related data.
    /// </summary>
    public async Task<bool> DeleteClientAsync(string clientId)
    {
        var client = await _context.Clients.FindAsync(clientId);
        if (client == null) return false;

        _context.Clients.Remove(client);
        await _context.SaveChangesAsync();
        return true;
    }
}

/// <summary>
/// Database statistics DTO.
/// </summary>
public class DatabaseStats
{
    public int TotalClients { get; set; }
    public int FlaggedClients { get; set; }
    public int BlockedClients { get; set; }
    public int ClientsSeenToday { get; set; }
    public int ClientsSeenThisWeek { get; set; }
    public int TotalConnections { get; set; }
    public int ConnectionsToday { get; set; }
    public int TotalCommands { get; set; }
    public int CommandsToday { get; set; }
}
