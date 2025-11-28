namespace TorGames.Client.Models;

/// <summary>
/// Represents a file or directory entry for the file explorer.
/// </summary>
public class FileEntryDto
{
    public string Name { get; set; } = string.Empty;
    public string FullPath { get; set; } = string.Empty;
    public long SizeBytes { get; set; }
    public string Type { get; set; } = "file";  // "file", "directory", "drive"
    public string Extension { get; set; } = string.Empty;
    public DateTime CreatedTime { get; set; }
    public DateTime ModifiedTime { get; set; }
    public bool IsDirectory { get; set; }
    public bool IsReadOnly { get; set; }
    public bool IsHidden { get; set; }
    public bool IsSystem { get; set; }
}

/// <summary>
/// Response containing a list of file/directory entries.
/// </summary>
public class DirectoryListingDto
{
    public string Path { get; set; } = string.Empty;
    public List<FileEntryDto> Entries { get; set; } = new();
    public bool Success { get; set; }
    public string? ErrorMessage { get; set; }
}
