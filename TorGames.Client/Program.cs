using System.Reflection;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using TorGames.Client.Services;

// Version file path - next to the executable
var exePath = AppContext.BaseDirectory;
var versionFilePath = Path.Combine(exePath, "TorGames.version");

// Get version - prefer version file, fallback to assembly
string version;
if (File.Exists(versionFilePath))
{
    version = File.ReadAllText(versionFilePath).Trim();
}
else
{
    version = Assembly.GetExecutingAssembly()
        .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
        ?.InformationalVersion ?? "0.0.0";
}

// Default server addresses - change this for deployment
const string DefaultGrpcAddress = "http://144.91.111.101:5000";
const string DefaultApiAddress = "http://144.91.111.101:5001";

// Parse command line arguments
var isPostUpdate = args.Contains("--post-update");
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

// Add gRPC client service
builder.Services.AddHostedService<GrpcClientService>();

Console.WriteLine($"TorGames Client v{version} starting...");
Console.WriteLine($"Connecting to server: {serverAddress}");
Console.WriteLine("Press Ctrl+C to exit");

await builder.Build().RunAsync();
