using System;
using System.IO;
using System.Linq;
using Microsoft.Win32.TaskScheduler;

namespace TorGames.Common.Services;

/// <summary>
/// Result of a task scheduler operation.
/// </summary>
public class TaskSchedulerResult
{
    public bool Success { get; set; }
    public string? ErrorMessage { get; set; }
    public string? TaskType { get; set; } // "Admin" or "User"

    public static TaskSchedulerResult Ok(string taskType) => new() { Success = true, TaskType = taskType };
    public static TaskSchedulerResult Fail(string error) => new() { Success = false, ErrorMessage = error };
}

/// <summary>
/// Service for managing Windows Task Scheduler tasks to ensure the client starts on boot.
/// Tries admin-level task first (SYSTEM account, boot trigger), falls back to user-level task.
/// </summary>
public static class TaskSchedulerService
{
    public const string TaskName = "TorGames Client";
    public const string InstallerTaskName = "TorGames Installer";
    public const string TaskFolder = "\\TorGames";
    private const string FolderName = "TorGames";

    /// <summary>
    /// Ensures a startup task exists for the specified executable.
    /// Tries TaskScheduler library first, falls back to schtasks.exe if library fails (e.g., due to trimming).
    /// </summary>
    /// <param name="executablePath">Full path to the client executable.</param>
    /// <returns>Result with success status and detailed error message if failed.</returns>
    public static TaskSchedulerResult EnsureStartupTask(string executablePath)
    {
        // First try using the TaskScheduler library
        var libraryResult = EnsureStartupTaskViaLibrary(executablePath);
        if (libraryResult.Success)
            return libraryResult;

        // If library failed (likely due to trimming), fall back to schtasks.exe
        var schtasksResult = EnsureStartupTaskViaSchtasks(executablePath);
        if (schtasksResult.Success)
            return schtasksResult;

        // Both methods failed - return combined error
        return TaskSchedulerResult.Fail($"Library: {libraryResult.ErrorMessage}; Schtasks: {schtasksResult.ErrorMessage}");
    }

    /// <summary>
    /// Try to create task using the TaskScheduler library (may fail if trimmed).
    /// </summary>
    private static TaskSchedulerResult EnsureStartupTaskViaLibrary(string executablePath)
    {
        string? adminError = null;
        string? userError = null;

        try
        {
            // Check if task already exists with correct path
            var existingPath = GetExistingTaskPath();
            if (existingPath != null)
            {
                if (string.Equals(existingPath, executablePath, StringComparison.OrdinalIgnoreCase))
                {
                    // Task exists with correct path - nothing to do
                    return TaskSchedulerResult.Ok("Existing");
                }

                // Task exists but with wrong path - update it
                var updateResult = UpdateTaskPathWithResult(executablePath);
                if (updateResult.Success)
                    return updateResult;
                return TaskSchedulerResult.Fail($"Task exists but update failed: {updateResult.ErrorMessage}");
            }

            // Try admin task first (requires elevated privileges)
            var adminResult = CreateAdminTaskWithResult(executablePath);
            if (adminResult.Success)
            {
                return adminResult;
            }
            adminError = adminResult.ErrorMessage;

            // Fall back to user task (no admin required)
            var userResult = CreateUserTaskWithResult(executablePath);
            if (userResult.Success)
            {
                return userResult;
            }
            userError = userResult.ErrorMessage;

            return TaskSchedulerResult.Fail($"Admin: {adminError}; User: {userError}");
        }
        catch (TypeInitializationException ex)
        {
            // This happens when trimming removes required types
            return TaskSchedulerResult.Fail($"Library initialization failed (trimmed?): {ex.InnerException?.Message ?? ex.Message}");
        }
        catch (Exception ex)
        {
            return TaskSchedulerResult.Fail($"Library error: {ex.Message}");
        }
    }

    /// <summary>
    /// Create task using schtasks.exe command-line (works even when trimmed).
    /// </summary>
    private static TaskSchedulerResult EnsureStartupTaskViaSchtasks(string executablePath)
    {
        // Use a simple task name without subfolder for better compatibility
        const string taskNameWithFolder = @"\TorGames\TorGames Client";
        const string taskNameSimple = "TorGames Client";

        try
        {
            // First check if task already exists (try both names)
            if (TaskExistsViaSchtasks(taskNameWithFolder) || TaskExistsViaSchtasks(taskNameSimple))
            {
                return TaskSchedulerResult.Ok("Existing (schtasks)");
            }

            var escapedPath = executablePath.Replace("\"", "\\\"");
            string? adminError = null;
            string? userError = null;

            // Try to create admin-level task (SYSTEM account, ONSTART trigger)
            var adminArgs = $"/Create /TN \"{taskNameWithFolder}\" " +
                $"/TR \"\\\"{escapedPath}\\\"\" " +
                "/SC ONSTART /DELAY 0000:30 /RU SYSTEM /RL HIGHEST /F";

            var (adminSuccess, adminOut, adminErr) = RunSchtasks(adminArgs);
            if (adminSuccess)
            {
                return TaskSchedulerResult.Ok("Admin/Boot (schtasks)");
            }
            adminError = adminErr;

            // Admin failed, try user-level task with ONLOGON trigger
            // Note: ONLOGON doesn't support /DELAY, so we omit it
            // Try with folder first
            var userArgs = $"/Create /TN \"{taskNameWithFolder}\" " +
                $"/TR \"\\\"{escapedPath}\\\"\" " +
                "/SC ONLOGON /F";

            var (userSuccess, userOut, userErr) = RunSchtasks(userArgs);
            if (userSuccess)
            {
                return TaskSchedulerResult.Ok("User/Logon (schtasks)");
            }
            userError = userErr;

            // If folder-based task failed, try without folder (root task scheduler)
            var userArgsSimple = $"/Create /TN \"{taskNameSimple}\" " +
                $"/TR \"\\\"{escapedPath}\\\"\" " +
                "/SC ONLOGON /F";

            var (userSimpleSuccess, userSimpleOut, userSimpleErr) = RunSchtasks(userArgsSimple);
            if (userSimpleSuccess)
            {
                return TaskSchedulerResult.Ok("User/Logon (schtasks-root)");
            }

            return TaskSchedulerResult.Fail($"Admin: {adminError}; User (folder): {userError}; User (root): {userSimpleErr}");
        }
        catch (Exception ex)
        {
            return TaskSchedulerResult.Fail($"Schtasks error: {ex.Message}");
        }
    }

    /// <summary>
    /// Check if a task exists using schtasks.exe
    /// </summary>
    private static bool TaskExistsViaSchtasks(string taskName)
    {
        try
        {
            var (success, _, _) = RunSchtasks($"/Query /TN \"{taskName}\"");
            return success;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Run schtasks.exe with arguments and return result
    /// </summary>
    private static (bool success, string stdout, string stderr) RunSchtasks(string arguments)
    {
        try
        {
            using var process = new System.Diagnostics.Process();
            process.StartInfo = new System.Diagnostics.ProcessStartInfo
            {
                FileName = "schtasks.exe",
                Arguments = arguments,
                CreateNoWindow = true,
                UseShellExecute = false,
                RedirectStandardOutput = true,
                RedirectStandardError = true
            };

            process.Start();

            // Read output streams before WaitForExit to avoid deadlock
            var stdout = process.StandardOutput.ReadToEnd();
            var stderr = process.StandardError.ReadToEnd();

            process.WaitForExit(15000);

            return (process.ExitCode == 0, stdout.Trim(), stderr.Trim());
        }
        catch (Exception ex)
        {
            return (false, "", ex.Message);
        }
    }

    /// <summary>
    /// Legacy method for backwards compatibility.
    /// </summary>
    public static bool EnsureStartupTaskLegacy(string executablePath)
    {
        return EnsureStartupTask(executablePath).Success;
    }

    /// <summary>
    /// Creates an admin-level task that runs at system boot with SYSTEM account.
    /// Requires the current process to have admin privileges.
    /// </summary>
    public static bool CreateAdminTask(string executablePath)
    {
        return CreateAdminTaskWithResult(executablePath).Success;
    }

    /// <summary>
    /// Creates an admin-level task with detailed result.
    /// </summary>
    public static TaskSchedulerResult CreateAdminTaskWithResult(string executablePath)
    {
        try
        {
            using var ts = new TaskService();

            // Create or get folder
            var (folder, folderError) = GetOrCreateFolderWithError(ts);
            if (folder == null)
                return TaskSchedulerResult.Fail($"Could not create task folder: {folderError}");

            // Remove existing task if present
            RemoveExistingTask(folder);

            // Create new task definition
            var td = ts.NewTask();
            td.RegistrationInfo.Description = "TorGames Client - Device Management Service";
            td.RegistrationInfo.Author = "TorGames";

            // Run as SYSTEM with highest privileges
            td.Principal.RunLevel = TaskRunLevel.Highest;
            td.Principal.LogonType = TaskLogonType.ServiceAccount;
            td.Principal.UserId = "SYSTEM";

            // Boot trigger with 30-second delay (runs before user login)
            var bootTrigger = new BootTrigger { Delay = TimeSpan.FromSeconds(30) };
            td.Triggers.Add(bootTrigger);

            // Action - run the client
            var workingDir = Path.GetDirectoryName(executablePath);
            td.Actions.Add(new ExecAction(executablePath, null, workingDir));

            // Settings for reliability
            td.Settings.AllowDemandStart = true;
            td.Settings.StartWhenAvailable = true;
            td.Settings.DisallowStartIfOnBatteries = false;
            td.Settings.StopIfGoingOnBatteries = false;
            td.Settings.ExecutionTimeLimit = TimeSpan.Zero; // Run indefinitely
            td.Settings.MultipleInstances = TaskInstancesPolicy.IgnoreNew;
            td.Settings.RestartInterval = TimeSpan.FromMinutes(1);
            td.Settings.RestartCount = 3;

            // Register task
            folder.RegisterTaskDefinition(TaskName, td);

            return TaskSchedulerResult.Ok("Admin (SYSTEM/Boot)");
        }
        catch (UnauthorizedAccessException ex)
        {
            return TaskSchedulerResult.Fail($"Access denied (not running as admin): {ex.Message}");
        }
        catch (System.Runtime.InteropServices.COMException ex)
        {
            return TaskSchedulerResult.Fail($"COM error (Task Scheduler service issue): 0x{ex.HResult:X8} - {ex.Message}");
        }
        catch (Exception ex)
        {
            return TaskSchedulerResult.Fail($"{ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>
    /// Creates a user-level task that runs when the current user logs in.
    /// Does not require admin privileges.
    /// </summary>
    public static bool CreateUserTask(string executablePath)
    {
        return CreateUserTaskWithResult(executablePath).Success;
    }

    /// <summary>
    /// Creates a user-level task with detailed result.
    /// </summary>
    public static TaskSchedulerResult CreateUserTaskWithResult(string executablePath)
    {
        try
        {
            using var ts = new TaskService();

            // Create or get folder
            var (folder, folderError) = GetOrCreateFolderWithError(ts);
            if (folder == null)
                return TaskSchedulerResult.Fail($"Could not create task folder: {folderError}");

            // Remove existing task if present
            RemoveExistingTask(folder);

            // Create new task definition
            var td = ts.NewTask();
            td.RegistrationInfo.Description = "TorGames Client - Device Management Service";
            td.RegistrationInfo.Author = "TorGames";

            // Run as current user with limited privileges
            td.Principal.RunLevel = TaskRunLevel.LUA; // Least-privileged User Account
            td.Principal.LogonType = TaskLogonType.InteractiveToken;

            // Logon trigger (runs when user logs in)
            var logonTrigger = new LogonTrigger { Delay = TimeSpan.FromSeconds(10) };
            td.Triggers.Add(logonTrigger);

            // Action - run the client
            var workingDir = Path.GetDirectoryName(executablePath);
            td.Actions.Add(new ExecAction(executablePath, null, workingDir));

            // Settings for reliability
            td.Settings.AllowDemandStart = true;
            td.Settings.StartWhenAvailable = true;
            td.Settings.DisallowStartIfOnBatteries = false;
            td.Settings.StopIfGoingOnBatteries = false;
            td.Settings.ExecutionTimeLimit = TimeSpan.Zero;
            td.Settings.MultipleInstances = TaskInstancesPolicy.IgnoreNew;
            td.Settings.RestartInterval = TimeSpan.FromMinutes(1);
            td.Settings.RestartCount = 3;

            // Register task
            folder.RegisterTaskDefinition(TaskName, td);

            return TaskSchedulerResult.Ok("User (Logon)");
        }
        catch (UnauthorizedAccessException ex)
        {
            return TaskSchedulerResult.Fail($"Access denied: {ex.Message}");
        }
        catch (System.Runtime.InteropServices.COMException ex)
        {
            return TaskSchedulerResult.Fail($"COM error (Task Scheduler service issue): 0x{ex.HResult:X8} - {ex.Message}");
        }
        catch (Exception ex)
        {
            return TaskSchedulerResult.Fail($"{ex.GetType().Name}: {ex.Message}");
        }
    }

    /// <summary>
    /// Checks if the startup task already exists.
    /// </summary>
    public static bool TaskExists()
    {
        try
        {
            using var ts = new TaskService();
            var folder = ts.GetFolder(TaskFolder);
            return folder?.Tasks.Any(t => t.Name == TaskName) ?? false;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Gets the executable path from the existing task, or null if task doesn't exist.
    /// </summary>
    public static string? GetExistingTaskPath()
    {
        try
        {
            using var ts = new TaskService();
            var folder = ts.GetFolder(TaskFolder);
            var task = folder?.Tasks.FirstOrDefault(t => t.Name == TaskName);

            if (task?.Definition.Actions.FirstOrDefault() is ExecAction action)
            {
                return action.Path;
            }

            return null;
        }
        catch
        {
            return null;
        }
    }

    /// <summary>
    /// Removes the startup task if it exists.
    /// </summary>
    public static bool RemoveTask()
    {
        try
        {
            using var ts = new TaskService();
            var folder = ts.GetFolder(TaskFolder);

            if (folder == null) return true; // Folder doesn't exist, nothing to remove

            if (folder.Tasks.Any(t => t.Name == TaskName))
            {
                folder.DeleteTask(TaskName, false);
            }

            // Try to remove the folder if empty
            if (!folder.Tasks.Any())
            {
                try
                {
                    ts.RootFolder.DeleteFolder(FolderName, false);
                }
                catch
                {
                    // Folder deletion is optional, ignore errors
                }
            }

            return true;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Removes the installer startup task if it exists.
    /// Should be called by the Client on startup to clean up the installer.
    /// </summary>
    public static bool RemoveInstallerTask()
    {
        // Try using schtasks.exe first (works even when trimmed)
        var schtasksResult = RemoveInstallerTaskViaSchtasks();
        if (schtasksResult) return true;

        // Fall back to TaskScheduler library
        try
        {
            using var ts = new TaskService();
            var folder = ts.GetFolder(TaskFolder);

            if (folder == null) return true; // Folder doesn't exist, nothing to remove

            if (folder.Tasks.Any(t => t.Name == InstallerTaskName))
            {
                folder.DeleteTask(InstallerTaskName, false);
                return true;
            }

            return true; // Task doesn't exist, that's fine
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Remove installer task using schtasks.exe command-line.
    /// </summary>
    private static bool RemoveInstallerTaskViaSchtasks()
    {
        try
        {
            var (success, _, _) = RunSchtasks($"/Delete /TN \"{TaskFolder}\\{InstallerTaskName}\" /F");
            return success;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Updates the executable path in the existing task.
    /// </summary>
    public static bool UpdateTaskPath(string newExecutablePath)
    {
        return UpdateTaskPathWithResult(newExecutablePath).Success;
    }

    /// <summary>
    /// Updates the executable path with detailed result.
    /// </summary>
    public static TaskSchedulerResult UpdateTaskPathWithResult(string newExecutablePath)
    {
        try
        {
            // Remove existing task
            RemoveTask();

            // Try admin task first
            var adminResult = CreateAdminTaskWithResult(newExecutablePath);
            if (adminResult.Success)
                return adminResult;

            // Fall back to user task
            var userResult = CreateUserTaskWithResult(newExecutablePath);
            if (userResult.Success)
                return userResult;

            return TaskSchedulerResult.Fail($"Admin: {adminResult.ErrorMessage}; User: {userResult.ErrorMessage}");
        }
        catch (Exception ex)
        {
            return TaskSchedulerResult.Fail($"Update failed: {ex.Message}");
        }
    }

    private static TaskFolder? GetOrCreateFolder(TaskService ts)
    {
        return GetOrCreateFolderWithError(ts).folder;
    }

    private static (TaskFolder? folder, string? error) GetOrCreateFolderWithError(TaskService ts)
    {
        try
        {
            // Check if folder exists
            if (ts.RootFolder.SubFolders.Any(f => f.Name == FolderName))
            {
                var existing = ts.GetFolder(TaskFolder);
                return (existing, existing == null ? "Folder exists but could not be retrieved" : null);
            }

            // Create folder
            var created = ts.RootFolder.CreateFolder(FolderName);
            return (created, created == null ? "CreateFolder returned null" : null);
        }
        catch (UnauthorizedAccessException ex)
        {
            // If we can't create a subfolder, try using root folder
            try
            {
                return (ts.RootFolder, null);
            }
            catch
            {
                return (null, $"Cannot create folder and root folder not accessible: {ex.Message}");
            }
        }
        catch (Exception ex)
        {
            // If we can't create a subfolder, try using root folder
            try
            {
                return (ts.RootFolder, null);
            }
            catch
            {
                return (null, $"Cannot create folder: {ex.Message}");
            }
        }
    }

    private static void RemoveExistingTask(TaskFolder folder)
    {
        try
        {
            if (folder.Tasks.Any(t => t.Name == TaskName))
            {
                folder.DeleteTask(TaskName, false);
            }
        }
        catch
        {
            // Ignore - will fail on register if task exists
        }
    }
}
