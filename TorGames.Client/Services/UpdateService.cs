using System.Diagnostics;
using System.Net.Http.Json;
using System.Reflection;
using System.Security.Cryptography;
using Microsoft.Extensions.Logging;

namespace TorGames.Client.Services;

/// <summary>
/// Information about an available version.
/// </summary>
public class VersionInfo
{
    public string Version { get; set; } = string.Empty;
    public string Sha256 { get; set; } = string.Empty;
    public long FileSize { get; set; }
    public DateTime UploadedAt { get; set; }
    public string ReleaseNotes { get; set; } = string.Empty;
}

/// <summary>
/// Response from update check endpoint.
/// </summary>
public class UpdateCheckResponse
{
    public bool UpdateAvailable { get; set; }
    public VersionInfo? LatestVersion { get; set; }
}

/// <summary>
/// Service for checking and applying client updates.
/// </summary>
public class UpdateService
{
    private readonly ILogger _logger;
    private readonly HttpClient _httpClient;
    private readonly string _serverUrl;
    private readonly string _currentVersion;

    private static readonly string TempUpdateDir = Path.Combine(Path.GetTempPath(), "TorGames_Update");

    public UpdateService(ILogger logger, string serverUrl)
    {
        _logger = logger;
        _serverUrl = serverUrl.TrimEnd('/');
        _httpClient = new HttpClient { Timeout = TimeSpan.FromMinutes(10) };

        // Get current version - prefer version file, fallback to assembly
        _currentVersion = GetCurrentVersion();

        _logger.LogInformation("UpdateService initialized. Current version: {Version}", _currentVersion);
    }

    private static string GetCurrentVersion()
    {
        // Try to read from version file first (updated by updater)
        try
        {
            var versionFilePath = Path.Combine(AppContext.BaseDirectory, "TorGames.version");
            if (File.Exists(versionFilePath))
            {
                var fileVersion = File.ReadAllText(versionFilePath).Trim();
                if (!string.IsNullOrEmpty(fileVersion))
                {
                    return fileVersion;
                }
            }
        }
        catch
        {
            // Ignore errors reading version file
        }

        // Fallback to assembly version
        return Assembly.GetExecutingAssembly()
            .GetCustomAttribute<AssemblyInformationalVersionAttribute>()
            ?.InformationalVersion ?? "0.0.0";
    }

    /// <summary>
    /// Gets the current client version.
    /// </summary>
    public string CurrentVersion => _currentVersion;

    /// <summary>
    /// Checks if an update is available.
    /// </summary>
    public async Task<VersionInfo?> CheckForUpdateAsync()
    {
        try
        {
            _logger.LogDebug("Checking for updates...");

            var response = await _httpClient.GetAsync(
                $"{_serverUrl}/api/update/check?currentVersion={_currentVersion}");

            if (!response.IsSuccessStatusCode)
            {
                _logger.LogWarning("Update check failed: {Status}", response.StatusCode);
                return null;
            }

            var result = await response.Content.ReadFromJsonAsync<UpdateCheckResponse>();

            if (result?.UpdateAvailable == true && result.LatestVersion != null)
            {
                _logger.LogInformation("Update available: {Version}", result.LatestVersion.Version);
                return result.LatestVersion;
            }

            _logger.LogDebug("No update available");
            return null;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to check for updates");
            return null;
        }
    }

    /// <summary>
    /// Downloads and applies an update.
    /// This method will exit the application to allow the updater to replace files.
    /// </summary>
    public async Task<bool> ApplyUpdateAsync(VersionInfo version)
    {
        try
        {
            _logger.LogInformation("Applying update to version {Version}...", version.Version);

            // Create temp directory
            Directory.CreateDirectory(TempUpdateDir);

            // Download paths
            var newClientPath = Path.Combine(TempUpdateDir, "TorGames.Client.exe");
            var updaterPath = Path.Combine(TempUpdateDir, "TorGames.Updater.exe");
            var backupDir = Path.Combine(TempUpdateDir, "backup");

            // Step 1: Download updater
            _logger.LogInformation("Downloading updater...");
            if (!await DownloadFileAsync($"{_serverUrl}/api/update/updater", updaterPath))
            {
                _logger.LogError("Failed to download updater");
                return false;
            }

            // Step 2: Download new client version
            _logger.LogInformation("Downloading new client version...");
            if (!await DownloadFileAsync($"{_serverUrl}/api/update/download/{version.Version}", newClientPath))
            {
                _logger.LogError("Failed to download new client version");
                return false;
            }

            // Step 3: Verify checksum
            _logger.LogInformation("Verifying checksum...");
            var actualHash = await CalculateSha256Async(newClientPath);
            if (!string.Equals(actualHash, version.Sha256, StringComparison.OrdinalIgnoreCase))
            {
                _logger.LogError("Checksum mismatch! Expected: {Expected}, Got: {Actual}",
                    version.Sha256, actualHash);
                return false;
            }

            // Step 4: Launch updater and exit
            var currentExePath = Process.GetCurrentProcess().MainModule?.FileName;
            if (string.IsNullOrEmpty(currentExePath))
            {
                _logger.LogError("Failed to get current executable path");
                return false;
            }

            var currentPid = Process.GetCurrentProcess().Id;

            _logger.LogInformation("Launching updater...");
            var startInfo = new ProcessStartInfo
            {
                FileName = updaterPath,
                // Pass version as 5th argument so updater can pass it to new client
                Arguments = $"\"{currentExePath}\" \"{newClientPath}\" \"{backupDir}\" {currentPid} \"{version.Version}\"",
                UseShellExecute = true,
                WorkingDirectory = TempUpdateDir
            };

            var updaterProcess = Process.Start(startInfo);
            if (updaterProcess == null)
            {
                _logger.LogError("Failed to start updater process");
                return false;
            }

            _logger.LogInformation("Updater launched (PID: {Pid}). Exiting for update...", updaterProcess.Id);

            // Exit to allow updater to replace files
            Environment.Exit(0);
            return true; // Never reached
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to apply update");
            return false;
        }
    }

    /// <summary>
    /// Cleans up temporary files after an update.
    /// Called when client starts with --post-update flag.
    /// </summary>
    public void CleanupAfterUpdate()
    {
        try
        {
            _logger.LogInformation("Post-update cleanup starting...");

            // Wait a moment to ensure updater has exited
            Thread.Sleep(2000);

            // Delete temp update directory
            if (Directory.Exists(TempUpdateDir))
            {
                try
                {
                    Directory.Delete(TempUpdateDir, recursive: true);
                    _logger.LogInformation("Cleaned up temp update directory");
                }
                catch (Exception ex)
                {
                    _logger.LogWarning(ex, "Failed to clean up temp directory completely, will try again later");

                    // Try to at least delete the updater
                    try
                    {
                        var updaterPath = Path.Combine(TempUpdateDir, "TorGames.Updater.exe");
                        if (File.Exists(updaterPath))
                        {
                            File.Delete(updaterPath);
                        }
                    }
                    catch { /* ignore */ }
                }
            }

            _logger.LogInformation("Post-update cleanup completed. New version: {Version}", _currentVersion);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Error during post-update cleanup");
        }
    }

    /// <summary>
    /// Downloads a file from the server.
    /// </summary>
    private async Task<bool> DownloadFileAsync(string url, string destinationPath)
    {
        try
        {
            using var response = await _httpClient.GetAsync(url, HttpCompletionOption.ResponseHeadersRead);

            if (!response.IsSuccessStatusCode)
            {
                _logger.LogError("Download failed: {Status} from {Url}", response.StatusCode, url);
                return false;
            }

            await using var contentStream = await response.Content.ReadAsStreamAsync();
            await using var fileStream = File.Create(destinationPath);
            await contentStream.CopyToAsync(fileStream);

            _logger.LogDebug("Downloaded {Url} to {Path}", url, destinationPath);
            return true;
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to download {Url}", url);
            return false;
        }
    }

    /// <summary>
    /// Calculates SHA256 hash of a file.
    /// </summary>
    private static async Task<string> CalculateSha256Async(string filePath)
    {
        using var sha256 = SHA256.Create();
        await using var stream = File.OpenRead(filePath);
        var hash = await sha256.ComputeHashAsync(stream);
        return BitConverter.ToString(hash).Replace("-", "").ToLowerInvariant();
    }
}
