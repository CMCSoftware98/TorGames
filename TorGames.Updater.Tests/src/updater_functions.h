// TorGames.Updater.Tests - Extracted functions for testing
// These are the pure logic functions that can be tested independently
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <string>

// Forward declarations of functions from main.cpp that we can test
// Note: These are duplicates for testing purposes - in production these live in main.cpp

namespace UpdaterFunctions {

// CreateDirectoryRecursive - creates nested directories
inline bool CreateDirectoryRecursive(const char* path) {
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

// Get the directory portion of a path
inline std::string GetDirectoryFromPath(const char* path) {
    char dir[MAX_PATH];
    strncpy(dir, path, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';

    char* lastSlash = strrchr(dir, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        return dir;
    }
    return "";
}

// Build command line for process creation
inline std::string BuildCommandLine(const char* path, const char* arguments) {
    char cmdLine[MAX_PATH * 2];
    if (arguments && arguments[0]) {
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\" %s", path, arguments);
    } else {
        snprintf(cmdLine, sizeof(cmdLine), "\"%s\"", path);
    }
    return cmdLine;
}

// Generate backup filename with timestamp
inline std::string GenerateBackupFileName(const SYSTEMTIME& st) {
    char backupFileName[128];
    snprintf(backupFileName, sizeof(backupFileName),
        "TorGames.Client_%04d%02d%02d_%02d%02d%02d.exe.bak",
        st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return backupFileName;
}

// Build full backup path
inline std::string BuildBackupPath(const char* backupDir, const char* backupFileName) {
    char backupPath[MAX_PATH];
    snprintf(backupPath, sizeof(backupPath), "%s\\%s", backupDir, backupFileName);
    return backupPath;
}

// Parse command line arguments
struct UpdateArgs {
    std::string targetPath;
    std::string newFilePath;
    std::string backupDir;
    DWORD parentPid;
    std::string version;
    bool isValid;
};

inline UpdateArgs ParseArguments(int argc, char* argv[]) {
    UpdateArgs args = {};
    args.isValid = false;

    if (argc < 5) {
        return args;
    }

    args.targetPath = argv[1];
    args.newFilePath = argv[2];
    args.backupDir = argv[3];
    args.parentPid = (DWORD)atoi(argv[4]);
    if (argc > 5) {
        args.version = argv[5];
    }
    args.isValid = true;

    return args;
}

// Sort backup files by name (descending) - bubble sort for simplicity
inline void SortBackupFilesDescending(std::string files[], int fileCount) {
    for (int i = 0; i < fileCount - 1; i++) {
        for (int j = i + 1; j < fileCount; j++) {
            if (files[i] < files[j]) {
                std::string temp = files[i];
                files[i] = files[j];
                files[j] = temp;
            }
        }
    }
}

} // namespace UpdaterFunctions
