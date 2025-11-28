using System;
using System.IO;
using System.Linq;
using Microsoft.Win32.TaskScheduler;

namespace TorGames.Common.Services;

/// <summary>
/// Service for managing Windows Task Scheduler tasks to ensure the client starts on boot.
/// Tries admin-level task first (SYSTEM account, boot trigger), falls back to user-level task.
/// </summary>
public static class TaskSchedulerService
{
    public const string TaskName = "TorGames Client";
    public const string TaskFolder = "\\TorGames";
    private const string FolderName = "TorGames";

    /// <summary>
    /// Ensures a startup task exists for the specified executable.
    /// Tries admin task first, falls back to user task if admin fails.
    /// </summary>
    /// <param name="executablePath">Full path to the client executable.</param>
    /// <returns>True if task was created or already exists with correct path.</returns>
    public static bool EnsureStartupTask(string executablePath)
    {
        try
        {
            // Check if task already exists with correct path
            var existingPath = GetExistingTaskPath();
            if (existingPath != null)
            {
                if (string.Equals(existingPath, executablePath, StringComparison.OrdinalIgnoreCase))
                {
                    // Task exists with correct path - nothing to do
                    return true;
                }

                // Task exists but with wrong path - update it
                return UpdateTaskPath(executablePath);
            }

            // Try admin task first (requires elevated privileges)
            if (CreateAdminTask(executablePath))
            {
                return true;
            }

            // Fall back to user task (no admin required)
            return CreateUserTask(executablePath);
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Creates an admin-level task that runs at system boot with SYSTEM account.
    /// Requires the current process to have admin privileges.
    /// </summary>
    public static bool CreateAdminTask(string executablePath)
    {
        try
        {
            using var ts = new TaskService();

            // Create or get folder
            var folder = GetOrCreateFolder(ts);
            if (folder == null) return false;

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

            return true;
        }
        catch (UnauthorizedAccessException)
        {
            // Need admin privileges - expected failure, will fall back to user task
            return false;
        }
        catch
        {
            return false;
        }
    }

    /// <summary>
    /// Creates a user-level task that runs when the current user logs in.
    /// Does not require admin privileges.
    /// </summary>
    public static bool CreateUserTask(string executablePath)
    {
        try
        {
            using var ts = new TaskService();

            // Create or get folder
            var folder = GetOrCreateFolder(ts);
            if (folder == null) return false;

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

            return true;
        }
        catch
        {
            return false;
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
    /// Updates the executable path in the existing task.
    /// </summary>
    public static bool UpdateTaskPath(string newExecutablePath)
    {
        try
        {
            // Remove existing and recreate with new path
            // This is more reliable than trying to modify the existing task
            RemoveTask();
            return EnsureStartupTask(newExecutablePath);
        }
        catch
        {
            return false;
        }
    }

    private static TaskFolder? GetOrCreateFolder(TaskService ts)
    {
        try
        {
            // Check if folder exists
            if (ts.RootFolder.SubFolders.Any(f => f.Name == FolderName))
            {
                return ts.GetFolder(TaskFolder);
            }

            // Create folder
            return ts.RootFolder.CreateFolder(FolderName);
        }
        catch
        {
            // If we can't create a subfolder, try using root folder
            return ts.RootFolder;
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
