// TorGames.Installer - Utility functions
#pragma once

#include "common.h"

namespace Utils {
    // String operations
    std::string ToLower(const std::string& str);
    std::string Trim(const std::string& str);
    std::vector<std::string> Split(const std::string& str, char delimiter);

    // JSON helpers (simple parsing)
    std::string JsonEscape(const std::string& str);
    std::string JsonGetString(const char* json, const char* key);
    long long JsonGetInt(const char* json, const char* key);
    bool JsonGetBool(const char* json, const char* key);

    // System info
    std::string GetMachineName();
    std::string GetUsername();
    std::string GetOsVersion();
    std::string GetArchitecture();
    int GetCpuCount();
    long long GetTotalMemory();
    long long GetAvailableMemory();
    std::string GetLocalIp();
    std::string GetMacAddress();
    std::string GetHardwareId();

    // File operations
    bool FileExists(const char* path);
    bool DirectoryExists(const char* path);
    bool CreateDirectoryRecursive(const char* path);

    // Crypto
    std::string Sha256(const std::string& input);
}
