using System.Diagnostics;
using System.Management;
using System.Net.NetworkInformation;
using System.Net.Sockets;
using System.Runtime.InteropServices;
using TorGames.Common.Protos;

namespace TorGames.Client.Services;

/// <summary>
/// Collects detailed system information for the Client Manager view.
/// </summary>
public static class SystemInfoCollector
{
    /// <summary>
    /// Collects all detailed system information.
    /// </summary>
    public static DetailedSystemInfo CollectAll()
    {
        var info = new DetailedSystemInfo
        {
            Cpu = CollectCpuInfo(),
            Memory = CollectMemoryInfo(),
            Os = CollectOsInfo(),
            Hardware = CollectHardwareInfo(),
            Performance = CollectPerformanceInfo()
        };

        // Add disks
        foreach (var disk in CollectDiskInfo())
        {
            info.Disks.Add(disk);
        }

        // Add network adapters
        foreach (var adapter in CollectNetworkAdapterInfo())
        {
            info.NetworkAdapters.Add(adapter);
        }

        // Add GPUs
        foreach (var gpu in CollectGpuInfo())
        {
            info.Gpus.Add(gpu);
        }

        return info;
    }

    private static CpuInfo CollectCpuInfo()
    {
        var cpu = new CpuInfo
        {
            Cores = Environment.ProcessorCount,
            LogicalProcessors = Environment.ProcessorCount,
            Architecture = RuntimeInformation.ProcessArchitecture.ToString()
        };

        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_Processor");
            foreach (var obj in searcher.Get())
            {
                cpu.Name = obj["Name"]?.ToString() ?? "Unknown";
                cpu.Manufacturer = obj["Manufacturer"]?.ToString() ?? "Unknown";
                cpu.Cores = Convert.ToInt32(obj["NumberOfCores"] ?? cpu.Cores);
                cpu.LogicalProcessors = Convert.ToInt32(obj["NumberOfLogicalProcessors"] ?? cpu.LogicalProcessors);
                cpu.MaxClockSpeedMhz = Convert.ToInt32(obj["MaxClockSpeed"] ?? 0);
                cpu.CurrentClockSpeedMhz = Convert.ToInt32(obj["CurrentClockSpeed"] ?? 0);
                cpu.L2CacheKb = Convert.ToInt32(obj["L2CacheSize"] ?? 0);
                cpu.L3CacheKb = Convert.ToInt32(obj["L3CacheSize"] ?? 0);
                break; // Usually only one CPU
            }
        }
        catch
        {
            cpu.Name = "Unknown (WMI unavailable)";
        }

        return cpu;
    }

    private static MemoryInfo CollectMemoryInfo()
    {
        var memory = new MemoryInfo();

        try
        {
            var gcMemory = GC.GetGCMemoryInfo();
            memory.TotalPhysicalBytes = gcMemory.TotalAvailableMemoryBytes;
            memory.AvailablePhysicalBytes = gcMemory.TotalAvailableMemoryBytes - GC.GetTotalMemory(false);

            using var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_OperatingSystem");
            foreach (var obj in searcher.Get())
            {
                memory.TotalPhysicalBytes = Convert.ToInt64(obj["TotalVisibleMemorySize"] ?? 0) * 1024;
                memory.AvailablePhysicalBytes = Convert.ToInt64(obj["FreePhysicalMemory"] ?? 0) * 1024;
                memory.TotalVirtualBytes = Convert.ToInt64(obj["TotalVirtualMemorySize"] ?? 0) * 1024;
                memory.AvailableVirtualBytes = Convert.ToInt64(obj["FreeVirtualMemory"] ?? 0) * 1024;
                break;
            }

            if (memory.TotalPhysicalBytes > 0)
            {
                memory.MemoryLoadPercent = (int)((memory.TotalPhysicalBytes - memory.AvailablePhysicalBytes) * 100 / memory.TotalPhysicalBytes);
            }

            // Get RAM details
            using var ramSearcher = new ManagementObjectSearcher("SELECT * FROM Win32_PhysicalMemory");
            var slotCount = 0;
            foreach (var obj in ramSearcher.Get())
            {
                slotCount++;
                if (memory.SpeedMhz == 0)
                {
                    memory.SpeedMhz = Convert.ToInt32(obj["Speed"] ?? 0);
                    var memType = Convert.ToInt32(obj["SMBIOSMemoryType"] ?? 0);
                    memory.MemoryType = memType switch
                    {
                        20 => "DDR",
                        21 => "DDR2",
                        22 => "DDR2 FB-DIMM",
                        24 => "DDR3",
                        26 => "DDR4",
                        34 => "DDR5",
                        _ => $"Type {memType}"
                    };
                }
            }
            memory.SlotCount = slotCount;
        }
        catch
        {
            // Use fallback values from GC
            var gcMemory = GC.GetGCMemoryInfo();
            memory.TotalPhysicalBytes = gcMemory.TotalAvailableMemoryBytes;
        }

        return memory;
    }

    private static List<DiskInfo> CollectDiskInfo()
    {
        var disks = new List<DiskInfo>();

        try
        {
            foreach (var drive in DriveInfo.GetDrives())
            {
                if (!drive.IsReady) continue;

                var disk = new DiskInfo
                {
                    Name = drive.Name,
                    DriveLetter = drive.Name.TrimEnd('\\', '/'),
                    VolumeLabel = drive.VolumeLabel,
                    FileSystem = drive.DriveFormat,
                    TotalBytes = drive.TotalSize,
                    FreeBytes = drive.AvailableFreeSpace,
                    DriveType = drive.DriveType.ToString(),
                    IsSystemDrive = drive.Name.Equals(Path.GetPathRoot(Environment.SystemDirectory), StringComparison.OrdinalIgnoreCase)
                };

                disks.Add(disk);
            }
        }
        catch
        {
            // Ignore errors
        }

        return disks;
    }

    private static List<NetworkAdapterInfo> CollectNetworkAdapterInfo()
    {
        var adapters = new List<NetworkAdapterInfo>();

        try
        {
            foreach (var nic in NetworkInterface.GetAllNetworkInterfaces())
            {
                if (nic.NetworkInterfaceType == NetworkInterfaceType.Loopback) continue;

                var adapter = new NetworkAdapterInfo
                {
                    Name = nic.Name,
                    Description = nic.Description,
                    MacAddress = nic.GetPhysicalAddress().ToString(),
                    SpeedBps = nic.Speed,
                    Status = nic.OperationalStatus.ToString(),
                    AdapterType = nic.NetworkInterfaceType.ToString()
                };

                var props = nic.GetIPProperties();

                // Get IP address
                var ipv4 = props.UnicastAddresses
                    .FirstOrDefault(a => a.Address.AddressFamily == AddressFamily.InterNetwork);
                if (ipv4 != null)
                {
                    adapter.IpAddress = ipv4.Address.ToString();
                    adapter.SubnetMask = ipv4.IPv4Mask?.ToString() ?? "";
                }

                // Get gateway
                var gateway = props.GatewayAddresses
                    .FirstOrDefault(g => g.Address.AddressFamily == AddressFamily.InterNetwork);
                if (gateway != null)
                {
                    adapter.DefaultGateway = gateway.Address.ToString();
                }

                // Get DNS servers
                var dnsServers = props.DnsAddresses
                    .Where(d => d.AddressFamily == AddressFamily.InterNetwork)
                    .Select(d => d.ToString());
                adapter.DnsServers = string.Join(", ", dnsServers);

                // DHCP enabled
                try
                {
                    adapter.IsDhcpEnabled = props.GetIPv4Properties()?.IsDhcpEnabled ?? false;
                }
                catch { }

                adapters.Add(adapter);
            }
        }
        catch
        {
            // Ignore errors
        }

        return adapters;
    }

    private static List<GpuInfo> CollectGpuInfo()
    {
        var gpus = new List<GpuInfo>();

        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_VideoController");
            foreach (var obj in searcher.Get())
            {
                var gpu = new GpuInfo
                {
                    Name = obj["Name"]?.ToString() ?? "Unknown",
                    Manufacturer = obj["AdapterCompatibility"]?.ToString() ?? "Unknown",
                    VideoMemoryBytes = Convert.ToInt64(obj["AdapterRAM"] ?? 0),
                    DriverVersion = obj["DriverVersion"]?.ToString() ?? "",
                    CurrentRefreshRate = Convert.ToInt32(obj["CurrentRefreshRate"] ?? 0),
                    VideoProcessor = obj["VideoProcessor"]?.ToString() ?? ""
                };

                var hRes = Convert.ToInt32(obj["CurrentHorizontalResolution"] ?? 0);
                var vRes = Convert.ToInt32(obj["CurrentVerticalResolution"] ?? 0);
                if (hRes > 0 && vRes > 0)
                {
                    gpu.Resolution = $"{hRes}x{vRes}";
                }

                gpus.Add(gpu);
            }
        }
        catch
        {
            // Ignore errors
        }

        return gpus;
    }

    private static OsInfo CollectOsInfo()
    {
        var os = new OsInfo
        {
            Name = RuntimeInformation.OSDescription,
            Architecture = Environment.Is64BitOperatingSystem ? "x64" : "x86",
            SystemDirectory = Environment.SystemDirectory,
            TimezoneOffsetMinutes = (int)TimeZoneInfo.Local.BaseUtcOffset.TotalMinutes,
            TimezoneName = TimeZoneInfo.Local.DisplayName
        };

        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_OperatingSystem");
            foreach (var obj in searcher.Get())
            {
                os.Name = obj["Caption"]?.ToString() ?? os.Name;
                os.Version = obj["Version"]?.ToString() ?? "";
                os.BuildNumber = obj["BuildNumber"]?.ToString() ?? "";
                os.SerialNumber = obj["SerialNumber"]?.ToString() ?? "";
                os.RegisteredUser = obj["RegisteredUser"]?.ToString() ?? "";

                var installDate = obj["InstallDate"]?.ToString();
                if (!string.IsNullOrEmpty(installDate))
                {
                    os.InstallDate = ManagementDateTimeConverter.ToDateTime(installDate).ToString("yyyy-MM-dd HH:mm:ss");
                }

                var lastBoot = obj["LastBootUpTime"]?.ToString();
                if (!string.IsNullOrEmpty(lastBoot))
                {
                    os.LastBootTime = ManagementDateTimeConverter.ToDateTime(lastBoot).ToString("yyyy-MM-dd HH:mm:ss");
                }

                break;
            }
        }
        catch
        {
            os.Version = Environment.OSVersion.Version.ToString();
        }

        return os;
    }

    private static SystemHardwareInfo CollectHardwareInfo()
    {
        var hardware = new SystemHardwareInfo();

        try
        {
            // Computer System
            using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_ComputerSystem"))
            {
                foreach (var obj in searcher.Get())
                {
                    hardware.Manufacturer = obj["Manufacturer"]?.ToString() ?? "";
                    hardware.Model = obj["Model"]?.ToString() ?? "";
                    hardware.SystemType = obj["SystemType"]?.ToString() ?? "";
                    break;
                }
            }

            // BIOS
            using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_BIOS"))
            {
                foreach (var obj in searcher.Get())
                {
                    hardware.BiosVersion = obj["SMBIOSBIOSVersion"]?.ToString() ?? "";
                    hardware.BiosManufacturer = obj["Manufacturer"]?.ToString() ?? "";
                    var releaseDate = obj["ReleaseDate"]?.ToString();
                    if (!string.IsNullOrEmpty(releaseDate))
                    {
                        hardware.BiosReleaseDate = ManagementDateTimeConverter.ToDateTime(releaseDate).ToString("yyyy-MM-dd");
                    }
                    break;
                }
            }

            // Motherboard
            using (var searcher = new ManagementObjectSearcher("SELECT * FROM Win32_BaseBoard"))
            {
                foreach (var obj in searcher.Get())
                {
                    hardware.MotherboardManufacturer = obj["Manufacturer"]?.ToString() ?? "";
                    hardware.MotherboardProduct = obj["Product"]?.ToString() ?? "";
                    hardware.MotherboardSerial = obj["SerialNumber"]?.ToString() ?? "";
                    break;
                }
            }

            // UUID
            using (var searcher = new ManagementObjectSearcher("SELECT UUID FROM Win32_ComputerSystemProduct"))
            {
                foreach (var obj in searcher.Get())
                {
                    hardware.Uuid = obj["UUID"]?.ToString() ?? "";
                    break;
                }
            }
        }
        catch
        {
            // Ignore errors
        }

        return hardware;
    }

    private static PerformanceInfo CollectPerformanceInfo()
    {
        var perf = new PerformanceInfo
        {
            UptimeSeconds = Environment.TickCount64 / 1000
        };

        try
        {
            var gcMemory = GC.GetGCMemoryInfo();
            perf.AvailableMemoryBytes = gcMemory.TotalAvailableMemoryBytes - GC.GetTotalMemory(false);

            // Get process/thread counts
            var processes = Process.GetProcesses();
            perf.ProcessCount = processes.Length;
            perf.ThreadCount = processes.Sum(p =>
            {
                try { return p.Threads.Count; }
                catch { return 0; }
            });
            perf.HandleCount = processes.Sum(p =>
            {
                try { return p.HandleCount; }
                catch { return 0; }
            });

            // Get top 10 processes by memory
            var topProcesses = processes
                .Select(p =>
                {
                    try
                    {
                        return new ProcessInfo
                        {
                            Pid = p.Id,
                            Name = p.ProcessName,
                            MemoryBytes = p.WorkingSet64,
                            CpuPercent = 0 // Would need PerformanceCounter for accurate CPU
                        };
                    }
                    catch
                    {
                        return null;
                    }
                })
                .Where(p => p != null)
                .OrderByDescending(p => p!.MemoryBytes)
                .Take(10);

            foreach (var proc in topProcesses)
            {
                if (proc != null) perf.TopProcesses.Add(proc);
            }

            // CPU usage is collected through WMI if available
            try
            {
                using var searcher = new ManagementObjectSearcher("SELECT LoadPercentage FROM Win32_Processor");
                foreach (var obj in searcher.Get())
                {
                    perf.CpuUsagePercent = Convert.ToDouble(obj["LoadPercentage"] ?? 0);
                    break;
                }
            }
            catch
            {
                // Ignore - CPU usage not critical
            }
        }
        catch
        {
            // Ignore errors
        }

        return perf;
    }
}
