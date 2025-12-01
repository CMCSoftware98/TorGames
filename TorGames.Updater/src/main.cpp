// TorGames.Updater - Crash-proof update helper
// This small executable handles the file replacement when updating TorGames.Client.
// It is designed to NEVER crash - all operations are wrapped in try-catch blocks.
//
// Usage: TorGames.Updater.exe <targetPath> <newFilePath> <backupDir> <parentPid> [version]

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <shlwapi.h>
#include <tlhelp32.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>

#pragma comment(lib, "shlwapi.lib")

// Global log file path
static char g_logPath[MAX_PATH] = {0};

// Forward declarations
void Log(const char* format, ...);
bool WaitForProcessExit(DWORD pid, DWORD timeoutMs);
bool CreateBackup(const char* targetPath, const char* backupDir, char* backupPathOut, size_t backupPathSize);
bool CopyFileWithRetry(const char* source, const char* destination, int maxRetries, int delayMs);
void CleanupOldBackups(const char* backupDir, int keepCount);
bool StartProcess(const char* path, const char* arguments);
void UpdateScheduledTask(const char* clientPath);
bool CreateDirectoryRecursive(const char* path);

void Log(const char* format, ...) {
    try {
        // Get timestamp
        SYSTEMTIME st;
        GetLocalTime(&st);

        char timestamp[32];
        snprintf(timestamp, sizeof(timestamp), "[%02d:%02d:%02d]",
            st.wHour, st.wMinute, st.wSecond);

        // Format message
        char message[4096];
        va_list args;
        va_start(args, format);
        vsnprintf(message, sizeof(message), format, args);
        va_end(args);

        // Print to console
        printf("%s %s\n", timestamp, message);

        // Write to log file
        if (g_logPath[0] != '\0') {
            FILE* f = fopen(g_logPath, "a");
            if (f) {
                fprintf(f, "%s %s\n", timestamp, message);
                fclose(f);
            }
        }
    }
    catch (...) {
        // Never crash on logging errors
    }
}

bool WaitForProcessExit(DWORD pid, DWORD timeoutMs) {
    DWORD startTime = GetTickCount();

    while ((GetTickCount() - startTime) < timeoutMs) {
        HANDLE hProcess = OpenProcess(SYNCHRONIZE, FALSE, pid);
        if (hProcess == NULL) {
            // Process doesn't exist or can't be opened - assume it's gone
            Log("Parent process exited after %lu ms", GetTickCount() - startTime);
            return true;
        }

        // Wait briefly for the process
        DWORD waitResult = WaitForSingleObject(hProcess, 500);
        CloseHandle(hProcess);

        if (waitResult == WAIT_OBJECT_0) {
            Log("Parent process exited after %lu ms", GetTickCount() - startTime);
            return true;
        }
    }

    return false;
}

bool CreateDirectoryRecursive(const char* path) {
    char tmp[MAX_PATH];
    strncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = '\0';

    for (char* p = tmp + 3; *p; p++) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            CreateDirectoryA(tmp, NULL);
            *p = '\\';
        }
    }
    return CreateDirectoryA(tmp, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CreateBackup(const char* targetPath, const char* backupDir, char* backupPathOut, size_t backupPathSize) {
    try {
        // Check if target exists
        if (GetFileAttributesA(targetPath) == INVALID_FILE_ATTRIBUTES) {
            return false;
        }

        // Create backup directory
        CreateDirectoryRecursive(backupDir);

        // Generate timestamp for backup filename
        SYSTEMTIME st;
        GetLocalTime(&st);

        char backupFileName[128];
        snprintf(backupFileName, sizeof(backupFileName),
            "TorGames.Client_%04d%02d%02d_%02d%02d%02d.exe.bak",
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

        snprintf(backupPathOut, backupPathSize, "%s\\%s", backupDir, backupFileName);

        // Copy file
        if (CopyFileA(targetPath, backupPathOut, FALSE)) {
            return true;
        }

        Log("WARNING: Failed to create backup: %lu", GetLastError());
    }
    catch (...) {
        Log("WARNING: Exception creating backup");
    }

    return false;
}

bool CopyFileWithRetry(const char* source, const char* destination, int maxRetries, int delayMs) {
    for (int attempt = 1; attempt <= maxRetries; attempt++) {
        try {
            Log("Copy attempt %d/%d: %s -> %s", attempt, maxRetries, source, destination);

            // Ensure destination directory exists
            char destDir[MAX_PATH];
            strncpy(destDir, destination, MAX_PATH - 1);
            destDir[MAX_PATH - 1] = '\0';
            char* lastSlash = strrchr(destDir, '\\');
            if (lastSlash) {
                *lastSlash = '\0';
                CreateDirectoryRecursive(destDir);
            }

            // Delete existing file if present
            DeleteFileA(destination);

            // Copy new file
            if (CopyFileA(source, destination, FALSE)) {
                // Verify
                if (GetFileAttributesA(destination) != INVALID_FILE_ATTRIBUTES) {
                    Log("Copy successful on attempt %d", attempt);
                    return true;
                }
            }

            Log("Copy attempt %d failed: %lu", attempt, GetLastError());
        }
        catch (...) {
            Log("Copy attempt %d failed with exception", attempt);
        }

        if (attempt < maxRetries) {
            Sleep(delayMs);
        }
    }

    return false;
}

void CleanupOldBackups(const char* backupDir, int keepCount) {
    try {
        char searchPath[MAX_PATH];
        snprintf(searchPath, sizeof(searchPath), "%s\\*.bak", backupDir);

        // Find all backup files
        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath, &fd);

        if (hFind == INVALID_HANDLE_VALUE) {
            return;
        }

        // Collect all backup files
        std::string files[100];
        int fileCount = 0;

        do {
            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && fileCount < 100) {
                char fullPath[MAX_PATH];
                snprintf(fullPath, sizeof(fullPath), "%s\\%s", backupDir, fd.cFileName);
                files[fileCount++] = fullPath;
            }
        } while (FindNextFileA(hFind, &fd));

        FindClose(hFind);

        // Sort by name (descending - newest first due to timestamp format)
        for (int i = 0; i < fileCount - 1; i++) {
            for (int j = i + 1; j < fileCount; j++) {
                if (files[i] < files[j]) {
                    std::string temp = files[i];
                    files[i] = files[j];
                    files[j] = temp;
                }
            }
        }

        // Delete old backups (keep only keepCount)
        for (int i = keepCount; i < fileCount; i++) {
            if (DeleteFileA(files[i].c_str())) {
                Log("Deleted old backup: %s", files[i].c_str());
            }
        }

        Log("Backup cleanup complete. Kept %d most recent backups.", keepCount);
    }
    catch (...) {
        Log("WARNING: Exception during backup cleanup");
    }
}

bool StartProcess(const char* path, const char* arguments) {
    try {
        // Get working directory
        char workDir[MAX_PATH];
        strncpy(workDir, path, MAX_PATH - 1);
        workDir[MAX_PATH - 1] = '\0';
        char* lastSlash = strrchr(workDir, '\\');
        if (lastSlash) {
            *lastSlash = '\0';
        }

        // Build command line
        char cmdLine[MAX_PATH * 2];
        if (arguments && arguments[0]) {
            snprintf(cmdLine, sizeof(cmdLine), "\"%s\" %s", path, arguments);
        } else {
            snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", path);
        }

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi = {};

        if (CreateProcessA(path, cmdLine, NULL, NULL, FALSE, 0, NULL, workDir, &si, &pi)) {
            Log("Started process: %s %s", path, arguments ? arguments : "");
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
            return true;
        }

        Log("WARNING: Failed to start process: %lu", GetLastError());
    }
    catch (...) {
        Log("WARNING: Exception starting process");
    }

    return false;
}

void UpdateScheduledTask(const char* clientPath) {
    const char* taskName = "\\TorGames\\TorGames Client";

    try {
        // Delete existing task
        Log("Deleting existing scheduled task...");

        char deleteCmd[512];
        snprintf(deleteCmd, sizeof(deleteCmd),
            "schtasks.exe /Delete /TN \"%s\" /F", taskName);

        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        PROCESS_INFORMATION pi = {};

        if (CreateProcessA(NULL, deleteCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }

        // Create new task with SYSTEM account (requires admin)
        Log("Creating new scheduled task with SYSTEM account...");

        char createCmd[1024];
        snprintf(createCmd, sizeof(createCmd),
            "schtasks.exe /Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC ONSTART /DELAY 0000:30 /RU SYSTEM /RL HIGHEST /F",
            taskName, clientPath);

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        memset(&pi, 0, sizeof(pi));

        if (CreateProcessA(NULL, createCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            if (exitCode == 0) {
                Log("Scheduled task updated successfully");
                return;
            }

            Log("Admin task creation failed (exit code %lu)", exitCode);
        }

        // Fall back to user-level task
        Log("Falling back to user-level scheduled task...");

        snprintf(createCmd, sizeof(createCmd),
            "schtasks.exe /Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC ONLOGON /DELAY 0000:10 /F",
            taskName, clientPath);

        memset(&si, 0, sizeof(si));
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;
        memset(&pi, 0, sizeof(pi));

        if (CreateProcessA(NULL, createCmd, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            WaitForSingleObject(pi.hProcess, 5000);

            DWORD exitCode = 0;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);

            if (exitCode == 0) {
                Log("User-level task created successfully");
            } else {
                Log("User task creation failed (exit code %lu)", exitCode);
            }
        }
    }
    catch (...) {
        Log("WARNING: Exception updating scheduled task");
    }
}

int RunUpdate(const char* targetPath, const char* newFilePath, const char* backupDir, DWORD parentPid, const char* version) {
    char backupPath[MAX_PATH] = {0};

    try {
        // Step 1: Wait for parent process to exit
        Log("Step 1: Waiting for parent process (PID: %lu) to exit...", parentPid);
        if (!WaitForProcessExit(parentPid, 60000)) {
            Log("WARNING: Parent process did not exit within timeout, proceeding anyway...");
        }

        // Add a small delay to ensure file handles are released
        Sleep(1000);

        // Step 2: Validate paths
        Log("Step 2: Validating paths...");
        if (GetFileAttributesA(newFilePath) == INVALID_FILE_ATTRIBUTES) {
            Log("ERROR: New file does not exist: %s", newFilePath);
            return 1;
        }

        if (GetFileAttributesA(targetPath) == INVALID_FILE_ATTRIBUTES) {
            Log("WARNING: Target file does not exist: %s", targetPath);
        }

        // Step 3: Create backup
        Log("Step 3: Creating backup...");
        if (CreateBackup(targetPath, backupDir, backupPath, sizeof(backupPath))) {
            Log("Backup created at: %s", backupPath);
        } else {
            Log("WARNING: Failed to create backup, continuing without backup");
        }

        // Step 4: Copy new file (with retry logic)
        Log("Step 4: Copying new file...");
        if (!CopyFileWithRetry(newFilePath, targetPath, 5, 1000)) {
            Log("ERROR: Failed to copy new file after retries");

            // Attempt rollback
            if (backupPath[0] && GetFileAttributesA(backupPath) != INVALID_FILE_ATTRIBUTES) {
                Log("Attempting rollback from backup...");
                if (CopyFileWithRetry(backupPath, targetPath, 3, 500)) {
                    Log("Rollback successful");
                } else {
                    Log("ERROR: Rollback also failed!");
                }
            }
            return 1;
        }

        // Step 5: Verify new file
        Log("Step 5: Verifying new file...");
        if (GetFileAttributesA(targetPath) == INVALID_FILE_ATTRIBUTES) {
            Log("ERROR: Target file missing after copy");
            return 1;
        }

        // Get file sizes
        HANDLE hTarget = CreateFileA(targetPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
        HANDLE hSource = CreateFileA(newFilePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

        if (hTarget != INVALID_HANDLE_VALUE && hSource != INVALID_HANDLE_VALUE) {
            LARGE_INTEGER targetSize, sourceSize;
            GetFileSizeEx(hTarget, &targetSize);
            GetFileSizeEx(hSource, &sourceSize);
            CloseHandle(hTarget);
            CloseHandle(hSource);

            if (targetSize.QuadPart != sourceSize.QuadPart) {
                Log("ERROR: File size mismatch. Expected: %lld, Got: %lld", sourceSize.QuadPart, targetSize.QuadPart);
                return 1;
            }

            Log("File verified. Size: %lld bytes", targetSize.QuadPart);
        } else {
            if (hTarget != INVALID_HANDLE_VALUE) CloseHandle(hTarget);
            if (hSource != INVALID_HANDLE_VALUE) CloseHandle(hSource);
            Log("WARNING: Could not verify file sizes");
        }

        // Step 6: Clean up old backups (keep only last 3)
        Log("Step 6: Cleaning up old backups...");
        CleanupOldBackups(backupDir, 3);

        // Step 6.5: Update scheduled task
        Log("Step 6.5: Updating scheduled task...");
        UpdateScheduledTask(targetPath);

        // Step 7: Start new client with --post-update flag
        Log("Step 7: Starting updated client...");
        char clientArgs[256];
        if (version && version[0]) {
            snprintf(clientArgs, sizeof(clientArgs), "--post-update --version \"%s\"", version);
        } else {
            strcpy(clientArgs, "--post-update");
        }

        if (!StartProcess(targetPath, clientArgs)) {
            Log("WARNING: Failed to start updated client");
        }

        Log("========================================");
        Log("Update completed successfully!");
        Log("========================================");
        return 0;
    }
    catch (...) {
        Log("ERROR: Exception during update");

        // Attempt rollback on any error
        if (backupPath[0] && GetFileAttributesA(backupPath) != INVALID_FILE_ATTRIBUTES) {
            Log("Attempting rollback from backup...");
            try {
                if (CopyFileWithRetry(backupPath, targetPath, 3, 500)) {
                    Log("Rollback successful");
                    StartProcess(targetPath, "");
                }
            }
            catch (...) {
                Log("ERROR: Rollback failed");
            }
        }

        return 1;
    }
}

int main(int argc, char* argv[]) {
    try {
        // Set up log file in temp directory
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        snprintf(g_logPath, sizeof(g_logPath), "%sTorGames_Updater.log", tempPath);

        Log("========================================");
        Log("TorGames.Updater started");

        // Log arguments
        std::string argsStr;
        for (int i = 0; i < argc; i++) {
            if (i > 0) argsStr += " ";
            argsStr += argv[i];
        }
        Log("Arguments: %s", argsStr.c_str());

        // Validate arguments
        if (argc < 5) {
            Log("ERROR: Insufficient arguments");
            Log("Usage: TorGames.Updater.exe <targetPath> <newFilePath> <backupDir> <parentPid> [version]");
            return 1;
        }

        const char* targetPath = argv[1];
        const char* newFilePath = argv[2];
        const char* backupDir = argv[3];
        DWORD parentPid = (DWORD)atoi(argv[4]);
        const char* version = (argc > 5) ? argv[5] : NULL;

        if (version) {
            Log("Target version: %s", version);
        }

        // Run the update process
        return RunUpdate(targetPath, newFilePath, backupDir, parentPid, version);
    }
    catch (...) {
        Log("FATAL: Unhandled exception in Main");
        return 1;
    }
}

// Windows subsystem entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
