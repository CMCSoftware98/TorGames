using System.Management;
using System.Net.NetworkInformation;
using System.Security.Cryptography;
using System.Text;

namespace TorGames.Common.Hardware;

/// <summary>
/// Generates a unique, persistent hardware fingerprint for machine identification.
/// The fingerprint survives application and OS reinstalls.
/// </summary>
public static class MachineFingerprint
{
    private static string? _cachedFingerprint;

    /// <summary>
    /// Gets the unique hardware fingerprint for this machine.
    /// Result is cached after first call.
    /// </summary>
    public static string GetFingerprint()
    {
        if (_cachedFingerprint != null)
            return _cachedFingerprint;

        var components = new StringBuilder();

        // CPU ID
        components.Append(GetCpuId());
        components.Append('|');

        // BIOS Serial
        components.Append(GetBiosSerial());
        components.Append('|');

        // Motherboard Serial
        components.Append(GetMotherboardSerial());
        components.Append('|');

        // Primary MAC Address
        components.Append(GetPrimaryMacAddress());

        // Generate SHA256 hash
        var hash = SHA256.HashData(Encoding.UTF8.GetBytes(components.ToString()));
        _cachedFingerprint = Convert.ToHexString(hash).ToLowerInvariant();

        return _cachedFingerprint;
    }

    /// <summary>
    /// Gets the unique connection key combining fingerprint and client type.
    /// </summary>
    public static string GetConnectionKey(string clientType)
    {
        return $"{GetFingerprint()}:{clientType.ToUpperInvariant()}";
    }

    private static string GetCpuId()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT ProcessorId FROM Win32_Processor");
            foreach (var obj in searcher.Get())
            {
                var id = obj["ProcessorId"]?.ToString();
                if (!string.IsNullOrEmpty(id))
                    return id;
            }
        }
        catch
        {
            // Fallback if WMI fails
        }

        return "CPU_UNKNOWN";
    }

    private static string GetBiosSerial()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT SerialNumber FROM Win32_BIOS");
            foreach (var obj in searcher.Get())
            {
                var serial = obj["SerialNumber"]?.ToString();
                if (!string.IsNullOrEmpty(serial))
                    return serial;
            }
        }
        catch
        {
            // Fallback if WMI fails
        }

        return "BIOS_UNKNOWN";
    }

    private static string GetMotherboardSerial()
    {
        try
        {
            using var searcher = new ManagementObjectSearcher("SELECT SerialNumber FROM Win32_BaseBoard");
            foreach (var obj in searcher.Get())
            {
                var serial = obj["SerialNumber"]?.ToString();
                if (!string.IsNullOrEmpty(serial))
                    return serial;
            }
        }
        catch
        {
            // Fallback if WMI fails
        }

        return "MB_UNKNOWN";
    }

    private static string GetPrimaryMacAddress()
    {
        try
        {
            var networkInterface = NetworkInterface.GetAllNetworkInterfaces()
                .Where(nic => nic.OperationalStatus == OperationalStatus.Up
                              && nic.NetworkInterfaceType != NetworkInterfaceType.Loopback
                              && nic.NetworkInterfaceType != NetworkInterfaceType.Tunnel)
                .OrderByDescending(nic => nic.Speed)
                .FirstOrDefault();

            if (networkInterface != null)
            {
                var mac = networkInterface.GetPhysicalAddress();
                return mac.ToString();
            }
        }
        catch
        {
            // Fallback if network info fails
        }

        return "MAC_UNKNOWN";
    }
}
