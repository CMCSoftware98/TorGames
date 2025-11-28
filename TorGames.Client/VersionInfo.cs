using System.Reflection;

namespace TorGames.Client;

/// <summary>
/// Provides version information for the client.
/// The version is read from assembly attributes set during build.
/// </summary>
public static class VersionInfo
{
    /// <summary>
    /// Gets the current version of the client.
    /// Format: YYYY.M.D.HHmm (e.g., 2025.11.28.1430)
    /// </summary>
    public static string Version { get; } = GetVersion();

    /// <summary>
    /// Version marker that can be found in the binary.
    /// Format: TORGAMES_VERSION_YYYY.M.D.HHmm_END
    /// </summary>
    public static string VersionMarker => $"TORGAMES_VERSION_{Version}_END";

    private static string GetVersion()
    {
        var assembly = Assembly.GetExecutingAssembly();

        // Try InformationalVersion first (most accurate)
        var infoVersion = assembly
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
            ?.InformationalVersion;

        if (!string.IsNullOrEmpty(infoVersion))
        {
            // Remove any +commit suffix if present
            var plusIndex = infoVersion.IndexOf('+');
            if (plusIndex > 0)
            {
                return infoVersion.Substring(0, plusIndex);
            }
            return infoVersion;
        }

        // Fallback to FileVersion
        var fileVersion = assembly
            .GetCustomAttribute<AssemblyFileVersionAttribute>()
            ?.Version;

        if (!string.IsNullOrEmpty(fileVersion))
        {
            return fileVersion;
        }

        // Last resort: AssemblyVersion
        return assembly.GetName().Version?.ToString() ?? "0.0.0.0";
    }

    /// <summary>
    /// Static constructor to ensure the version marker string is embedded in the binary.
    /// This helps the server find the version when scanning the executable.
    /// </summary>
    static VersionInfo()
    {
        // This string will be embedded in the binary and can be found by the server
        // when extracting the version from a single-file app
        _ = $"TORGAMES_VERSION_{Version}_END";
    }
}
