using System.Text;
using Microsoft.AspNetCore.Authentication.JwtBearer;
using Microsoft.EntityFrameworkCore;
using Microsoft.IdentityModel.Tokens;
using TorGames.Database;
using TorGames.Database.Extensions;
using TorGames.Database.Services;
using TorGames.Server.Hubs;
using TorGames.Server.Services;

var builder = WebApplication.CreateBuilder(args);

// Configure Kestrel for both HTTP/1.1 (REST) and HTTP/2 (gRPC)
builder.WebHost.ConfigureKestrel(options =>
{
    // Port 5000 for gRPC (HTTP/2 only)
    options.ListenAnyIP(5000, listenOptions =>
    {
        listenOptions.Protocols = Microsoft.AspNetCore.Server.Kestrel.Core.HttpProtocols.Http2;
    });

    // Port 5001 for REST API (HTTP/1.1)
    options.ListenAnyIP(5001, listenOptions =>
    {
        listenOptions.Protocols = Microsoft.AspNetCore.Server.Kestrel.Core.HttpProtocols.Http1;
    });

    // Connection limits for 10K+ clients
    options.Limits.MaxConcurrentConnections = 15000;
    options.Limits.MaxConcurrentUpgradedConnections = 15000;

    // HTTP/2 settings
    options.Limits.Http2.MaxStreamsPerConnection = 100;
    options.Limits.Http2.InitialConnectionWindowSize = 128 * 1024;  // 128KB
    options.Limits.Http2.InitialStreamWindowSize = 64 * 1024;       // 64KB
});

// Add services
builder.Services.AddGrpc();
builder.Services.AddSingleton<ClientManager>();
builder.Services.AddHostedService<ClientCleanupService>();

// Add JWT Service
builder.Services.AddSingleton<JwtService>();

// Add Update Service
builder.Services.AddSingleton<UpdateService>();

// Add Database
var dataFolder = Path.Combine(AppContext.BaseDirectory, "Data");
Directory.CreateDirectory(dataFolder);
var databasePath = Path.Combine(dataFolder, "torgames.db");
builder.Services.AddTorGamesDatabase($"Data Source={databasePath}");
builder.Services.AddScoped<ClientRepository>();
builder.Services.AddHostedService<DatabaseSyncService>();

// Add SignalR
builder.Services.AddSignalR();
builder.Services.AddHostedService<SignalRBroadcastService>();

// Add graceful shutdown handler
builder.Services.AddHostedService<GracefulShutdownService>();

// Add Controllers for REST API
builder.Services.AddControllers();

// Configure JWT Authentication
var jwtSecret = builder.Configuration["Jwt:Secret"] ?? throw new InvalidOperationException("JWT Secret not configured");
var jwtIssuer = builder.Configuration["Jwt:Issuer"] ?? "TorGames.Server";
var jwtAudience = builder.Configuration["Jwt:Audience"] ?? "TorGames.App";

builder.Services.AddAuthentication(options =>
{
    options.DefaultAuthenticateScheme = JwtBearerDefaults.AuthenticationScheme;
    options.DefaultChallengeScheme = JwtBearerDefaults.AuthenticationScheme;
})
.AddJwtBearer(options =>
{
    options.TokenValidationParameters = new TokenValidationParameters
    {
        ValidateIssuerSigningKey = true,
        IssuerSigningKey = new SymmetricSecurityKey(Encoding.UTF8.GetBytes(jwtSecret)),
        ValidateIssuer = true,
        ValidIssuer = jwtIssuer,
        ValidateAudience = true,
        ValidAudience = jwtAudience,
        ValidateLifetime = true,
        ClockSkew = TimeSpan.Zero
    };

    // SignalR sends token as query string, not header
    options.Events = new JwtBearerEvents
    {
        OnMessageReceived = context =>
        {
            var accessToken = context.Request.Query["access_token"];
            var path = context.HttpContext.Request.Path;

            if (!string.IsNullOrEmpty(accessToken) && path.StartsWithSegments("/hubs"))
            {
                context.Token = accessToken;
            }

            return Task.CompletedTask;
        }
    };
});

builder.Services.AddAuthorization();

// Configure CORS for the Tauri app
builder.Services.AddCors(options =>
{
    options.AddPolicy("TauriApp", policy =>
    {
        policy.WithOrigins(
                "http://localhost:5173",    // Vite dev server
                "tauri://localhost",        // Tauri app
                "https://tauri.localhost",  // Tauri app (HTTPS)
                "http://144.91.111.101:5173" // Remote dev server
            )
            .SetIsOriginAllowed(_ => true)  // Allow any origin for now
            .AllowAnyMethod()
            .AllowAnyHeader()
            .AllowCredentials();
    });
});

// Configure logging
builder.Logging.AddConsole();

var app = builder.Build();

// Ensure database is created and migrated
using (var scope = app.Services.CreateScope())
{
    var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();
    dbContext.Database.EnsureCreated();
}

// Use CORS
app.UseCors("TauriApp");

// Use Authentication & Authorization
app.UseAuthentication();
app.UseAuthorization();

// Map gRPC service
app.MapGrpcService<TorServiceImpl>();

// Map REST Controllers
app.MapControllers();

// Map SignalR Hub
app.MapHub<ClientHub>("/hubs/clients");

// Health check endpoint
app.MapGet("/", () => "TorGames Server is running. REST API on :5001, gRPC on :5000, SignalR on /hubs/clients");

// Log startup info
var clientManager = app.Services.GetRequiredService<ClientManager>();
app.Lifetime.ApplicationStarted.Register(() =>
{
    Console.WriteLine("========================================");
    Console.WriteLine("  TorGames Server Started");
    Console.WriteLine("  gRPC listening on: http://0.0.0.0:5000");
    Console.WriteLine("  REST API on: http://0.0.0.0:5001");
    Console.WriteLine("  SignalR Hub: /hubs/clients");
    Console.WriteLine("  Max connections: 15,000");
    Console.WriteLine("  Press Ctrl+C to shutdown gracefully");
    Console.WriteLine("========================================");
});

app.Lifetime.ApplicationStopped.Register(() =>
{
    Console.WriteLine("========================================");
    Console.WriteLine("  TorGames Server Stopped");
    Console.WriteLine("========================================");
});

await app.RunAsync();
