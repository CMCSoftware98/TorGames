using System.Reflection.Metadata;
using System.Reflection.PortableExecutable;
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

        // Scan for existing version folders and populate manifest
        ScanAndPopulateVersions();
    }

    /// <summary>
    /// Scans the updates folder for version directories and populates the manifest.
    /// This helps when files are manually added to the server.
    /// </summary>
    public void ScanAndPopulateVersions()
    {
        try
        {
            var manifest = LoadManifest();
            var existingVersions = new HashSet<string>(manifest.Versions.Select(v => v.Version));
            var foundNewVersions = false;

            // Scan for version directories (format: YYYY.MM.DD.BUILD)
            var versionPattern = new System.Text.RegularExpressions.Regex(@"^20\d{2}\.\d{1,2}\.\d{1,2}\.\d+$");

            foreach (var dir in Directory.GetDirectories(_updatesPath))
            {
                var dirName = Path.GetFileName(dir);
                if (!versionPattern.IsMatch(dirName))
                    continue;

                if (existingVersions.Contains(dirName))
                    continue;

                // Check if TorGames.Client.exe exists in this folder
                var clientExePath = Path.Combine(dir, "TorGames.Client.exe");
                if (!File.Exists(clientExePath))
                {
                    _logger.LogDebug("Version folder {Version} has no TorGames.Client.exe, skipping", dirName);
                    continue;
                }

                // Calculate hash and get file info
                var fileInfo = new FileInfo(clientExePath);
                var hash = CalculateSha256Sync(clientExePath);

                var versionInfo = new VersionInfo
                {
                    Version = dirName,
                    Sha256 = hash,
                    FileSize = fileInfo.Length,
                    UploadedAt = fileInfo.CreationTimeUtc,
                    ReleaseNotes = "Auto-discovered from updates folder",
                    UploadedBy = "System"
                };

                manifest.Versions.Add(versionInfo);
                foundNewVersions = true;
                _logger.LogInformation("Discovered version {Version} (size: {Size} bytes)", dirName, fileInfo.Length);
            }

            if (foundNewVersions)
            {
                // Update latest version to the highest version found
                var latestVersion = manifest.Versions
                    .OrderByDescending(v => v.Version, new VersionComparer())
                    .FirstOrDefault();

                if (latestVersion != null)
                {
                    manifest.LatestVersion = latestVersion.Version;
                }

                SaveManifest(manifest);
                _logger.LogInformation("Updated manifest with {Count} versions, latest: {Latest}",
                    manifest.Versions.Count, manifest.LatestVersion);
            }
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to scan for versions");
        }
    }

    private static string CalculateSha256Sync(string filePath)
    {
        using var sha256 = SHA256.Create();
        using var stream = File.OpenRead(filePath);
        var hash = sha256.ComputeHash(stream);
        return BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
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
    /// Gets the latest version info (production only).
    /// </summary>
    public VersionInfo? GetLatestVersion()
    {
        var manifest = LoadManifest();
        if (string.IsNullOrEmpty(manifest.LatestVersion))
            return null;

        return manifest.Versions.FirstOrDefault(v => v.Version == manifest.LatestVersion);
    }

    /// <summary>
    /// Gets the latest test version info.
    /// </summary>
    public VersionInfo? GetLatestTestVersion()
    {
        var manifest = LoadManifest();
        if (string.IsNullOrEmpty(manifest.LatestTestVersion))
            return null;

        return manifest.Versions.FirstOrDefault(v => v.Version == manifest.LatestTestVersion);
    }

    /// <summary>
    /// Checks if an update is available for the given version.
    /// </summary>
    /// <param name="currentVersion">The client's current version</param>
    /// <param name="isTestClient">Whether the client is in test mode</param>
    public UpdateCheckResponse CheckForUpdate(string currentVersion, bool isTestClient = false)
    {
        VersionInfo? targetVersion;

        if (isTestClient)
        {
            // Test clients get the latest test version if available, otherwise fall back to production
            targetVersion = GetLatestTestVersion() ?? GetLatestVersion();
        }
        else
        {
            targetVersion = GetLatestVersion();
        }

        if (targetVersion == null)
        {
            return new UpdateCheckResponse { UpdateAvailable = false };
        }

        var current = ParseVersion(currentVersion);
        var latestVer = ParseVersion(targetVersion.Version);

        return new UpdateCheckResponse
        {
            UpdateAvailable = CompareVersions(latestVer, current) > 0,
            LatestVersion = targetVersion
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
    /// <param name="fileStream">The file stream</param>
    /// <param name="version">Version string</param>
    /// <param name="releaseNotes">Release notes</param>
    /// <param name="uploadedBy">Who uploaded this version</param>
    /// <param name="isTestVersion">Whether this is a test version</param>
    public async Task<VersionInfo> AddVersionAsync(Stream fileStream, string version, string releaseNotes, string uploadedBy, bool isTestVersion = false)
    {
        // Validate version format and ensure it's higher than existing
        if (!IsValidNewVersion(version, isTestVersion))
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
            UploadedBy = uploadedBy,
            IsTestVersion = isTestVersion
        };

        // Update manifest
        lock (_manifestLock)
        {
            var manifest = LoadManifest();
            manifest.Versions.Add(versionInfo);

            if (isTestVersion)
            {
                manifest.LatestTestVersion = version;
            }
            else
            {
                manifest.LatestVersion = version;
            }

            SaveManifest(manifest);
        }

        _logger.LogInformation("Added {Type} version {Version} (size: {Size} bytes, hash: {Hash})",
            isTestVersion ? "test" : "production", version, fileInfo.Length, hash);

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

            // Count versions of the same type
            var sameTypeVersions = manifest.Versions.Where(v => v.IsTestVersion == versionInfo.IsTestVersion).ToList();

            // Don't allow deleting the only version of a type (unless it's the only version overall)
            if (sameTypeVersions.Count == 1 && manifest.Versions.Count > 1)
            {
                // Allow deletion - will just clear the latest for that type
            }
            else if (manifest.Versions.Count == 1)
            {
                throw new InvalidOperationException("Cannot delete the only version");
            }

            // Remove from manifest
            manifest.Versions.Remove(versionInfo);

            // Update latest version if needed
            if (versionInfo.IsTestVersion)
            {
                if (manifest.LatestTestVersion == version)
                {
                    var newLatest = manifest.Versions
                        .Where(v => v.IsTestVersion)
                        .OrderByDescending(v => ParseVersion(v.Version))
                        .FirstOrDefault();
                    manifest.LatestTestVersion = newLatest?.Version ?? string.Empty;
                }
            }
            else
            {
                if (manifest.LatestVersion == version)
                {
                    var newLatest = manifest.Versions
                        .Where(v => !v.IsTestVersion)
                        .OrderByDescending(v => ParseVersion(v.Version))
                        .FirstOrDefault();
                    manifest.LatestVersion = newLatest?.Version ?? string.Empty;
                }
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

            _logger.LogInformation("Deleted {Type} version {Version}",
                versionInfo.IsTestVersion ? "test" : "production", version);
            return true;
        }
    }

    /// <summary>
    /// Validates that a new version is higher than all existing versions of the same type.
    /// </summary>
    /// <param name="newVersion">The new version string</param>
    /// <param name="isTestVersion">Whether this is a test version</param>
    public bool IsValidNewVersion(string newVersion, bool isTestVersion = false)
    {
        // Validate format: YYYY.MM.DD.BUILD
        var parts = newVersion.Split('.');
        if (parts.Length != 4)
            return false;

        if (!parts.All(p => int.TryParse(p, out _)))
            return false;

        var manifest = LoadManifest();

        // Filter versions by type
        var versionsOfSameType = manifest.Versions.Where(v => v.IsTestVersion == isTestVersion).ToList();

        if (versionsOfSameType.Count == 0)
            return true;

        var newVer = ParseVersion(newVersion);

        foreach (var existing in versionsOfSameType)
        {
            var existingVer = ParseVersion(existing.Version);
            if (CompareVersions(newVer, existingVer) <= 0)
                return false;
        }

        return true;
    }

    /// <summary>
    /// Extracts version from a .NET assembly file using PE metadata.
    /// Works cross-platform (Linux/Windows).
    /// For single-file apps, falls back to pattern matching in the binary.
    /// </summary>
    public string? ExtractVersionFromFile(string filePath)
    {
        // First, try pattern matching which works for single-file apps
        // This is the most reliable method for published single-file executables
        var patternVersion = ExtractVersionFromSingleFileApp(filePath);
        if (!string.IsNullOrEmpty(patternVersion))
        {
            return patternVersion;
        }

        // Fallback: Try reading PE metadata directly (works for non-single-file apps)
        try
        {
            using var fs = new FileStream(filePath, FileMode.Open, FileAccess.Read, FileShare.Read);
            using var peReader = new PEReader(fs);

            if (!peReader.HasMetadata)
            {
                _logger.LogWarning("File has no .NET metadata and no version pattern found: {FilePath}", filePath);
                return null;
            }

            var metadataReader = peReader.GetMetadataReader();

            // Try to get AssemblyInformationalVersion first (most accurate)
            foreach (var attrHandle in metadataReader.GetAssemblyDefinition().GetCustomAttributes())
            {
                var attr = metadataReader.GetCustomAttribute(attrHandle);
                var ctorHandle = attr.Constructor;

                if (ctorHandle.Kind == HandleKind.MemberReference)
                {
                    var memberRef = metadataReader.GetMemberReference((MemberReferenceHandle)ctorHandle);
                    var typeRef = metadataReader.GetTypeReference((TypeReferenceHandle)memberRef.Parent);
                    var typeName = metadataReader.GetString(typeRef.Name);

                    if (typeName == "AssemblyInformationalVersionAttribute")
                    {
                        var value = attr.DecodeValue(new CustomAttributeTypeProvider());
                        if (value.FixedArguments.Length > 0)
                        {
                            var version = value.FixedArguments[0].Value?.ToString();
                            if (!string.IsNullOrEmpty(version))
                            {
                                // Remove any +commit suffix if present
                                var plusIndex = version.IndexOf('+');
                                if (plusIndex > 0)
                                {
                                    version = version.Substring(0, plusIndex);
                                }
                                _logger.LogInformation("Extracted InformationalVersion from PE: {Version}", version);
                                return version;
                            }
                        }
                    }
                }
            }

            // Fallback to AssemblyVersion
            var assemblyDef = metadataReader.GetAssemblyDefinition();
            var assemblyVersion = assemblyDef.Version;
            var versionString = $"{assemblyVersion.Major}.{assemblyVersion.Minor}.{assemblyVersion.Build}.{assemblyVersion.Revision}";
            _logger.LogInformation("Extracted AssemblyVersion from PE: {Version}", versionString);
            return versionString;
        }
        catch (Exception ex)
        {
            _logger.LogWarning(ex, "Could not read PE metadata: {FilePath}", filePath);
            return null;
        }
    }

    /// <summary>
    /// Attempts to extract version from a single-file .NET app by searching for version markers.
    /// </summary>
    private string? ExtractVersionFromSingleFileApp(string filePath)
    {
        try
        {
            // For single-file apps, search for our custom version marker in the binary
            var fileBytes = File.ReadAllBytes(filePath);
            var fileContent = System.Text.Encoding.UTF8.GetString(fileBytes);

            // First, try to find our custom version marker: TORGAMES_VERSION_YYYY.M.D.HHmm_END
            var markerPattern = new System.Text.RegularExpressions.Regex(@"TORGAMES_VERSION_(20\d{2}\.\d{1,2}\.\d{1,2}\.\d{1,4})_END");
            var markerMatch = markerPattern.Match(fileContent);

            if (markerMatch.Success)
            {
                var version = markerMatch.Groups[1].Value;
                _logger.LogInformation("Extracted version from marker: {Version}", version);
                return version;
            }

            // Fallback: Look for version pattern: YYYY.M.D.HHmm (e.g., 2025.11.28.1430)
            var versionPattern = new System.Text.RegularExpressions.Regex(@"20\d{2}\.\d{1,2}\.\d{1,2}\.\d{3,4}");
            var matches = versionPattern.Matches(fileContent);

            if (matches.Count > 0)
            {
                // Find the most likely version (one that appears multiple times or looks valid)
                var versionCounts = new Dictionary<string, int>();
                foreach (System.Text.RegularExpressions.Match match in matches)
                {
                    var v = match.Value;
                    // Skip versions that look like timestamps or other data
                    if (v.EndsWith(".0000") || v.EndsWith(".0001"))
                        continue;
                    versionCounts.TryGetValue(v, out var count);
                    versionCounts[v] = count + 1;
                }

                if (versionCounts.Count > 0)
                {
                    // Get the version that appears most frequently
                    var bestVersion = versionCounts.OrderByDescending(x => x.Value).First().Key;
                    _logger.LogInformation("Extracted version from pattern match: {Version}", bestVersion);
                    return bestVersion;
                }
            }

            _logger.LogWarning("Could not find version pattern in single-file app");
            return null;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to extract version from single-file app");
            return null;
        }
    }

    /// <summary>
    /// Custom attribute type provider for decoding custom attributes.
    /// </summary>
    private class CustomAttributeTypeProvider : ICustomAttributeTypeProvider<object>
    {
        public object GetPrimitiveType(PrimitiveTypeCode typeCode) => typeCode;
        public object GetSystemType() => typeof(Type);
        public object GetSZArrayType(object elementType) => elementType;
        public object GetTypeFromDefinition(MetadataReader reader, TypeDefinitionHandle handle, byte rawTypeKind) => handle;
        public object GetTypeFromReference(MetadataReader reader, TypeReferenceHandle handle, byte rawTypeKind) => handle;
        public object GetTypeFromSerializedName(string name) => name;
        public PrimitiveTypeCode GetUnderlyingEnumType(object type) => PrimitiveTypeCode.Int32;
        public bool IsSystemType(object type) => type is Type;
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
