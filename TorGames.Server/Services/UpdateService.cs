using System.Security.Cryptography;
using System.Text.Json;
using TorGames.Server.Models;

namespace TorGames.Server.Services;

/// <summary>
/// Service for managing client versions and updates.
/// </summary>
public class UpdateService
{
    private readonly ILogger<UpdateService> _logger;
    private readonly IConfiguration _configuration;
    private readonly string _updatesPath;
    private readonly string _manifestPath;
    private readonly object _manifestLock = new();

    private static readonly JsonSerializerOptions JsonOptions = new()
    {
        PropertyNamingPolicy = JsonNamingPolicy.CamelCase,
        WriteIndented = true
    };

    public UpdateService(ILogger<UpdateService> logger, IConfiguration configuration)
    {
        _logger = logger;
        _configuration = configuration;
        _updatesPath = _configuration["Updates:StoragePath"] ?? "updates";
        _manifestPath = Path.Combine(_updatesPath, "manifest.json");

        // Ensure directories exist
        Directory.CreateDirectory(_updatesPath);
        Directory.CreateDirectory(Path.Combine(_updatesPath, "updater"));

        // Initialize manifest if it doesn't exist
        if (!File.Exists(_manifestPath))
        {
            SaveManifest(new VersionManifest());
        }
    }

    /// <summary>
    /// Gets all available versions sorted by version descending.
    /// </summary>
    public List<VersionInfo> GetAllVersions()
    {
        var manifest = LoadManifest();
        return manifest.Versions
            .OrderByDescending(v => v.Version, new VersionComparer())
            .ToList();
    }

    /// <summary>
    /// Gets the latest version info.
    /// </summary>
    public VersionInfo? GetLatestVersion()
    {
        var manifest = LoadManifest();
        if (string.IsNullOrEmpty(manifest.LatestVersion))
            return null;

        return manifest.Versions.FirstOrDefault(v => v.Version == manifest.LatestVersion);
    }

    /// <summary>
    /// Checks if an update is available for the given version.
    /// </summary>
    public UpdateCheckResponse CheckForUpdate(string currentVersion)
    {
        var latest = GetLatestVersion();
        if (latest == null)
        {
            return new UpdateCheckResponse { UpdateAvailable = false };
        }

        var current = ParseVersion(currentVersion);
        var latestVer = ParseVersion(latest.Version);

        return new UpdateCheckResponse
        {
            UpdateAvailable = CompareVersions(latestVer, current) > 0,
            LatestVersion = latest
        };
    }

    /// <summary>
    /// Gets the file path for a specific version.
    /// </summary>
    public string? GetVersionFilePath(string version)
    {
        var manifest = LoadManifest();
        var versionInfo = manifest.Versions.FirstOrDefault(v => v.Version == version);
        if (versionInfo == null)
            return null;

        var filePath = Path.Combine(_updatesPath, version, "TorGames.Client.exe");
        return File.Exists(filePath) ? filePath : null;
    }

    /// <summary>
    /// Gets the updater executable path.
    /// </summary>
    public string? GetUpdaterPath()
    {
        var path = Path.Combine(_updatesPath, "updater", "TorGames.Updater.exe");
        return File.Exists(path) ? path : null;
    }

    /// <summary>
    /// Adds a new version to the manifest.
    /// </summary>
    public async Task<VersionInfo> AddVersionAsync(Stream fileStream, string version, string releaseNotes, string uploadedBy)
    {
        // Validate version format and ensure it's higher than existing
        if (!IsValidNewVersion(version))
        {
            throw new InvalidOperationException($"Version {version} is not valid or not higher than existing versions");
        }

        var versionDir = Path.Combine(_updatesPath, version);
        Directory.CreateDirectory(versionDir);

        var filePath = Path.Combine(versionDir, "TorGames.Client.exe");

        // Save the file
        await using (var fs = File.Create(filePath))
        {
            await fileStream.CopyToAsync(fs);
        }

        // Calculate hash and size
        var hash = await CalculateSha256Async(filePath);
        var fileInfo = new FileInfo(filePath);

        var versionInfo = new VersionInfo
        {
            Version = version,
            Sha256 = hash,
            FileSize = fileInfo.Length,
            UploadedAt = DateTime.UtcNow,
            ReleaseNotes = releaseNotes,
            UploadedBy = uploadedBy
        };

        // Update manifest
        lock (_manifestLock)
        {
            var manifest = LoadManifest();
            manifest.Versions.Add(versionInfo);
            manifest.LatestVersion = version;
            SaveManifest(manifest);
        }

        _logger.LogInformation("Added version {Version} (size: {Size} bytes, hash: {Hash})",
            version, fileInfo.Length, hash);

        return versionInfo;
    }

    /// <summary>
    /// Deletes a version from the manifest and disk.
    /// </summary>
    public bool DeleteVersion(string version)
    {
        lock (_manifestLock)
        {
            var manifest = LoadManifest();
            var versionInfo = manifest.Versions.FirstOrDefault(v => v.Version == version);
            if (versionInfo == null)
                return false;

            // Don't allow deleting the only version or the latest if there are others
            if (manifest.Versions.Count == 1)
            {
                throw new InvalidOperationException("Cannot delete the only version");
            }

            // Remove from manifest
            manifest.Versions.Remove(versionInfo);

            // Update latest version if needed
            if (manifest.LatestVersion == version)
            {
                var newLatest = manifest.Versions
                    .OrderByDescending(v => ParseVersion(v.Version))
                    .FirstOrDefault();
                manifest.LatestVersion = newLatest?.Version ?? string.Empty;
            }

            SaveManifest(manifest);

            // Delete the version directory
            var versionDir = Path.Combine(_updatesPath, version);
            if (Directory.Exists(versionDir))
            {
                try
                {
                    Directory.Delete(versionDir, recursive: true);
                }
                catch (Exception ex)
                {
                    _logger.LogWarning(ex, "Failed to delete version directory {Dir}", versionDir);
                }
            }

            _logger.LogInformation("Deleted version {Version}", version);
            return true;
        }
    }

    /// <summary>
    /// Validates that a new version is higher than all existing versions.
    /// </summary>
    public bool IsValidNewVersion(string newVersion)
    {
        // Validate format: YYYY.MM.DD.BUILD
        var parts = newVersion.Split('.');
        if (parts.Length != 4)
            return false;

        if (!parts.All(p => int.TryParse(p, out _)))
            return false;

        var manifest = LoadManifest();
        if (manifest.Versions.Count == 0)
            return true;

        var newVer = ParseVersion(newVersion);

        foreach (var existing in manifest.Versions)
        {
            var existingVer = ParseVersion(existing.Version);
            if (CompareVersions(newVer, existingVer) <= 0)
                return false;
        }

        return true;
    }

    /// <summary>
    /// Extracts version from uploaded file's metadata.
    /// </summary>
    public string? ExtractVersionFromFile(string filePath)
    {
        try
        {
            var versionInfo = System.Diagnostics.FileVersionInfo.GetVersionInfo(filePath);
            return versionInfo.ProductVersion;
        }
        catch
        {
            return null;
        }
    }

    private VersionManifest LoadManifest()
    {
        if (!File.Exists(_manifestPath))
            return new VersionManifest();

        try
        {
            var json = File.ReadAllText(_manifestPath);
            return JsonSerializer.Deserialize<VersionManifest>(json, JsonOptions) ?? new VersionManifest();
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to load manifest");
            return new VersionManifest();
        }
    }

    private void SaveManifest(VersionManifest manifest)
    {
        var json = JsonSerializer.Serialize(manifest, JsonOptions);
        File.WriteAllText(_manifestPath, json);
    }

    private static async Task<string> CalculateSha256Async(string filePath)
    {
        using var sha256 = SHA256.Create();
        await using var stream = File.OpenRead(filePath);
        var hash = await sha256.ComputeHashAsync(stream);
        return BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
    }

    /// <summary>
    /// Parses a date-based version string into comparable parts.
    /// Format: YYYY.MM.DD.BUILD
    /// </summary>
    private static int[] ParseVersion(string version)
    {
        var parts = version.Split('.');
        if (parts.Length != 4)
            return new[] { 0, 0, 0, 0 };

        return parts.Select(p => int.TryParse(p, out var n) ? n : 0).ToArray();
    }

    /// <summary>
    /// Compares two parsed versions.
    /// Returns positive if a > b, negative if a < b, 0 if equal.
    /// </summary>
    private static int CompareVersions(int[] a, int[] b)
    {
        for (int i = 0; i < 4; i++)
        {
            if (a[i] > b[i]) return 1;
            if (a[i] < b[i]) return -1;
        }
        return 0;
    }

    /// <summary>
    /// Comparer for version strings in format YYYY.MM.DD.BUILD
    /// </summary>
    private class VersionComparer : IComparer<string>
    {
        public int Compare(string? x, string? y)
        {
            if (x == null && y == null) return 0;
            if (x == null) return -1;
            if (y == null) return 1;

            var partsX = x.Split('.').Select(p => int.TryParse(p, out var n) ? n : 0).ToArray();
            var partsY = y.Split('.').Select(p => int.TryParse(p, out var n) ? n : 0).ToArray();

            for (int i = 0; i < Math.Min(partsX.Length, partsY.Length); i++)
            {
                if (partsX[i] > partsY[i]) return 1;
                if (partsX[i] < partsY[i]) return -1;
            }

            return partsX.Length.CompareTo(partsY.Length);
        }
    }
}
