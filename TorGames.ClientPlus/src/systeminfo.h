// TorGames.ClientPlus - System information collection
#pragma once

#include "common.h"

struct ProcessInfo {
    DWORD pid;
    std::string name;
    SIZE_T memoryUsage;
};

namespace SystemInfo {
    // Build complete system info JSON for registration
    std::string GetSystemInfoJson();

    // Individual queries
    std::string GetGpuInfo();
    std::vector<std::string> GetDriveInfo();
    std::vector<ProcessInfo> GetProcessList();
    std::string GetProcessListJson();

    // Screenshots
    bool CaptureScreenshot(const char* outputPath);
    std::string CaptureScreenshotBase64();
}
