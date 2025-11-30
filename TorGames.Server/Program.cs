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

// Add TCP Installer Service for lightweight C++ clients
builder.Services.AddSingleton<TcpInstallerService>();
builder.Services.AddHostedService(sp => sp.GetRequiredService<TcpInstallerService>());

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

// Apply pending database migrations on startup
using (var scope = app.Services.CreateScope())
{
    var dbContext = scope.ServiceProvider.GetRequiredService<TorGamesDbContext>();
    var logger = scope.ServiceProvider.GetRequiredService<ILogger<Program>>();

    try
    {
        // Check if database file exists
        var dbExists = File.Exists(databasePath);

        if (!dbExists)
        {
            // New database - create with migrations
            logger.LogInformation("Creating new database with migrations...");
            dbContext.Database.Migrate();
            logger.LogInformation("Database created successfully");
        }
        else
        {
            // Check if migrations table exists (database may have been created with EnsureCreated)
            var connection = dbContext.Database.GetDbConnection();
            await connection.OpenAsync();

            using var command = connection.CreateCommand();
            command.CommandText = "SELECT name FROM sqlite_master WHERE type='table' AND name='__EFMigrationsHistory'";
            var migrationsTableExists = await command.ExecuteScalarAsync() != null;

            if (!migrationsTableExists)
            {
                // Database exists but was created without migrations
                // Create migrations table and mark existing schema as applied
                logger.LogInformation("Initializing migrations for existing database...");

                // Create the migrations history table
                using var createTableCmd = connection.CreateCommand();
                createTableCmd.CommandText = @"
                    CREATE TABLE IF NOT EXISTS ""__EFMigrationsHistory"" (
                        ""MigrationId"" TEXT NOT NULL CONSTRAINT ""PK___EFMigrationsHistory"" PRIMARY KEY,
                        ""ProductVersion"" TEXT NOT NULL
                    )";
                await createTableCmd.ExecuteNonQueryAsync();

                // Check if IsTestMode column already exists
                using var checkColumnCmd = connection.CreateCommand();
                checkColumnCmd.CommandText = "PRAGMA table_info(Clients)";
                var hasIsTestMode = false;
                using (var reader = await checkColumnCmd.ExecuteReaderAsync())
                {
                    while (await reader.ReadAsync())
                    {
                        if (reader.GetString(1) == "IsTestMode")
                        {
                            hasIsTestMode = true;
                            break;
                        }
                    }
                }

                if (!hasIsTestMode)
                {
                    // Add the IsTestMode column
                    logger.LogInformation("Adding IsTestMode column to Clients table...");
                    using var addColumnCmd = connection.CreateCommand();
                    addColumnCmd.CommandText = "ALTER TABLE Clients ADD COLUMN IsTestMode INTEGER NOT NULL DEFAULT 0";
                    await addColumnCmd.ExecuteNonQueryAsync();
                }

                // Mark the migration as applied
                using var insertCmd = connection.CreateCommand();
                insertCmd.CommandText = @"
                    INSERT OR IGNORE INTO ""__EFMigrationsHistory"" (""MigrationId"", ""ProductVersion"")
                    VALUES ('20251130225506_AddIsTestModeToClient', '10.0.0')";
                await insertCmd.ExecuteNonQueryAsync();

                logger.LogInformation("Database migrations initialized");
            }
            else
            {
                // Normal migration path
                var pendingMigrations = dbContext.Database.GetPendingMigrations().ToList();
                if (pendingMigrations.Count > 0)
                {
                    logger.LogInformation("Applying {Count} pending database migration(s): {Migrations}",
                        pendingMigrations.Count, string.Join(", ", pendingMigrations));
                    dbContext.Database.Migrate();
                    logger.LogInformation("Database migrations applied successfully");
                }
                else
                {
                    logger.LogInformation("Database is up to date");
                }
            }
        }
    }
    catch (Exception ex)
    {
        logger.LogError(ex, "Error applying database migrations");
        throw;
    }
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
    var tcpPort = app.Configuration.GetValue("TcpInstaller:Port", 5050);
    Console.WriteLine("========================================");
    Console.WriteLine("  TorGames Server Started");
    Console.WriteLine("  gRPC listening on: http://0.0.0.0:5000");
    Console.WriteLine("  REST API on: http://0.0.0.0:5001");
    Console.WriteLine($"  TCP Installer on: tcp://0.0.0.0:{tcpPort}");
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
