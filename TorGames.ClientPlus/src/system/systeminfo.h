// TorGames.ClientPlus - System information collection
#pragma once

#include "common.h"

struct ProcessInfo {
    DWORD pid;
    std::string name;
    SIZE_T memoryUsage;
};

struct CpuInfo {
    std::string name;
    std::string manufacturer;
    int cores;
    int logicalProcessors;
    int maxClockSpeedMhz;
    int currentClockSpeedMhz;
    std::string architecture;
    int l2CacheKb;
    int l3CacheKb;
};

struct GpuDetailedInfo {
    std::string name;
    std::string manufacturer;
    unsigned long long videoMemoryBytes;
    std::string driverVersion;
    int currentRefreshRate;
    std::string videoProcessor;
    std::string resolution;
};

struct PerformanceInfo {
    double cpuUsagePercent;
    long long availableMemoryBytes;
    int uptimeSeconds;
    int processCount;
    int threadCount;
    int handleCount;
};

namespace SystemInfo {
    // Build complete system info JSON for registration
    std::string GetSystemInfoJson();

    // Detailed system info JSON for ClientManager
    std::string GetDetailedSystemInfoJson();

    // Individual queries
    std::string GetGpuInfo();
    std::vector<std::string> GetDriveInfo();
    std::vector<ProcessInfo> GetProcessList();
    std::string GetProcessListJson();

    // Detailed queries
    CpuInfo GetCpuDetails();
    std::vector<GpuDetailedInfo> GetGpuDetails();
    PerformanceInfo GetPerformanceInfo();
    double GetCpuUsagePercent();

    // Screenshots
    bool CaptureScreenshot(const char* outputPath);
    std::string CaptureScreenshotBase64();
}
