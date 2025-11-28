using System.Reflection;
using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using Microsoft.Extensions.Logging;
using TorGames.Client.Services;

// Get version from assembly
var version = Assembly.GetExecutingAssembly()
    .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
    ?.InformationalVersion ?? "0.0.0";

// Default server addresses - change this for deployment
const string DefaultGrpcAddress = "http://144.91.111.101:5000";
const string DefaultApiAddress = "http://144.91.111.101:5001";

// Parse command line arguments
var isPostUpdate = args.Contains("--post-update");
var serverAddress = args.FirstOrDefault(a => a.StartsWith("http://") || a.StartsWith("https://")) ?? DefaultGrpcAddress;

// Handle post-update cleanup
if (isPostUpdate)
{
    Console.WriteLine("Post-update mode detected. Cleaning up...");
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
