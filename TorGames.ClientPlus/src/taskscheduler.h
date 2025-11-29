// TorGames.ClientPlus - Task scheduler operations
#pragma once

#include "common.h"

namespace TaskScheduler {
    // Add startup task using schtasks.exe
    bool AddStartupTask(const char* taskName, const char* exePath);

    // Remove startup task
    bool RemoveStartupTask(const char* taskName);

    // Check if task exists
    bool TaskExists(const char* taskName);
}
