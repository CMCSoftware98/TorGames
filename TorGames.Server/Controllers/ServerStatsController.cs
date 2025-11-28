using System.Diagnostics;
using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using TorGames.Server.Services;

namespace TorGames.Server.Controllers;

/// <summary>
/// Controller for server statistics and resource monitoring.
/// </summary>
[ApiController]
[Route("api/[controller]")]
[Authorize]
public class ServerStatsController : ControllerBase
{
    private readonly ClientManager _clientManager;
    private readonly ILogger<ServerStatsController> _logger;
    private static readonly Process CurrentProcess = Process.GetCurrentProcess();
    private static DateTime _lastCpuCheck = DateTime.UtcNow;
    private static TimeSpan _lastTotalProcessorTime = TimeSpan.Zero;
    private static double _lastCpuUsage = 0;

    public ServerStatsController(
        ClientManager clientManager,
        ILogger<ServerStatsController> logger)
    {
        _clientManager = clientManager;
        _logger = logger;
    }

    /// <summary>
    /// Gets current server statistics including CPU, RAM, disk usage, and connected clients.
    /// </summary>
    [HttpGet]
    public ActionResult<ServerStatsDto> GetStats()
    {
        try
        {
            var stats = new ServerStatsDto
            {
                CpuUsagePercent = GetCpuUsage(),
                MemoryUsedBytes = GetUsedMemory(),
                MemoryTotalBytes = GetTotalMemory(),
                DiskUsedBytes = GetDiskUsed(),
                DiskTotalBytes = GetDiskTotal(),
                ConnectedClients = _clientManager.GetAllClients().Count(c => c.IsOnline),
                TotalClients = _clientManager.GetAllClients().Count(),
                UptimeSeconds = (long)(DateTime.UtcNow - CurrentProcess.StartTime.ToUniversalTime()).TotalSeconds,
                Timestamp = DateTime.UtcNow
            };

            return Ok(stats);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error getting server stats");
            return StatusCode(500, "Failed to retrieve server statistics");
        }
    }

    private static double GetCpuUsage()
    {
        try
        {
            var now = DateTime.UtcNow;
            var currentTotalProcessorTime = CurrentProcess.TotalProcessorTime;

            var timeDiff = (now - _lastCpuCheck).TotalMilliseconds;
            if (timeDiff > 0)
            {
                var cpuTimeDiff = (currentTotalProcessorTime - _lastTotalProcessorTime).TotalMilliseconds;
                _lastCpuUsage = (cpuTimeDiff / (Environment.ProcessorCount * timeDiff)) * 100;
            }

            _lastCpuCheck = now;
            _lastTotalProcessorTime = currentTotalProcessorTime;

            return Math.Round(Math.Min(100, Math.Max(0, _lastCpuUsage)), 1);
        }
        catch
        {
            return 0;
        }
    }

    private static long GetUsedMemory()
    {
        try
        {
            // Get system memory info on Linux
            if (OperatingSystem.IsLinux())
            {
                var memInfo = System.IO.File.ReadAllText("/proc/meminfo");
                var lines = memInfo.Split('\n');

                long totalKb = 0;
                long availableKb = 0;

                foreach (var line in lines)
                {
                    if (line.StartsWith("MemTotal:"))
                    {
                        totalKb = ParseMemInfoValue(line);
                    }
                    else if (line.StartsWith("MemAvailable:"))
                    {
                        availableKb = ParseMemInfoValue(line);
                    }
                }

                return (totalKb - availableKb) * 1024;
            }

            // Fallback to process memory
            return CurrentProcess.WorkingSet64;
        }
        catch
        {
            return CurrentProcess.WorkingSet64;
        }
    }

    private static long GetTotalMemory()
    {
        try
        {
            if (OperatingSystem.IsLinux())
            {
                var memInfo = System.IO.File.ReadAllText("/proc/meminfo");
                var lines = memInfo.Split('\n');

                foreach (var line in lines)
                {
                    if (line.StartsWith("MemTotal:"))
                    {
                        return ParseMemInfoValue(line) * 1024;
                    }
                }
            }

            return GC.GetGCMemoryInfo().TotalAvailableMemoryBytes;
        }
        catch
        {
            return GC.GetGCMemoryInfo().TotalAvailableMemoryBytes;
        }
    }

    private static long ParseMemInfoValue(string line)
    {
        // Format: "MemTotal:       16384000 kB"
        var parts = line.Split(new[] { ' ' }, StringSplitOptions.RemoveEmptyEntries);
        if (parts.Length >= 2 && long.TryParse(parts[1], out var value))
        {
            return value;
        }
        return 0;
    }

    private static long GetDiskUsed()
    {
        try
        {
            var drive = GetPrimaryDrive();
            return drive.TotalSize - drive.AvailableFreeSpace;
        }
        catch
        {
            return 0;
        }
    }

    private static long GetDiskTotal()
    {
        try
        {
            return GetPrimaryDrive().TotalSize;
        }
        catch
        {
            return 0;
        }
    }

    private static DriveInfo GetPrimaryDrive()
    {
        // On Linux, use root drive. On Windows, use C:
        var drives = DriveInfo.GetDrives()
            .Where(d => d.IsReady && d.DriveType == DriveType.Fixed)
            .ToList();

        return drives.FirstOrDefault(d => d.Name == "/" || d.Name.StartsWith("C"))
               ?? drives.FirstOrDefault()
               ?? new DriveInfo("/");
    }
}

/// <summary>
/// Server statistics data transfer object.
/// </summary>
public class ServerStatsDto
{
    public double CpuUsagePercent { get; set; }
    public long MemoryUsedBytes { get; set; }
    public long MemoryTotalBytes { get; set; }
    public long DiskUsedBytes { get; set; }
    public long DiskTotalBytes { get; set; }
    public int ConnectedClients { get; set; }
    public int TotalClients { get; set; }
    public long UptimeSeconds { get; set; }
    public DateTime Timestamp { get; set; }
}
