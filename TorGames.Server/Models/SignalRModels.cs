using TorGames.Database.Entities;

namespace TorGames.Server.Models;

/// <summary>
/// DTO for client information sent to web app via SignalR.
/// </summary>
public class ClientDto
{
    public string ConnectionKey { get; set; } = string.Empty;
    public string ClientId { get; set; } = string.Empty;
    public string ClientType { get; set; } = string.Empty;
    public string MachineName { get; set; } = string.Empty;
    public string OsVersion { get; set; } = string.Empty;
    public string OsArchitecture { get; set; } = string.Empty;
    public int CpuCount { get; set; }
    public long TotalMemoryBytes { get; set; }
    public string Username { get; set; } = string.Empty;
    public string ClientVersion { get; set; } = string.Empty;
    public string IpAddress { get; set; } = string.Empty;
    public string MacAddress { get; set; } = string.Empty;
    public bool IsAdmin { get; set; }
    public string CountryCode { get; set; } = string.Empty;
    public bool IsUacEnabled { get; set; } = true;
    public DateTime ConnectedAt { get; set; }
    public DateTime LastHeartbeat { get; set; }
    public bool IsOnline { get; set; }
    public double CpuUsagePercent { get; set; }
    public long AvailableMemoryBytes { get; set; }
    public long UptimeSeconds { get; set; }

    // Database fields for persistent data
    public DateTime FirstSeenAt { get; set; }
    public DateTime LastSeenAt { get; set; }
    public int TotalConnections { get; set; }
    public bool IsFlagged { get; set; }
    public bool IsBlocked { get; set; }
    public string? Label { get; set; }

    public static ClientDto FromConnectedClient(ConnectedClient client)
    {
        return new ClientDto
        {
            ConnectionKey = client.ConnectionKey,
            ClientId = client.ClientId,
            ClientType = client.ClientType,
            MachineName = client.MachineName,
            OsVersion = client.OsVersion,
            OsArchitecture = client.OsArchitecture,
            CpuCount = client.CpuCount,
            TotalMemoryBytes = client.TotalMemoryBytes,
            Username = client.Username,
            ClientVersion = client.ClientVersion,
            IpAddress = client.IpAddress,
            MacAddress = client.MacAddress,
            IsAdmin = client.IsAdmin,
            CountryCode = client.CountryCode,
            IsUacEnabled = client.IsUacEnabled,
            ConnectedAt = client.ConnectedAt,
            LastHeartbeat = client.LastHeartbeat,
            IsOnline = client.IsOnline,
            CpuUsagePercent = client.CpuUsagePercent,
            AvailableMemoryBytes = client.AvailableMemoryBytes,
            UptimeSeconds = client.UptimeSeconds
        };
    }

    /// <summary>
    /// Creates a ClientDto from a database client entity.
    /// </summary>
    public static ClientDto FromDatabaseClient(Client dbClient, ConnectedClient? liveClient = null)
    {
        // If client is currently connected, merge live data
        if (liveClient != null)
        {
            var dto = FromConnectedClient(liveClient);
            dto.FirstSeenAt = dbClient.FirstSeenAt;
            dto.LastSeenAt = dbClient.LastSeenAt;
            dto.TotalConnections = dbClient.TotalConnections;
            dto.IsFlagged = dbClient.IsFlagged;
            dto.IsBlocked = dbClient.IsBlocked;
            dto.Label = dbClient.Label;
            return dto;
        }

        // Offline client from database
        return new ClientDto
        {
            ConnectionKey = $"{dbClient.ClientId}:{dbClient.ClientType}",
            ClientId = dbClient.ClientId,
            ClientType = dbClient.ClientType,
            MachineName = dbClient.MachineName,
            OsVersion = dbClient.OsVersion,
            OsArchitecture = dbClient.OsArchitecture,
            CpuCount = dbClient.CpuCount,
            TotalMemoryBytes = dbClient.TotalMemoryBytes,
            Username = dbClient.Username,
            ClientVersion = dbClient.ClientVersion,
            IpAddress = dbClient.LastIpAddress,
            MacAddress = dbClient.MacAddress,
            IsAdmin = dbClient.IsAdmin,
            CountryCode = dbClient.CountryCode,
            IsUacEnabled = dbClient.IsUacEnabled,
            ConnectedAt = dbClient.LastSeenAt,
            LastHeartbeat = dbClient.LastSeenAt,
            IsOnline = false,
            CpuUsagePercent = 0,
            AvailableMemoryBytes = 0,
            UptimeSeconds = 0,
            FirstSeenAt = dbClient.FirstSeenAt,
            LastSeenAt = dbClient.LastSeenAt,
            TotalConnections = dbClient.TotalConnections,
            IsFlagged = dbClient.IsFlagged,
            IsBlocked = dbClient.IsBlocked,
            Label = dbClient.Label
        };
    }
}

/// <summary>
/// DTO for command results sent to web app via SignalR.
/// </summary>
public class CommandResultDto
{
    public string ConnectionKey { get; set; } = string.Empty;
    public string CommandId { get; set; } = string.Empty;
    public bool Success { get; set; }
    public int ExitCode { get; set; }
    public string Stdout { get; set; } = string.Empty;
    public string Stderr { get; set; } = string.Empty;
    public string ErrorMessage { get; set; } = string.Empty;
}

/// <summary>
/// Request to execute a command on a client.
/// </summary>
public class ExecuteCommandRequest
{
    public string ConnectionKey { get; set; } = string.Empty;
    public string CommandType { get; set; } = string.Empty;
    public string CommandText { get; set; } = string.Empty;
    public int TimeoutSeconds { get; set; } = 30;
}

/// <summary>
/// DTO for detailed system information sent to web app via SignalR.
/// </summary>
public class DetailedSystemInfoDto
{
    public CpuInfoDto? Cpu { get; set; }
    public MemoryInfoDto? Memory { get; set; }
    public List<DiskInfoDto> Disks { get; set; } = new();
    public List<NetworkAdapterInfoDto> NetworkAdapters { get; set; } = new();
    public List<GpuInfoDto> Gpus { get; set; } = new();
    public OsInfoDto? Os { get; set; }
    public SystemHardwareInfoDto? Hardware { get; set; }
    public PerformanceInfoDto? Performance { get; set; }

    public static DetailedSystemInfoDto FromProto(TorGames.Common.Protos.DetailedSystemInfo info)
    {
        return new DetailedSystemInfoDto
        {
            Cpu = info.Cpu != null ? CpuInfoDto.FromProto(info.Cpu) : null,
            Memory = info.Memory != null ? MemoryInfoDto.FromProto(info.Memory) : null,
            Disks = info.Disks.Select(DiskInfoDto.FromProto).ToList(),
            NetworkAdapters = info.NetworkAdapters.Select(NetworkAdapterInfoDto.FromProto).ToList(),
            Gpus = info.Gpus.Select(GpuInfoDto.FromProto).ToList(),
            Os = info.Os != null ? OsInfoDto.FromProto(info.Os) : null,
            Hardware = info.Hardware != null ? SystemHardwareInfoDto.FromProto(info.Hardware) : null,
            Performance = info.Performance != null ? PerformanceInfoDto.FromProto(info.Performance) : null
        };
    }
}

public class CpuInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Manufacturer { get; set; } = string.Empty;
    public int Cores { get; set; }
    public int LogicalProcessors { get; set; }
    public int MaxClockSpeedMhz { get; set; }
    public int CurrentClockSpeedMhz { get; set; }
    public string Architecture { get; set; } = string.Empty;
    public int L2CacheKb { get; set; }
    public int L3CacheKb { get; set; }

    public static CpuInfoDto FromProto(TorGames.Common.Protos.CpuInfo cpu)
    {
        return new CpuInfoDto
        {
            Name = cpu.Name,
            Manufacturer = cpu.Manufacturer,
            Cores = cpu.Cores,
            LogicalProcessors = cpu.LogicalProcessors,
            MaxClockSpeedMhz = cpu.MaxClockSpeedMhz,
            CurrentClockSpeedMhz = cpu.CurrentClockSpeedMhz,
            Architecture = cpu.Architecture,
            L2CacheKb = cpu.L2CacheKb,
            L3CacheKb = cpu.L3CacheKb
        };
    }
}

public class MemoryInfoDto
{
    public long TotalPhysicalBytes { get; set; }
    public long AvailablePhysicalBytes { get; set; }
    public long TotalVirtualBytes { get; set; }
    public long AvailableVirtualBytes { get; set; }
    public int MemoryLoadPercent { get; set; }
    public int SlotCount { get; set; }
    public int SpeedMhz { get; set; }
    public string MemoryType { get; set; } = string.Empty;

    public static MemoryInfoDto FromProto(TorGames.Common.Protos.MemoryInfo mem)
    {
        return new MemoryInfoDto
        {
            TotalPhysicalBytes = mem.TotalPhysicalBytes,
            AvailablePhysicalBytes = mem.AvailablePhysicalBytes,
            TotalVirtualBytes = mem.TotalVirtualBytes,
            AvailableVirtualBytes = mem.AvailableVirtualBytes,
            MemoryLoadPercent = mem.MemoryLoadPercent,
            SlotCount = mem.SlotCount,
            SpeedMhz = mem.SpeedMhz,
            MemoryType = mem.MemoryType
        };
    }
}

public class DiskInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string DriveLetter { get; set; } = string.Empty;
    public string VolumeLabel { get; set; } = string.Empty;
    public string FileSystem { get; set; } = string.Empty;
    public long TotalBytes { get; set; }
    public long FreeBytes { get; set; }
    public string DriveType { get; set; } = string.Empty;
    public bool IsSystemDrive { get; set; }

    public static DiskInfoDto FromProto(TorGames.Common.Protos.DiskInfo disk)
    {
        return new DiskInfoDto
        {
            Name = disk.Name,
            DriveLetter = disk.DriveLetter,
            VolumeLabel = disk.VolumeLabel,
            FileSystem = disk.FileSystem,
            TotalBytes = disk.TotalBytes,
            FreeBytes = disk.FreeBytes,
            DriveType = disk.DriveType,
            IsSystemDrive = disk.IsSystemDrive
        };
    }
}

public class NetworkAdapterInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Description { get; set; } = string.Empty;
    public string MacAddress { get; set; } = string.Empty;
    public string IpAddress { get; set; } = string.Empty;
    public string SubnetMask { get; set; } = string.Empty;
    public string DefaultGateway { get; set; } = string.Empty;
    public string DnsServers { get; set; } = string.Empty;
    public long SpeedBps { get; set; }
    public string Status { get; set; } = string.Empty;
    public string AdapterType { get; set; } = string.Empty;
    public bool IsDhcpEnabled { get; set; }

    public static NetworkAdapterInfoDto FromProto(TorGames.Common.Protos.NetworkAdapterInfo adapter)
    {
        return new NetworkAdapterInfoDto
        {
            Name = adapter.Name,
            Description = adapter.Description,
            MacAddress = adapter.MacAddress,
            IpAddress = adapter.IpAddress,
            SubnetMask = adapter.SubnetMask,
            DefaultGateway = adapter.DefaultGateway,
            DnsServers = adapter.DnsServers,
            SpeedBps = adapter.SpeedBps,
            Status = adapter.Status,
            AdapterType = adapter.AdapterType,
            IsDhcpEnabled = adapter.IsDhcpEnabled
        };
    }
}

public class GpuInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Manufacturer { get; set; } = string.Empty;
    public long VideoMemoryBytes { get; set; }
    public string DriverVersion { get; set; } = string.Empty;
    public int CurrentRefreshRate { get; set; }
    public string VideoProcessor { get; set; } = string.Empty;
    public string Resolution { get; set; } = string.Empty;

    public static GpuInfoDto FromProto(TorGames.Common.Protos.GpuInfo gpu)
    {
        return new GpuInfoDto
        {
            Name = gpu.Name,
            Manufacturer = gpu.Manufacturer,
            VideoMemoryBytes = gpu.VideoMemoryBytes,
            DriverVersion = gpu.DriverVersion,
            CurrentRefreshRate = gpu.CurrentRefreshRate,
            VideoProcessor = gpu.VideoProcessor,
            Resolution = gpu.Resolution
        };
    }
}

public class OsInfoDto
{
    public string Name { get; set; } = string.Empty;
    public string Version { get; set; } = string.Empty;
    public string BuildNumber { get; set; } = string.Empty;
    public string Architecture { get; set; } = string.Empty;
    public string InstallDate { get; set; } = string.Empty;
    public string LastBootTime { get; set; } = string.Empty;
    public string SerialNumber { get; set; } = string.Empty;
    public string RegisteredUser { get; set; } = string.Empty;
    public string SystemDirectory { get; set; } = string.Empty;
    public int TimezoneOffsetMinutes { get; set; }
    public string TimezoneName { get; set; } = string.Empty;

    public static OsInfoDto FromProto(TorGames.Common.Protos.OsInfo os)
    {
        return new OsInfoDto
        {
            Name = os.Name,
            Version = os.Version,
            BuildNumber = os.BuildNumber,
            Architecture = os.Architecture,
            InstallDate = os.InstallDate,
            LastBootTime = os.LastBootTime,
            SerialNumber = os.SerialNumber,
            RegisteredUser = os.RegisteredUser,
            SystemDirectory = os.SystemDirectory,
            TimezoneOffsetMinutes = os.TimezoneOffsetMinutes,
            TimezoneName = os.TimezoneName
        };
    }
}

public class SystemHardwareInfoDto
{
    public string Manufacturer { get; set; } = string.Empty;
    public string Model { get; set; } = string.Empty;
    public string SystemType { get; set; } = string.Empty;
    public string BiosVersion { get; set; } = string.Empty;
    public string BiosManufacturer { get; set; } = string.Empty;
    public string BiosReleaseDate { get; set; } = string.Empty;
    public string MotherboardManufacturer { get; set; } = string.Empty;
    public string MotherboardProduct { get; set; } = string.Empty;
    public string MotherboardSerial { get; set; } = string.Empty;
    public string Uuid { get; set; } = string.Empty;

    public static SystemHardwareInfoDto FromProto(TorGames.Common.Protos.SystemHardwareInfo hw)
    {
        return new SystemHardwareInfoDto
        {
            Manufacturer = hw.Manufacturer,
            Model = hw.Model,
            SystemType = hw.SystemType,
            BiosVersion = hw.BiosVersion,
            BiosManufacturer = hw.BiosManufacturer,
            BiosReleaseDate = hw.BiosReleaseDate,
            MotherboardManufacturer = hw.MotherboardManufacturer,
            MotherboardProduct = hw.MotherboardProduct,
            MotherboardSerial = hw.MotherboardSerial,
            Uuid = hw.Uuid
        };
    }
}

public class PerformanceInfoDto
{
    public double CpuUsagePercent { get; set; }
    public long AvailableMemoryBytes { get; set; }
    public long UptimeSeconds { get; set; }
    public int ProcessCount { get; set; }
    public int ThreadCount { get; set; }
    public int HandleCount { get; set; }
    public List<ProcessInfoDto> TopProcesses { get; set; } = new();

    public static PerformanceInfoDto FromProto(TorGames.Common.Protos.PerformanceInfo perf)
    {
        return new PerformanceInfoDto
        {
            CpuUsagePercent = perf.CpuUsagePercent,
            AvailableMemoryBytes = perf.AvailableMemoryBytes,
            UptimeSeconds = perf.UptimeSeconds,
            ProcessCount = perf.ProcessCount,
            ThreadCount = perf.ThreadCount,
            HandleCount = perf.HandleCount,
            TopProcesses = perf.TopProcesses.Select(ProcessInfoDto.FromProto).ToList()
        };
    }
}

public class ProcessInfoDto
{
    public int Pid { get; set; }
    public string Name { get; set; } = string.Empty;
    public double CpuPercent { get; set; }
    public long MemoryBytes { get; set; }

    public static ProcessInfoDto FromProto(TorGames.Common.Protos.ProcessInfo proc)
    {
        return new ProcessInfoDto
        {
            Pid = proc.Pid,
            Name = proc.Name,
            CpuPercent = proc.CpuPercent,
            MemoryBytes = proc.MemoryBytes
        };
    }
}
