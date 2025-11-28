using Microsoft.Extensions.Logging;
using MsLogLevel = Microsoft.Extensions.Logging.LogLevel;

namespace TorGames.Client.Services;

/// <summary>
/// Logger provider that writes to both console and a circular buffer.
/// </summary>
public class BufferedLoggerProvider : ILoggerProvider
{
    public ILogger CreateLogger(string categoryName)
    {
        return new BufferedLogger(categoryName);
    }

    public void Dispose() { }
}

/// <summary>
/// Logger that writes to both console and the LogBuffer.
/// </summary>
public class BufferedLogger : ILogger
{
    private readonly string _categoryName;

    public BufferedLogger(string categoryName)
    {
        // Shorten category name for display
        _categoryName = categoryName.Contains('.')
            ? categoryName.Substring(categoryName.LastIndexOf('.') + 1)
            : categoryName;
    }

    public IDisposable? BeginScope<TState>(TState state) where TState : notnull => null;

    public bool IsEnabled(MsLogLevel logLevel) => logLevel >= MsLogLevel.Information;

    public void Log<TState>(MsLogLevel logLevel, EventId eventId, TState state, Exception? exception, Func<TState, Exception?, string> formatter)
    {
        if (!IsEnabled(logLevel))
            return;

        var message = formatter(state, exception);
        if (exception != null)
        {
            message += Environment.NewLine + exception.ToString();
        }

        // Convert Microsoft.Extensions.Logging.LogLevel to our LogLevel
        var bufferLevel = logLevel switch
        {
            MsLogLevel.Trace => LogLevel.Trace,
            MsLogLevel.Debug => LogLevel.Debug,
            MsLogLevel.Information => LogLevel.Information,
            MsLogLevel.Warning => LogLevel.Warning,
            MsLogLevel.Error => LogLevel.Error,
            MsLogLevel.Critical => LogLevel.Critical,
            _ => LogLevel.Information
        };

        // Add to buffer
        LogBuffer.Instance.Add(bufferLevel, _categoryName, message);

        // Also write to console with color
        var originalColor = Console.ForegroundColor;
        var levelStr = logLevel switch
        {
            MsLogLevel.Trace => "trce",
            MsLogLevel.Debug => "dbug",
            MsLogLevel.Information => "info",
            MsLogLevel.Warning => "warn",
            MsLogLevel.Error => "fail",
            MsLogLevel.Critical => "crit",
            _ => "????"
        };

        Console.ForegroundColor = logLevel switch
        {
            MsLogLevel.Trace => ConsoleColor.Gray,
            MsLogLevel.Debug => ConsoleColor.Gray,
            MsLogLevel.Information => ConsoleColor.Green,
            MsLogLevel.Warning => ConsoleColor.Yellow,
            MsLogLevel.Error => ConsoleColor.Red,
            MsLogLevel.Critical => ConsoleColor.DarkRed,
            _ => ConsoleColor.White
        };

        Console.Write($"{levelStr}: ");
        Console.ForegroundColor = ConsoleColor.White;
        Console.WriteLine($"{_categoryName}[{eventId.Id}]");
        Console.ForegroundColor = originalColor;
        Console.WriteLine($"      {message}");
    }
}

/// <summary>
/// Extension methods for adding the buffered logger.
/// </summary>
public static class BufferedLoggerExtensions
{
    public static ILoggingBuilder AddBufferedLogger(this ILoggingBuilder builder)
    {
        builder.ClearProviders();
        builder.AddProvider(new BufferedLoggerProvider());
        return builder;
    }
}
