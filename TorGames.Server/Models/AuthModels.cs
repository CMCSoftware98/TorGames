namespace TorGames.Server.Models;

public class LoginRequest
{
    public required string Password { get; set; }
}

public class LoginResponse
{
    public bool Success { get; set; }
    public string? Token { get; set; }
    public string? Error { get; set; }
    public DateTime? ExpiresAt { get; set; }
}

public class ValidateTokenResponse
{
    public bool Valid { get; set; }
    public DateTime? ExpiresAt { get; set; }
}
