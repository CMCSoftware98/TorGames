using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using TorGames.Client.Services;

// Default server address - change this for deployment
const string DefaultServerAddress = "http://144.91.111.101:5000";

// Allow override via command line: TorGames.Client.exe http://192.168.1.100:5000
var serverAddress = args.Length > 0 ? args[0] : DefaultServerAddress;

var builder = Host.CreateApplicationBuilder(args);

// Configure server address
builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
{
    ["Server:Address"] = serverAddress
});

// Add gRPC client service
builder.Services.AddHostedService<GrpcClientService>();

Console.WriteLine("TorGames Client starting...");
Console.WriteLine($"Connecting to server: {serverAddress}");
Console.WriteLine("Press Ctrl+C to exit");

await builder.Build().RunAsync();
