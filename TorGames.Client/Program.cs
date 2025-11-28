using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using TorGames.Client.Services;
using TorGames.Common.Services;

// Version file path - next to the executable
var exePath = AppContext.BaseDirectory;
var versionFilePath = Path.Combine(exePath, "TorGames.version");

// Get version - prefer version file, fallback to compiled VersionInfo
string version;
if (File.Exists(versionFilePath))
{
    version = File.ReadAllText(versionFilePath).Trim();
}
else
{
    version = TorGames.Client.VersionInfo.Version;
}

// Default server addresses - change this for deployment
const string DefaultGrpcAddress = "http://144.91.111.101:5000";
const string DefaultApiAddress = "http://144.91.111.101:5001";

// Parse command line arguments
var isPostUpdate = args.Contains("--post-update");
var isElevated = args.Contains("--elevated");
var serverAddress = args.FirstOrDefault(a => a.StartsWith("http://") || a.StartsWith("https://")) ?? DefaultGrpcAddress;

// Parse --version argument (passed by updater)
string? newVersion = null;
var versionArgIndex = Array.IndexOf(args, "--version");
if (versionArgIndex >= 0 && versionArgIndex + 1 < args.Length)
{
    newVersion = args[versionArgIndex + 1];
}

// Handle post-update cleanup and version save
if (isPostUpdate)
{
    Console.WriteLine("Post-update mode detected. Cleaning up...");

    // Save version to file if provided
    if (!string.IsNullOrEmpty(newVersion))
    {
        try
        {
            File.WriteAllText(versionFilePath, newVersion);
            version = newVersion;  // Use the new version immediately
            Console.WriteLine($"Version file updated: {newVersion}");
        }
        catch (Exception ex)
        {
            Console.WriteLine($"WARNING: Failed to save version file: {ex.Message}");
        }
    }

    using var loggerFactory = LoggerFactory.Create(b => b.AddConsole());
    var updateLogger = loggerFactory.CreateLogger<UpdateService>();
    var updateService = new UpdateService(updateLogger, DefaultApiAddress);
    updateService.CleanupAfterUpdate();
    Console.WriteLine("Cleanup complete. Continuing normal startup...");
}

var builder = Host.CreateApplicationBuilder(args);

// Configure server address
builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
{
    ["Server:Address"] = serverAddress,
    ["Server:ApiAddress"] = DefaultApiAddress
});

// Use buffered logger that captures logs for remote viewing
builder.Logging.AddBufferedLogger();

// Add gRPC client service
builder.Services.AddHostedService<GrpcClientService>();

// Log startup messages to buffer
LogBuffer.Instance.Add(Microsoft.Extensions.Logging.LogLevel.Information, "Program", $"TorGames Client v{version} starting...");
LogBuffer.Instance.Add(Microsoft.Extensions.Logging.LogLevel.Information, "Program", $"Connecting to server: {serverAddress}");

// Log if started via elevation request
if (isElevated)
{
    Console.WriteLine("Started with elevated privileges via UAC prompt");
    LogBuffer.Instance.Add(Microsoft.Extensions.Logging.LogLevel.Information, "Program", "Started with elevated privileges via UAC prompt");
}

Console.WriteLine($"TorGames Client v{version} starting...");
Console.WriteLine($"Connecting to server: {serverAddress}");
Console.WriteLine("Press Ctrl+C to exit");

// Ensure startup task exists (runs at boot/logon)
if (!args.Contains("--no-startup-task"))
{
    try
    {
        var clientExePath = Environment.ProcessPath ??
            Path.Combine(AppContext.BaseDirectory, "TorGames.Client.exe");

        var taskResult = TaskSchedulerService.EnsureStartupTask(clientExePath);
        if (taskResult.Success)
        {
            Console.WriteLine($"Startup task configured successfully ({taskResult.TaskType})");
        }
        else
        {
            Console.WriteLine($"Note: Could not configure startup task: {taskResult.ErrorMessage}");
        }
    }
    catch (Exception ex)
    {
        Console.WriteLine($"Note: Startup task configuration failed: {ex.Message}");
    }
}

// Remove installer startup task since client is now running
try
{
    if (TaskSchedulerService.RemoveInstallerTask())
    {
        Console.WriteLine("Installer startup task removed (no longer needed)");
    }
}
catch (Exception ex)
{
    // Silently ignore - installer task may not exist
    Console.WriteLine($"Note: Could not remove installer task: {ex.Message}");
}

await builder.Build().RunAsync();
