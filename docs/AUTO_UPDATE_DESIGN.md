# TorGames.Client Auto-Update System Design

## Overview

This document outlines the auto-update architecture for TorGames.Client, leveraging the existing TorGames.Server infrastructure. The system supports automatic updates with backup/rollback capabilities.

## Architecture

```
┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
│  TorGames.Server│     │  TorGames.Client│     │  TorGames.Updater│
│                 │     │                 │     │  (Helper Process)│
│  - Host updates │────▶│  - Check version│────▶│  - Download new  │
│  - Serve files  │     │  - Request update│    │  - Backup old    │
│  - Track versions│    │  - Launch updater│    │  - Replace files │
└─────────────────┘     └─────────────────┘     │  - Rollback if   │
                                                │    needed        │
                                                └─────────────────┘
```

## Why External Updater?

Windows prevents overwriting a running executable. The solution requires a small helper process (TorGames.Updater) that:
1. Gets launched by TorGames.Client
2. Waits for TorGames.Client to exit
3. Performs the file replacement
4. Restarts TorGames.Client
5. Exits

## Components

### 1. Server-Side: Update Hosting

**New Files/Endpoints:**

```
TorGames.Server/
├── Updates/
│   ├── latest/
│   │   └── TorGames.Client.exe
│   └── manifest.json
├── Controllers/
│   └── UpdateController.cs
```

**manifest.json Structure:**
```json
{
  "version": "1.1.0",
  "releaseDate": "2025-01-15T10:00:00Z",
  "fileSize": 15728640,
  "sha256": "abc123...",
  "releaseNotes": "Bug fixes and performance improvements",
  "minVersion": "1.0.0",
  "mandatory": false
}
```

**UpdateController.cs:**
```csharp
[ApiController]
[Route("api/[controller]")]
public class UpdateController : ControllerBase
{
    private readonly string _updatesPath;

    [HttpGet("check")]
    public async Task<ActionResult<UpdateManifest>> CheckForUpdates([FromQuery] string currentVersion)
    {
        var manifest = await LoadManifestAsync();
        if (Version.Parse(manifest.Version) > Version.Parse(currentVersion))
        {
            return Ok(manifest);
        }
        return NoContent(); // No update available
    }

    [HttpGet("download")]
    public async Task<IActionResult> DownloadUpdate()
    {
        var filePath = Path.Combine(_updatesPath, "latest", "TorGames.Client.exe");
        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Client.exe");
    }

    [HttpGet("updater")]
    public async Task<IActionResult> DownloadUpdater()
    {
        var filePath = Path.Combine(_updatesPath, "TorGames.Updater.exe");
        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Updater.exe");
    }
}
```

### 2. Client-Side: Update Checker

**New Service in TorGames.Client:**

```csharp
public class UpdateService
{
    private readonly HttpClient _httpClient;
    private readonly ILogger<UpdateService> _logger;
    private readonly string _serverUrl;
    private readonly string _currentVersion;

    public async Task<UpdateInfo?> CheckForUpdateAsync()
    {
        try
        {
            var response = await _httpClient.GetAsync(
                $"{_serverUrl}/api/update/check?currentVersion={_currentVersion}");

            if (response.StatusCode == HttpStatusCode.NoContent)
                return null; // No update

            return await response.Content.ReadFromJsonAsync<UpdateInfo>();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to check for updates");
            return null;
        }
    }

    public async Task<bool> DownloadAndApplyUpdateAsync(UpdateInfo update)
    {
        var tempPath = Path.GetTempPath();
        var downloadPath = Path.Combine(tempPath, "TorGames_Update");
        Directory.CreateDirectory(downloadPath);

        try
        {
            // 1. Download new client
            var newClientPath = Path.Combine(downloadPath, "TorGames.Client.exe");
            await DownloadFileAsync($"{_serverUrl}/api/update/download", newClientPath);

            // 2. Verify checksum
            if (!VerifySha256(newClientPath, update.Sha256))
            {
                _logger.LogError("Update file checksum mismatch!");
                return false;
            }

            // 3. Download updater if not present
            var updaterPath = Path.Combine(downloadPath, "TorGames.Updater.exe");
            if (!File.Exists(updaterPath))
            {
                await DownloadFileAsync($"{_serverUrl}/api/update/updater", updaterPath);
            }

            // 4. Launch updater and exit
            var currentExe = Process.GetCurrentProcess().MainModule?.FileName;
            var backupPath = Path.Combine(downloadPath, "backup");

            Process.Start(new ProcessStartInfo
            {
                FileName = updaterPath,
                Arguments = $"\"{currentExe}\" \"{newClientPath}\" \"{backupPath}\" {Process.GetCurrentProcess().Id}",
                UseShellExecute = true
            });

            // Exit to allow updater to replace files
            Environment.Exit(0);
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to download/apply update");
            return false;
        }
    }

    private bool VerifySha256(string filePath, string expectedHash)
    {
        using var sha256 = SHA256.Create();
        using var stream = File.OpenRead(filePath);
        var hash = BitConverter.ToString(sha256.ComputeHash(stream))
            .Replace("-", "").ToLowerInvariant();
        return hash == expectedHash.ToLowerInvariant();
    }
}
```

### 3. Updater Helper: TorGames.Updater

**New Project: TorGames.Updater**

A minimal console application (~50KB compiled) that handles the actual file replacement:

```csharp
// TorGames.Updater/Program.cs
class Program
{
    static async Task<int> Main(string[] args)
    {
        if (args.Length < 4)
        {
            Console.WriteLine("Usage: TorGames.Updater <target> <source> <backup> <pid>");
            return 1;
        }

        var targetPath = args[0];      // Current client location
        var sourcePath = args[1];      // New client download
        var backupDir = args[2];       // Backup directory
        var parentPid = int.Parse(args[3]);

        try
        {
            // Wait for parent process to exit (max 30 seconds)
            Console.WriteLine("Waiting for TorGames.Client to exit...");
            await WaitForProcessExit(parentPid, TimeSpan.FromSeconds(30));

            // Create backup
            Console.WriteLine("Creating backup...");
            Directory.CreateDirectory(backupDir);
            var backupPath = Path.Combine(backupDir,
                $"TorGames.Client_{DateTime.Now:yyyyMMdd_HHmmss}.exe.bak");
            File.Copy(targetPath, backupPath, overwrite: true);

            // Keep only last 3 backups
            CleanupOldBackups(backupDir, keepCount: 3);

            // Replace file
            Console.WriteLine("Applying update...");
            File.Copy(sourcePath, targetPath, overwrite: true);

            // Verify the new file
            if (!File.Exists(targetPath))
            {
                throw new Exception("Update failed - target file missing after copy");
            }

            // Restart client
            Console.WriteLine("Restarting TorGames.Client...");
            Process.Start(new ProcessStartInfo
            {
                FileName = targetPath,
                UseShellExecute = true
            });

            // Cleanup temp files
            try { File.Delete(sourcePath); } catch { }

            Console.WriteLine("Update completed successfully!");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"Update failed: {ex.Message}");
            Console.WriteLine("Attempting rollback...");

            // Attempt rollback from latest backup
            var latestBackup = Directory.GetFiles(backupDir, "*.bak")
                .OrderByDescending(f => f)
                .FirstOrDefault();

            if (latestBackup != null)
            {
                try
                {
                    File.Copy(latestBackup, targetPath, overwrite: true);
                    Console.WriteLine("Rollback successful!");

                    // Restart old version
                    Process.Start(new ProcessStartInfo
                    {
                        FileName = targetPath,
                        UseShellExecute = true
                    });
                }
                catch (Exception rollbackEx)
                {
                    Console.WriteLine($"Rollback failed: {rollbackEx.Message}");
                }
            }

            return 1;
        }
    }

    static async Task WaitForProcessExit(int pid, TimeSpan timeout)
    {
        var stopwatch = Stopwatch.StartNew();
        while (stopwatch.Elapsed < timeout)
        {
            try
            {
                var process = Process.GetProcessById(pid);
                await Task.Delay(500);
            }
            catch (ArgumentException)
            {
                // Process has exited
                return;
            }
        }
        throw new TimeoutException("Parent process did not exit in time");
    }

    static void CleanupOldBackups(string backupDir, int keepCount)
    {
        var backups = Directory.GetFiles(backupDir, "*.bak")
            .OrderByDescending(f => f)
            .Skip(keepCount)
            .ToList();

        foreach (var backup in backups)
        {
            try { File.Delete(backup); } catch { }
        }
    }
}
```

## Backup & Rollback Strategy

### Backup System

1. **Pre-Update Backup**: Before any update, the current executable is copied to:
   ```
   %TEMP%/TorGames_Update/backup/TorGames.Client_20250115_143022.exe.bak
   ```

2. **Retention Policy**: Keep last 3 backups, delete older ones automatically.

3. **Backup Verification**: After creating backup, verify file exists and has correct size.

### Rollback Triggers

Automatic rollback occurs when:
- File copy fails during update
- New executable fails to start
- Checksum verification fails after copy

### Manual Rollback (via Server Command)

Add a new command type to allow server-initiated rollback:

```csharp
case "rollback":
    result = await RollbackToBackupAsync(command);
    break;
```

## gRPC Integration (Alternative to HTTP)

The existing proto already has `FileTransferRequest`. We can extend it for updates:

**Proto Additions:**
```protobuf
message UpdateAvailable {
  string version = 1;
  string sha256 = 2;
  int64 file_size = 3;
  string release_notes = 4;
  bool mandatory = 5;
}

message UpdateRequest {
  string current_version = 1;
}

// Add to ServerMessage oneof:
UpdateAvailable update_available = 8;

// Add to ClientMessage oneof:
UpdateRequest update_request = 8;
```

This allows the server to push update notifications to connected clients.

## Implementation Phases

### Phase 1: Basic Infrastructure (MVP)
- [ ] Create TorGames.Updater project
- [ ] Add UpdateController to server
- [ ] Add UpdateService to client
- [ ] Implement basic download and replace

### Phase 2: Backup & Rollback
- [ ] Implement backup creation
- [ ] Add rollback functionality
- [ ] Add backup cleanup
- [ ] Test rollback scenarios

### Phase 3: Server Push Notifications
- [ ] Add proto messages for updates
- [ ] Server broadcasts update availability
- [ ] Client responds to update notifications
- [ ] Add "rollback" command support

### Phase 4: Admin Dashboard Integration
- [ ] Show client versions in dashboard
- [ ] Upload new versions via dashboard
- [ ] Trigger mass updates
- [ ] View update status/history

## Security Considerations

1. **Checksum Verification**: Always verify SHA256 before applying updates
2. **HTTPS**: Use HTTPS for update downloads in production
3. **Code Signing**: Consider signing executables (future enhancement)
4. **Version Validation**: Prevent downgrade attacks by checking min version

## File Structure After Implementation

```
TorGames/
├── TorGames.Server/
│   ├── Controllers/
│   │   └── UpdateController.cs
│   └── Updates/
│       ├── manifest.json
│       ├── TorGames.Updater.exe
│       └── latest/
│           └── TorGames.Client.exe
├── TorGames.Client/
│   └── Services/
│       └── UpdateService.cs
├── TorGames.Updater/          # New project
│   ├── Program.cs
│   └── TorGames.Updater.csproj
└── TorGames.Common/
    └── Protos/
        └── tor_service.proto  # Updated with update messages
```

## Configuration

**Client appsettings.json:**
```json
{
  "Update": {
    "CheckIntervalMinutes": 60,
    "AutoUpdate": true,
    "BackupCount": 3
  }
}
```

**Server appsettings.json:**
```json
{
  "Updates": {
    "Path": "./Updates",
    "MaxFileSizeMb": 100
  }
}
```

## Testing Checklist

- [ ] Update downloads correctly
- [ ] Checksum verification works
- [ ] Backup is created before update
- [ ] Update applies successfully
- [ ] Client restarts after update
- [ ] Rollback works when update fails
- [ ] Old backups are cleaned up
- [ ] Server push notifications work
- [ ] Dashboard shows correct versions
