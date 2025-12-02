// TorGames.Binder - Utility functions
#pragma once

#include "../core/common.h"

namespace Utils {
    // File operations
    bool FileExists(const char* path);
    bool DirectoryExists(const char* path);
    bool CreateDirectoryRecursive(const char* path);
    DWORD GetFileSize(const char* path);
    std::string GetFileExtension(const std::string& filename);
    std::string GetFileName(const std::string& path);

    // Path operations
    std::string GetExecutableDirectory();
    std::string GetTempPath();
    std::string BrowseForFile(HWND parent, const char* filter, const char* title);
    std::string BrowseForFolder(HWND parent, const char* title);
    std::string SaveFileDialog(HWND parent, const char* filter, const char* defaultExt, const char* title);

    // String operations
    std::string WideToUtf8(const wchar_t* wide);
    std::wstring Utf8ToWide(const char* utf8);

    // Resource operations
    bool ExtractResource(HMODULE hModule, int resourceId, int resourceType, const char* outputPath);
    std::vector<BYTE> LoadFileToMemory(const char* path);
}
