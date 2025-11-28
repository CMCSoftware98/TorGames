using System.Collections.Concurrent;
using Microsoft.Extensions.Logging;

namespace TorGames.Client.Services;

/// <summary>
/// Thread-safe circular buffer for storing recent log entries.
/// </summary>
public sealed class LogBuffer
{
    private static readonly Lazy<LogBuffer> _instance = new(() => new LogBuffer());
    public static LogBuffer Instance => _instance.Value;

    private readonly ConcurrentQueue<LogEntry> _logs = new();
    private readonly int _maxEntries;
    private int _currentCount;

    public LogBuffer(int maxEntries = 500)
    {
        _maxEntries = maxEntries;
    }

    /// <summary>
    /// Add a log entry to the buffer.
    /// </summary>
    public void Add(LogLevel level, string category, string message)
    {
        var entry = new LogEntry
        {
            Timestamp = DateTime.Now,
            Level = level,
            Category = category,
            Message = message
        };

        _logs.Enqueue(entry);
        Interlocked.Increment(ref _currentCount);

        // Remove old entries if over limit
        while (_currentCount > _maxEntries && _logs.TryDequeue(out _))
        {
            Interlocked.Decrement(ref _currentCount);
        }
    }

    /// <summary>
    /// Get all log entries as formatted strings.
    /// </summary>
    public string GetLogs(int? maxLines = null)
    {
        var entries = _logs.ToArray();

        if (maxLines.HasValue && entries.Length > maxLines.Value)
        {
            entries = entries.TakeLast(maxLines.Value).ToArray();
        }

        return string.Join(Environment.NewLine, entries.Select(e => e.ToString()));
    }

    /// <summary>
    /// Get log entries as a list.
    /// </summary>
    public List<LogEntry> GetLogEntries(int? maxLines = null)
    {
        var entries = _logs.ToArray();

        if (maxLines.HasValue && entries.Length > maxLines.Value)
        {
            entries = entries.TakeLast(maxLines.Value).ToArray();
        }

        return entries.ToList();
    }

    /// <summary>
    /// Clear all log entries.
    /// </summary>
    public void Clear()
    {
        while (_logs.TryDequeue(out _))
        {
            Interlocked.Decrement(ref _currentCount);
        }
    }

    public int Count => _currentCount;
}

/// <summary>
/// A single log entry.
/// </summary>
public class LogEntry
{
    public DateTime Timestamp { get; set; }
    public LogLevel Level { get; set; }
    public string Category { get; set; } = "";
    public string Message { get; set; } = "";

    public override string ToString()
    {
        var levelStr = Level switch
        {
            LogLevel.Trace => "trce",
            LogLevel.Debug => "dbug",
            LogLevel.Information => "info",
            LogLevel.Warning => "warn",
            LogLevel.Error => "fail",
            LogLevel.Critical => "crit",
            _ => "????"
        };

        return $"{Timestamp:HH:mm:ss} [{levelStr}] {Category}: {Message}";
    }
}
