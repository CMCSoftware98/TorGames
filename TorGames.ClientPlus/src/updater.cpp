// TorGames.ClientPlus - Self-update implementation
#include "updater.h"
#include "fileexplorer.h"
#include "taskscheduler.h"
#include "logger.h"
#include "utils.h"

namespace Updater {

std::string GetCurrentExePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return path;
}

std::string GetInstallPath() {
    char programFiles[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PROGRAM_FILES, nullptr, 0, programFiles))) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\TorGames\\TorGames.Client.exe", programFiles);
        return path;
    }
    return "C:\\Program Files\\TorGames\\TorGames.Client.exe";
}

bool DownloadAndInstall(const char* downloadUrl) {
    LOG_INFO("Downloading update from: %s", downloadUrl);

    // Download to temp location
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    char downloadPath[MAX_PATH];
    snprintf(downloadPath, sizeof(downloadPath), "%sTorGames_Update.exe", tempPath);

    if (!FileExplorer::DownloadFile(downloadUrl, downloadPath)) {
        LOG_ERROR("Failed to download update");
        return false;
    }

    // Verify download
    if (!Utils::FileExists(downloadPath)) {
        LOG_ERROR("Downloaded file not found");
        return false;
    }

    std::string installPath = GetInstallPath();
    LOG_INFO("Installing to: %s", installPath.c_str());

    // Create target directory
    char dir[MAX_PATH];
    strncpy(dir, installPath.c_str(), MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    char* lastSlash = strrchr(dir, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        Utils::CreateDirectoryRecursive(dir);
    }

    // Create batch script to replace running executable
    char batchPath[MAX_PATH];
    snprintf(batchPath, sizeof(batchPath), "%sTorGames_Update.bat", tempPath);

    FILE* f = fopen(batchPath, "w");
    if (!f) {
        LOG_ERROR("Failed to create update script");
        return false;
    }

    fprintf(f,
        "@echo off\n"
        "timeout /t 2 /nobreak > nul\n"
        "copy /Y \"%s\" \"%s\"\n"
        "start \"\" \"%s\"\n"
        "del \"%%~f0\"\n",
        downloadPath, installPath.c_str(), installPath.c_str());
    fclose(f);

    // Run the batch script
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    char cmd[MAX_PATH];
    snprintf(cmd, sizeof(cmd), "cmd.exe /c \"%s\"", batchPath);

    if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        LOG_INFO("Update script launched, exiting...");
        return true;
    }

    LOG_ERROR("Failed to launch update script");
    return false;
}

bool InstallToLocation(const char* targetPath) {
    std::string currentPath = GetCurrentExePath();

    LOG_INFO("Installing from %s to %s", currentPath.c_str(), targetPath);

    // Create directory
    char dir[MAX_PATH];
    strncpy(dir, targetPath, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    char* lastSlash = strrchr(dir, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        Utils::CreateDirectoryRecursive(dir);
    }

    // Copy file
    if (!CopyFileA(currentPath.c_str(), targetPath, FALSE)) {
        LOG_ERROR("Failed to copy file: %lu", GetLastError());
        return false;
    }

    // Add startup task
    TaskScheduler::AddStartupTask("TorGamesClient", targetPath);

    // Launch installed version
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(targetPath, nullptr, nullptr, nullptr, FALSE,
        0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        LOG_INFO("Installed version launched");
        return true;
    }

    return false;
}

bool Uninstall() {
    LOG_INFO("Uninstalling...");

    // Remove startup task
    TaskScheduler::RemoveStartupTask("TorGamesClient");

    std::string installPath = GetInstallPath();
    std::string currentPath = GetCurrentExePath();

    // Create self-delete batch
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    char batchPath[MAX_PATH];
    snprintf(batchPath, sizeof(batchPath), "%sTorGames_Uninstall.bat", tempPath);

    FILE* f = fopen(batchPath, "w");
    if (f) {
        const char* programFiles = getenv("ProgramFiles");
        fprintf(f,
            "@echo off\n"
            "timeout /t 2 /nobreak > nul\n"
            "del /F /Q \"%s\"\n"
            "del /F /Q \"%s\"\n"
            "rmdir \"%s\\TorGames\" 2>nul\n"
            "del \"%%~f0\"\n",
            installPath.c_str(),
            currentPath.c_str(),
            programFiles ? programFiles : "C:\\Program Files");
        fclose(f);

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        PROCESS_INFORMATION pi = {};
        char cmd[MAX_PATH];
        snprintf(cmd, sizeof(cmd), "cmd.exe /c \"%s\"", batchPath);

        if (CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE,
            CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }
    }

    return false;
}

void RestartApplication() {
    std::string exePath = GetCurrentExePath();

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(exePath.c_str(), nullptr, nullptr, nullptr, FALSE,
        0, nullptr, nullptr, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    ExitProcess(0);
}

} // namespace Updater
