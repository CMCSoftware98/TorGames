using System.Diagnostics;
using System.Net.NetworkInformation;
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

    // Network I/O tracking
    private static DateTime _lastNetworkCheck = DateTime.UtcNow;
    private static long _lastBytesReceived = 0;
    private static long _lastBytesSent = 0;
    private static long _networkBytesInPerSec = 0;
    private static long _networkBytesOutPerSec = 0;

    public ServerStatsController(
        ClientManager clientManager,
        ILogger<ServerStatsController> logger)
    {
        _clientManager = clientManager;
        _logger = logger;
    }

    /// <summary>
    /// Gets current server statistics including CPU, RAM, disk usage, network I/O, and connected clients.
    /// </summary>
    [HttpGet]
    public ActionResult<ServerStatsDto> GetStats()
    {
        try
        {
            // Update network stats
            UpdateNetworkStats();

            var stats = new ServerStatsDto
            {
                CpuUsagePercent = GetCpuUsage(),
                MemoryUsedBytes = GetUsedMemory(),
                MemoryTotalBytes = GetTotalMemory(),
                DiskUsedBytes = GetDiskUsed(),
                DiskTotalBytes = GetDiskTotal(),
                NetworkBytesInPerSec = _networkBytesInPerSec,
                NetworkBytesOutPerSec = _networkBytesOutPerSec,
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

    private static void UpdateNetworkStats()
    {
        try
        {
            var now = DateTime.UtcNow;
            var timeDiffSeconds = (now - _lastNetworkCheck).TotalSeconds;

            if (timeDiffSeconds < 0.1) return; // Avoid division by zero or too frequent updates

            // Get all network interfaces and sum their statistics
            var interfaces = NetworkInterface.GetAllNetworkInterfaces()
                .Where(ni => ni.OperationalStatus == OperationalStatus.Up &&
                             ni.NetworkInterfaceType != NetworkInterfaceType.Loopback)
                .ToList();

            long totalBytesReceived = 0;
            long totalBytesSent = 0;

            foreach (var ni in interfaces)
            {
                try
                {
                    var stats = ni.GetIPv4Statistics();
                    totalBytesReceived += stats.BytesReceived;
                    totalBytesSent += stats.BytesSent;
                }
                catch
                {
                    // Some interfaces may not support IPv4 statistics
                }
            }

            // Calculate bytes per second
            if (_lastBytesReceived > 0 && _lastBytesSent > 0)
            {
                var bytesReceivedDiff = totalBytesReceived - _lastBytesReceived;
                var bytesSentDiff = totalBytesSent - _lastBytesSent;

                // Only update if positive (handles counter resets)
                if (bytesReceivedDiff >= 0 && bytesSentDiff >= 0)
                {
                    _networkBytesInPerSec = (long)(bytesReceivedDiff / timeDiffSeconds);
                    _networkBytesOutPerSec = (long)(bytesSentDiff / timeDiffSeconds);
                }
            }

            _lastBytesReceived = totalBytesReceived;
            _lastBytesSent = totalBytesSent;
            _lastNetworkCheck = now;
        }
        catch
        {
            // Network stats unavailable
            _networkBytesInPerSec = 0;
            _networkBytesOutPerSec = 0;
        }
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
    public long NetworkBytesInPerSec { get; set; }
    public long NetworkBytesOutPerSec { get; set; }
    public int ConnectedClients { get; set; }
    public int TotalClients { get; set; }
    public long UptimeSeconds { get; set; }
    public DateTime Timestamp { get; set; }
}
