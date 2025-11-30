// TorGames.ClientPlus - Self-update functionality
#pragma once

#include "common.h"

namespace Updater {
    // Download and apply update
    bool DownloadAndInstall(const char* downloadUrl);

    // Install to proper location
    bool InstallToLocation(const char* targetPath);

    // Self-uninstall
    bool Uninstall();

    // Restart application
    void RestartApplication();

    // Get install path
    std::string GetInstallPath();
    std::string GetInstallDir();
    std::string GetCurrentExePath();
}
