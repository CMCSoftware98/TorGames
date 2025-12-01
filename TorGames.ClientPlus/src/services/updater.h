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

    // Get paths
    std::string GetInstallPath();
    std::string GetInstallDir();
    std::string GetCurrentExePath();
    std::string GetUpdaterPath();
    std::string GetBackupDir();
}
