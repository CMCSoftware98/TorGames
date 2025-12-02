// TorGames.Binder.Stub - Process execution
#pragma once

#include "common.h"

namespace Executor {
    // Check if running with admin privileges
    bool IsRunningAsAdmin();

    // Relaunch self as administrator
    bool RelaunchAsAdmin();

    // Execute a bound file with its settings
    bool ExecuteFile(const BoundFileConfig& config, const std::string& extractedPath);

    // Execute all files according to configuration
    bool ExecuteAll(const BinderConfig& config, const std::string& extractionFolder);
}
