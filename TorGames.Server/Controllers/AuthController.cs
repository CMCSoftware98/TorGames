using Microsoft.AspNetCore.Authorization;
using Microsoft.AspNetCore.Mvc;
using TorGames.Server.Models;
using TorGames.Server.Services;

namespace TorGames.Server.Controllers;

[ApiController]
[Route("api/[controller]")]
public class AuthController : ControllerBase
{
    private readonly JwtService _jwtService;
    private readonly IConfiguration _configuration;
    private readonly ILogger<AuthController> _logger;

    public AuthController(JwtService jwtService, IConfiguration configuration, ILogger<AuthController> logger)
    {
        _jwtService = jwtService;
        _configuration = configuration;
        _logger = logger;
    }

    [HttpPost("login")]
    [AllowAnonymous]
    public IActionResult Login([FromBody] LoginRequest request)
    {
        var masterPassword = _configuration["Auth:MasterPassword"] ?? "123456";

        if (request.Password != masterPassword)
        {
            _logger.LogWarning("Failed login attempt from {IP}", HttpContext.Connection.RemoteIpAddress);
            return Ok(new LoginResponse
            {
                Success = false,
                Error = "Invalid password"
            });
        }

        var (token, expiresAt) = _jwtService.GenerateToken();

        _logger.LogInformation("Successful login from {IP}", HttpContext.Connection.RemoteIpAddress);

        return Ok(new LoginResponse
        {
            Success = true,
            Token = token,
            ExpiresAt = expiresAt
        });
    }

    [HttpPost("validate")]
    [AllowAnonymous]
    public IActionResult ValidateToken([FromHeader(Name = "Authorization")] string? authHeader)
    {
        if (string.IsNullOrEmpty(authHeader) || !authHeader.StartsWith("Bearer "))
        {
            return Ok(new ValidateTokenResponse { Valid = false });
        }

        var token = authHeader["Bearer ".Length..];

        if (_jwtService.ValidateToken(token, out var expiresAt))
        {
            return Ok(new ValidateTokenResponse
            {
                Valid = true,
                ExpiresAt = expiresAt
            });
        }

        return Ok(new ValidateTokenResponse { Valid = false });
    }

    [HttpGet("protected")]
    [Authorize]
    public IActionResult Protected()
    {
        return Ok(new { message = "You are authenticated!", user = User.Identity?.Name });
    }
}
