using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
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
    private readonly ILogger<UpdateController> _logger;

    public UpdateController(UpdateService updateService, ILogger<UpdateController> logger)
    {
        _updateService = updateService;
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
    [HttpGet("check")]
    [AllowAnonymous]
    public ActionResult<UpdateCheckResponse> CheckForUpdate([FromQuery] string currentVersion)
    {
        if (string.IsNullOrWhiteSpace(currentVersion))
            return BadRequest("currentVersion is required");

        var response = _updateService.CheckForUpdate(currentVersion);
        return Ok(response);
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
            return NotFound("Updater not found");

        var stream = new FileStream(filePath, FileMode.Open, FileAccess.Read);
        return File(stream, "application/octet-stream", "TorGames.Updater.exe");
    }

    /// <summary>
    /// Uploads a new version of the client.
    /// Version can be provided manually or extracted from file metadata (Windows only).
    /// </summary>
    [HttpPost("upload")]
    [Authorize]
    [RequestSizeLimit(200_000_000)] // 200MB max
    public async Task<ActionResult<VersionInfo>> UploadVersion(
        [FromForm] IFormFile file,
        [FromForm] string? releaseNotes,
        [FromForm] string? version)
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

            // Use provided version or try to extract from file metadata
            var finalVersion = version;
            if (string.IsNullOrEmpty(finalVersion))
            {
                finalVersion = _updateService.ExtractVersionFromFile(tempPath);
            }

            if (string.IsNullOrEmpty(finalVersion))
            {
                return BadRequest("Version is required. Provide it manually in the format YYYY.MM.DD.BUILD (e.g., 2025.11.28.1)");
            }

            // Validate version is higher than existing
            if (!_updateService.IsValidNewVersion(finalVersion))
            {
                return BadRequest($"Version {finalVersion} is not valid or not higher than existing versions. Use format YYYY.MM.DD.BUILD");
            }

            // Add version using temp file
            await using var fileStream = System.IO.File.OpenRead(tempPath);
            var versionInfo = await _updateService.AddVersionAsync(
                fileStream,
                finalVersion,
                releaseNotes ?? string.Empty,
                User.Identity?.Name ?? "Unknown");

            _logger.LogInformation("Version {Version} uploaded successfully", finalVersion);
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
}
