using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using TorGames.Common.Protos;
using TorGames.Server.Models;
using TorGames.Server.Services;

namespace TorGames.Server.Controllers;

/// <summary>
/// Controller for managing client updates and versions.
/// </summary>
[ApiController]
[Route("api/[controller]")]
public class UpdateController : ControllerBase
{
    private readonly UpdateService _updateService;
    private readonly ClientManager _clientManager;
    private readonly ILogger<UpdateController> _logger;

    public UpdateController(
        UpdateService updateService,
        ClientManager clientManager,
        ILogger<UpdateController> logger)
    {
        _updateService = updateService;
        _clientManager = clientManager;
        _logger = logger;
    }

    /// <summary>
    /// Gets all available versions sorted by version descending.
    /// </summary>
    [HttpGet("versions")]
    [AllowAnonymous]
    public ActionResult<List<VersionInfo>> GetVersions()
    {
        var versions = _updateService.GetAllVersions();
        return Ok(versions);
    }

    /// <summary>
    /// Gets the latest version info.
    /// </summary>
    [HttpGet("latest")]
    [AllowAnonymous]
    public ActionResult<VersionInfo> GetLatestVersion()
    {
        var latest = _updateService.GetLatestVersion();
        if (latest == null)
            return NoContent();

        return Ok(latest);
    }

    /// <summary>
    /// Checks if an update is available for the given version.
    /// </summary>
    /// <param name="currentVersion">The client's current version</param>
    /// <param name="clientId">Optional client ID to check if client is in test mode</param>
    [HttpGet("check")]
    [AllowAnonymous]
    public async Task<ActionResult<UpdateCheckResponse>> CheckForUpdate(
        [FromQuery] string currentVersion,
        [FromQuery] string? clientId = null)
    {
        if (string.IsNullOrWhiteSpace(currentVersion))
            return BadRequest("currentVersion is required");

        // Check if the client is in test mode
        bool isTestClient = false;
        if (!string.IsNullOrWhiteSpace(clientId))
        {
            isTestClient = await _clientManager.IsClientInTestModeAsync(clientId);
        }

        var response = _updateService.CheckForUpdate(currentVersion, isTestClient);
        return Ok(response);
    }

    /// <summary>
    /// Downloads the latest version of the client.
    /// </summary>
    [HttpGet("download/latest")]
    [AllowAnonymous]
    public IActionResult DownloadLatestVersion()
    {
        var latest = _updateService.GetLatestVersion();
        if (latest == null)
            return NotFound("No versions available");

        var filePath = _updateService.GetVersionFilePath(latest.Version);
        if (filePath == null)
            return NotFound($"Version file not found for {latest.Version}");

        _logger.LogInformation("Serving latest client version: {Version}", latest.Version);
        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Client.exe");
    }

    /// <summary>
    /// Downloads a specific version of the client.
    /// </summary>
    [HttpGet("download/{version}")]
    [AllowAnonymous]
    public IActionResult DownloadVersion(string version)
    {
        var filePath = _updateService.GetVersionFilePath(version);
        if (filePath == null)
            return NotFound($"Version {version} not found");

        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Client.exe");
    }

    /// <summary>
    /// Downloads the updater executable.
    /// </summary>
    [HttpGet("updater")]
    [AllowAnonymous]
    public IActionResult DownloadUpdater()
    {
        var filePath = _updateService.GetUpdaterPath();
        if (filePath == null)
        {
            _logger.LogWarning("Updater not found. Expected at: /root/updates/updater/TorGames.Updater.exe");
            return NotFound("Updater not found. Run deploy.sh to build and install the updater.");
        }

        _logger.LogInformation("Serving updater from: {Path}", filePath);
        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Updater.exe");
    }

    /// <summary>
    /// Uploads a new version of the client.
    /// Version is automatically extracted from the executable's assembly metadata.
    /// </summary>
    /// <param name="file">The executable file</param>
    /// <param name="releaseNotes">Optional release notes</param>
    /// <param name="isTestVersion">Whether this is a test version (default: false)</param>
    [HttpPost("upload")]
    [Authorize]
    [RequestSizeLimit(200_000_000)] // 200MB max
    public async Task<ActionResult<VersionInfo>> UploadVersion(
        [FromForm] IFormFile file,
        [FromForm] string? releaseNotes,
        [FromForm] bool isTestVersion = false)
    {
        if (file == null || file.Length == 0)
            return BadRequest("No file provided");

        if (!file.FileName.EndsWith(".exe", StringComparison.OrdinalIgnoreCase))
            return BadRequest("File must be an executable (.exe)");

        // Save to temp file
        var tempPath = Path.GetTempFileName();
        try
        {
            await using (var fs = System.IO.File.Create(tempPath))
            {
                await file.CopyToAsync(fs);
            }

            // Extract version from assembly metadata
            var extractedVersion = _updateService.ExtractVersionFromFile(tempPath);

            if (string.IsNullOrEmpty(extractedVersion))
            {
                return BadRequest("Could not extract version from executable. Ensure the client was built with proper version metadata (format: YYYY.MM.DD.HHMM)");
            }

            _logger.LogInformation("Extracted version from uploaded file: {Version} (test: {IsTest})", extractedVersion, isTestVersion);

            // Validate version is higher than existing versions of the same type
            if (!_updateService.IsValidNewVersion(extractedVersion, isTestVersion))
            {
                var latestVersion = isTestVersion
                    ? _updateService.GetLatestTestVersion()?.Version ?? "none"
                    : _updateService.GetLatestVersion()?.Version ?? "none";
                return BadRequest($"Version {extractedVersion} is not higher than the latest {(isTestVersion ? "test" : "production")} version ({latestVersion}). Build a newer version of the client.");
            }

            // Add version using temp file
            await using var fileStream = System.IO.File.OpenRead(tempPath);
            var versionInfo = await _updateService.AddVersionAsync(
                fileStream,
                extractedVersion,
                releaseNotes ?? string.Empty,
                User.Identity?.Name ?? "Unknown",
                isTestVersion);

            _logger.LogInformation("{Type} version {Version} uploaded successfully",
                isTestVersion ? "Test" : "Production", extractedVersion);

            // Build the update command - use "update" type with URL for backwards compatibility
            var downloadUrl = $"https://{Request.Host.Host}:{Request.Host.Port ?? 5001}/api/update/download/{extractedVersion}";

            // Only notify clients for production versions (test versions are targeted)
            if (!isTestVersion)
            {
                _ = Task.Run(async () =>
                {
                    try
                    {
                        _logger.LogInformation(
                            "Broadcasting update to {TotalClients} connected clients (Online: {OnlineClients})",
                            _clientManager.ConnectedCount,
                            _clientManager.OnlineCount);

                        // Use "update" command type with URL for backwards compatibility with older clients
                        var updateCommand = new Command
                        {
                            CommandId = Guid.NewGuid().ToString(),
                            CommandType = "update",
                            CommandText = $"{{\"url\":\"{downloadUrl}\"}}",
                            TimeoutSeconds = 300
                        };

                        var clientCount = await _clientManager.BroadcastCommandToAllAsync(updateCommand);
                        _logger.LogInformation(
                            "Notified {ClientCount} clients about new version {Version}",
                            clientCount,
                            extractedVersion);
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning(ex, "Failed to notify clients about new update");
                    }
                });
            }
            else
            {
                // For test versions, notify only test clients
                _ = Task.Run(async () =>
                {
                    try
                    {
                        // Use "update" command type with URL for backwards compatibility with older clients
                        var updateCommand = new Command
                        {
                            CommandId = Guid.NewGuid().ToString(),
                            CommandType = "update",
                            CommandText = $"{{\"url\":\"{downloadUrl}\"}}",
                            TimeoutSeconds = 300
                        };

                        var clientCount = await _clientManager.BroadcastCommandToTestClientsAsync(updateCommand);
                        _logger.LogInformation(
                            "Notified {ClientCount} test clients about new test version {Version}",
                            clientCount,
                            extractedVersion);
                    }
                    catch (Exception ex)
                    {
                        _logger.LogWarning(ex, "Failed to notify test clients about new update");
                    }
                });
            }

            return Ok(versionInfo);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to upload version");
            return StatusCode(500, $"Failed to upload version: {ex.Message}");
        }
        finally
        {
            // Clean up temp file
            if (System.IO.File.Exists(tempPath))
            {
                try { System.IO.File.Delete(tempPath); }
                catch { /* ignore */ }
            }
        }
    }

    /// <summary>
    /// Deletes a version.
    /// </summary>
    [HttpDelete("{version}")]
    [Authorize]
    public ActionResult DeleteVersion(string version)
    {
        try
        {
            var result = _updateService.DeleteVersion(version);
            if (!result)
                return NotFound($"Version {version} not found");

            _logger.LogInformation("Version {Version} deleted", version);
            return Ok();
        }
        catch (InvalidOperationException ex)
        {
            return BadRequest(ex.Message);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to delete version {Version}", version);
            return StatusCode(500, $"Failed to delete version: {ex.Message}");
        }
    }

    /// <summary>
    /// Validates if a version string is valid and higher than existing versions.
    /// </summary>
    [HttpGet("validate/{version}")]
    [AllowAnonymous]
    public ActionResult<bool> ValidateVersion(string version)
    {
        var isValid = _updateService.IsValidNewVersion(version);
        return Ok(isValid);
    }

    /// <summary>
    /// Promotes a test version to production.
    /// </summary>
    [HttpPost("promote/{version}")]
    [Authorize]
    public async Task<ActionResult<VersionInfo>> PromoteVersion(string version)
    {
        try
        {
            var versionInfo = _updateService.PromoteTestToProduction(version);
            if (versionInfo == null)
                return NotFound($"Version {version} not found");

            _logger.LogInformation("Version {Version} promoted to production", version);

            // Notify all clients about the new production version
            _ = Task.Run(async () =>
            {
                try
                {
                    var updateCommand = new Command
                    {
                        CommandId = Guid.NewGuid().ToString(),
                        CommandType = "update_available",
                        CommandText = version,
                        TimeoutSeconds = 0
                    };

                    var clientCount = await _clientManager.BroadcastCommandToAllAsync(updateCommand);
                    _logger.LogInformation(
                        "Notified {ClientCount} clients about promoted version {Version}",
                        clientCount,
                        version);
                }
                catch (Exception ex)
                {
                    _logger.LogWarning(ex, "Failed to notify clients about promoted version");
                }
            });

            return Ok(versionInfo);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "Failed to promote version {Version}", version);
            return StatusCode(500, $"Failed to promote version: {ex.Message}");
        }
    }
}
