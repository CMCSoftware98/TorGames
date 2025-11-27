using Microsoft.Extensions.Configuration;
using Microsoft.Extensions.DependencyInjection;
using Microsoft.Extensions.Hosting;
using TorGames.Client.Services;

var builder = Host.CreateApplicationBuilder(args);

// Configure server address
builder.Configuration.AddInMemoryCollection(new Dictionary<string, string?>
{
    ["Server:Address"] = "http://localhost:5000"
});

// Add gRPC client service
builder.Services.AddHostedService<GrpcClientService>();

Console.WriteLine("TorGames Client starting...");
Console.WriteLine("Press Ctrl+C to exit");

await builder.Build().RunAsync();
