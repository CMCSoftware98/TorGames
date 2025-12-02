// TorGames.Binder - Common definitions
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _CRT_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <commdlg.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

// Version info
#include "version.h"

// Custom resource types for binder (must match Stub)
#define RT_BINDER_CONFIG    256
#define RT_BINDER_FILE      257

// Configuration structure for bound files
struct BoundFileConfig {
    std::string filename;
    std::string fullPath;           // Full path to source file
    int executionOrder;
    bool executeFile;
    bool waitForExit;
    bool runHidden;
    bool runAsAdmin;
    std::string extractPath;
    std::string commandLineArgs;
    int executionDelay;
    bool deleteAfterExecution;
    DWORD fileSize;                 // Size of the file

    BoundFileConfig() :
        executionOrder(1),
        executeFile(true),
        waitForExit(false),
        runHidden(false),
        runAsAdmin(false),
        executionDelay(0),
        deleteAfterExecution(false),
        fileSize(0) {}
};

struct BinderConfig {
    int configVersion;
    bool requireAdmin;
    std::string customIcon;
    std::vector<BoundFileConfig> files;

    BinderConfig() : configVersion(1), requireAdmin(false) {}
};

// Window IDs
#define ID_LISTVIEW         1001
#define ID_BTN_ADD          1002
#define ID_BTN_REMOVE       1003
#define ID_BTN_MOVEUP       1004
#define ID_BTN_MOVEDOWN     1005
#define ID_BTN_SETTINGS     1006
#define ID_BTN_BUILD        1007
#define ID_BTN_GLOBAL       1008
#define ID_STATUSBAR        1009
