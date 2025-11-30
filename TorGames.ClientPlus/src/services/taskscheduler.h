// TorGames.ClientPlus - Task Scheduler and Service functionality
#pragma once

#include "common.h"

#define SERVICE_NAME "TorGamesClient"
#define SERVICE_DISPLAY_NAME "TorGames Client Service"

namespace TaskScheduler {
    // Scheduled task functions
    bool AddStartupTask(const char* taskName, const char* exePath);
    bool RemoveStartupTask(const char* taskName);
    bool TaskExists(const char* taskName);

    // Process detection
    bool IsProcessRunning(const char* processName);
    int CountProcessInstances(const char* processName);

    // Windows Service functions
    bool InstallService(const char* serviceName, const char* displayName, const char* exePath);
    bool UninstallService(const char* serviceName);
    bool IsServiceInstalled(const char* serviceName);
    bool StartService(const char* serviceName);
    bool StopService(const char* serviceName);
}
