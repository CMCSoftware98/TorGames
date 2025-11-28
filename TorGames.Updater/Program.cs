// TorGames.Updater - Crash-proof update helper
// This small executable handles the file replacement when updating TorGames.Client.
// It is designed to NEVER crash - all operations are wrapped in try-catch blocks.

using System.Diagnostics;

namespace TorGames.Updater;

internal static class Program
{
    private static string? _logPath;

    /// <summary>
    /// Entry point - crash-proof wrapper around the update logic.
    /// Arguments: targetPath newFilePath backupDir parentPid [version]
    /// </summary>
    public static int Main(string[] args)
    {
        try
        {
            // Set up log file in temp directory
            _logPath = Path.Combine(Path.GetTempPath(), "TorGames_Updater.log");
            Log("========================================");
            Log($"TorGames.Updater started at {DateTime.Now:yyyy-MM-dd HH:mm:ss}");
            Log($"Arguments: {string.Join(" ", args)}");

            // Validate arguments
            if (args.Length < 4)
            {
                Log("ERROR: Insufficient arguments");
                Log("Usage: TorGames.Updater.exe <targetPath> <newFilePath> <backupDir> <parentPid> [version]");
                return 1;
            }

            var targetPath = args[0];
            var newFilePath = args[1];
            var backupDir = args[2];

            if (!int.TryParse(args[3], out var parentPid))
            {
                Log($"ERROR: Invalid parent PID: {args[3]}");
                return 1;
            }

            // Version is optional 5th argument
            var version = args.Length > 4 ? args[4] : null;
            if (!string.IsNullOrEmpty(version))
            {
                Log($"Target version: {version}");
            }

            // Run the update process
            return RunUpdate(targetPath, newFilePath, backupDir, parentPid, version);
        }
        catch (Exception ex)
        {
            Log($"FATAL: Unhandled exception in Main: {ex.Message}");
            Log(ex.ToString());
            return 1;
        }
    }

    private static int RunUpdate(string targetPath, string newFilePath, string backupDir, int parentPid, string? version)
    {
        string? backupPath = null;

        try
        {
            // Step 1: Wait for parent process to exit
            Log($"Step 1: Waiting for parent process (PID: {parentPid}) to exit...");
            if (!WaitForProcessExit(parentPid, TimeSpan.FromSeconds(60)))
            {
                Log("WARNING: Parent process did not exit within timeout, proceeding anyway...");
            }

            // Add a small delay to ensure file handles are released
            Thread.Sleep(1000);

            // Step 2: Validate paths
            Log("Step 2: Validating paths...");
            if (!File.Exists(newFilePath))
            {
                Log($"ERROR: New file does not exist: {newFilePath}");
                return 1;
            }

            if (!File.Exists(targetPath))
            {
                Log($"WARNING: Target file does not exist: {targetPath}");
                // This might be first install, continue anyway
            }

            // Step 3: Create backup
            Log("Step 3: Creating backup...");
            backupPath = CreateBackup(targetPath, backupDir);
            if (backupPath != null)
            {
                Log($"Backup created at: {backupPath}");
            }
            else
            {
                Log("WARNING: Failed to create backup, continuing without backup");
            }

            // Step 4: Copy new file (with retry logic)
            Log("Step 4: Copying new file...");
            if (!CopyFileWithRetry(newFilePath, targetPath, maxRetries: 5, delayMs: 1000))
            {
                Log("ERROR: Failed to copy new file after retries");

                // Attempt rollback
                if (backupPath != null && File.Exists(backupPath))
                {
                    Log("Attempting rollback from backup...");
                    if (CopyFileWithRetry(backupPath, targetPath, maxRetries: 3, delayMs: 500))
                    {
                        Log("Rollback successful");
                    }
                    else
                    {
                        Log("ERROR: Rollback also failed!");
                    }
                }
                return 1;
            }

            // Step 5: Verify new file
            Log("Step 5: Verifying new file...");
            if (!File.Exists(targetPath))
            {
                Log("ERROR: Target file missing after copy");
                return 1;
            }

            var targetInfo = new FileInfo(targetPath);
            var sourceInfo = new FileInfo(newFilePath);
            if (targetInfo.Length != sourceInfo.Length)
            {
                Log($"ERROR: File size mismatch. Expected: {sourceInfo.Length}, Got: {targetInfo.Length}");
                return 1;
            }

            Log($"File verified. Size: {targetInfo.Length} bytes");

            // Step 6: Clean up old backups (keep only last 3)
            Log("Step 6: Cleaning up old backups...");
            CleanupOldBackups(backupDir, keepCount: 3);

            // Step 7: Start new client with --post-update flag and version
            Log("Step 7: Starting updated client...");
            var clientArgs = string.IsNullOrEmpty(version)
                ? "--post-update"
                : $"--post-update --version \"{version}\"";
            if (!StartProcess(targetPath, clientArgs))
            {
                Log("WARNING: Failed to start updated client");
                // Don't return error - the update itself was successful
            }

            Log("========================================");
            Log("Update completed successfully!");
            Log("========================================");
            return 0;
        }
        catch (Exception ex)
        {
            Log($"ERROR: Exception during update: {ex.Message}");
            Log(ex.ToString());

            // Attempt rollback on any error
            if (backupPath != null && File.Exists(backupPath))
            {
                Log("Attempting rollback from backup...");
                try
                {
                    if (CopyFileWithRetry(backupPath, targetPath, maxRetries: 3, delayMs: 500))
                    {
                        Log("Rollback successful");

                        // Try to start the old version
                        StartProcess(targetPath, string.Empty);
                    }
                }
                catch (Exception rollbackEx)
                {
                    Log($"ERROR: Rollback failed: {rollbackEx.Message}");
                }
            }

            return 1;
        }
    }

    /// <summary>
    /// Waits for a process to exit with timeout.
    /// </summary>
    private static bool WaitForProcessExit(int pid, TimeSpan timeout)
    {
        var stopwatch = Stopwatch.StartNew();

        while (stopwatch.Elapsed < timeout)
        {
            try
            {
                var process = Process.GetProcessById(pid);
                // Process still exists, wait a bit
                Thread.Sleep(500);
            }
            catch (ArgumentException)
            {
                // Process has exited
                Log($"Parent process exited after {stopwatch.ElapsedMilliseconds}ms");
                return true;
            }
            catch (Exception ex)
            {
                Log($"WARNING: Error checking process: {ex.Message}");
                Thread.Sleep(500);
            }
        }

        return false;
    }

    /// <summary>
    /// Creates a timestamped backup of the target file.
    /// </summary>
    private static string? CreateBackup(string targetPath, string backupDir)
    {
        try
        {
            if (!File.Exists(targetPath))
                return null;

            // Ensure backup directory exists
            if (!Directory.Exists(backupDir))
            {
                Directory.CreateDirectory(backupDir);
            }

            var timestamp = DateTime.Now.ToString("yyyyMMdd_HHmmss");
            var backupFileName = $"TorGames.Client_{timestamp}.exe.bak";
            var backupPath = Path.Combine(backupDir, backupFileName);

            File.Copy(targetPath, backupPath, overwrite: true);

            // Verify backup was created
            if (File.Exists(backupPath))
            {
                return backupPath;
            }
        }
        catch (Exception ex)
        {
            Log($"WARNING: Failed to create backup: {ex.Message}");
        }

        return null;
    }

    /// <summary>
    /// Copies a file with retry logic for handling locked files.
    /// </summary>
    private static bool CopyFileWithRetry(string source, string destination, int maxRetries, int delayMs)
    {
        for (int attempt = 1; attempt <= maxRetries; attempt++)
        {
            try
            {
                Log($"Copy attempt {attempt}/{maxRetries}: {source} -> {destination}");

                // Ensure destination directory exists
                var destDir = Path.GetDirectoryName(destination);
                if (!string.IsNullOrEmpty(destDir) && !Directory.Exists(destDir))
                {
                    Directory.CreateDirectory(destDir);
                }

                // Delete existing file if present
                if (File.Exists(destination))
                {
                    File.Delete(destination);
                }

                // Copy new file
                File.Copy(source, destination, overwrite: true);

                // Verify
                if (File.Exists(destination))
                {
                    Log($"Copy successful on attempt {attempt}");
                    return true;
                }
            }
            catch (IOException ex) when (attempt < maxRetries)
            {
                Log($"Copy attempt {attempt} failed (IO): {ex.Message}");
                Thread.Sleep(delayMs);
            }
            catch (UnauthorizedAccessException ex) when (attempt < maxRetries)
            {
                Log($"Copy attempt {attempt} failed (Access): {ex.Message}");
                Thread.Sleep(delayMs);
            }
            catch (Exception ex)
            {
                Log($"Copy attempt {attempt} failed: {ex.Message}");
                if (attempt >= maxRetries)
                    return false;
                Thread.Sleep(delayMs);
            }
        }

        return false;
    }

    /// <summary>
    /// Removes old backup files, keeping only the most recent ones.
    /// </summary>
    private static void CleanupOldBackups(string backupDir, int keepCount)
    {
        try
        {
            if (!Directory.Exists(backupDir))
                return;

            var backupFiles = Directory.GetFiles(backupDir, "*.bak")
                .OrderByDescending(f => f)
                .Skip(keepCount)
                .ToList();

            foreach (var file in backupFiles)
            {
                try
                {
                    File.Delete(file);
                    Log($"Deleted old backup: {file}");
                }
                catch (Exception ex)
                {
                    Log($"WARNING: Failed to delete backup {file}: {ex.Message}");
                }
            }

            Log($"Backup cleanup complete. Kept {keepCount} most recent backups.");
        }
        catch (Exception ex)
        {
            Log($"WARNING: Failed to cleanup backups: {ex.Message}");
        }
    }

    /// <summary>
    /// Starts a process with optional arguments.
    /// </summary>
    private static bool StartProcess(string path, string arguments)
    {
        try
        {
            var startInfo = new ProcessStartInfo
            {
                FileName = path,
                Arguments = arguments,
                UseShellExecute = true,
                WorkingDirectory = Path.GetDirectoryName(path) ?? string.Empty
            };

            var process = Process.Start(startInfo);
            if (process != null)
            {
                Log($"Started process: {path} {arguments}");
                return true;
            }
        }
        catch (Exception ex)
        {
            Log($"WARNING: Failed to start process: {ex.Message}");
        }

        return false;
    }

    /// <summary>
    /// Writes a message to the log file.
    /// </summary>
    private static void Log(string message)
    {
        try
        {
            var logMessage = $"[{DateTime.Now:HH:mm:ss}] {message}";
            Console.WriteLine(logMessage);

            if (!string.IsNullOrEmpty(_logPath))
            {
                File.AppendAllText(_logPath, logMessage + Environment.NewLine);
            }
        }
        catch
        {
            // Never crash on logging errors
        }
    }
}
