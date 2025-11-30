namespace TorGames.Server.Models;

/// <summary>
/// Information about a specific version of the client.
/// </summary>
public class VersionInfo
{
    public string Version { get; set; } = string.Empty;
    public string Sha256 { get; set; } = string.Empty;
    public long FileSize { get; set; }
    public DateTime UploadedAt { get; set; }
    public string ReleaseNotes { get; set; } = string.Empty;
    public string UploadedBy { get; set; } = string.Empty;

    /// <summary>
    /// Whether this is a test version (only deployed to clients in test mode).
    /// </summary>
    public bool IsTestVersion { get; set; }
}

/// <summary>
/// Manifest containing all available versions.
/// </summary>
public class VersionManifest
{
    public string LatestVersion { get; set; } = string.Empty;

    /// <summary>
    /// The latest test version (for clients in test mode).
    /// </summary>
    public string LatestTestVersion { get; set; } = string.Empty;

    public List<VersionInfo> Versions { get; set; } = new();
}

/// <summary>
/// Response for update check requests.
/// </summary>
public class UpdateCheckResponse
{
    public bool UpdateAvailable { get; set; }
    public VersionInfo? LatestVersion { get; set; }
}

/// <summary>
/// Request model for uploading a new version.
/// </summary>
public class UploadVersionRequest
{
    public string ReleaseNotes { get; set; } = string.Empty;
}
