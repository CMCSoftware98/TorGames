// TorGames.ClientPlus - File explorer operations
#pragma once

#include "common.h"

struct FileEntry {
    std::string name;
    bool isDirectory;
    long long size;
    FILETIME modifiedTime;
};

namespace FileExplorer {
    // List directory contents
    std::vector<FileEntry> ListDirectory(const char* path);
    std::string ListDirectoryJson(const char* path);

    // File operations
    bool DeleteFileOrDirectory(const char* path);
    bool DownloadFile(const char* url, const char* outputPath);
    bool UploadFile(const char* filePath, const char* uploadUrl);

    // Read file contents
    std::string ReadFileContents(const char* path, size_t maxSize = 1024 * 1024);
}
