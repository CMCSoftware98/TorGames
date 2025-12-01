// TorGames.Installer - File download functionality
#pragma once

#include "common.h"

namespace Downloader {
    // Download file from URL to local path
    bool DownloadFile(const char* url, const char* outputPath);

    // Get the target install path for the client
    std::string GetInstallPath();
    std::string GetInstallDir();

    // Get the current executable path
    std::string GetCurrentExePath();
}
