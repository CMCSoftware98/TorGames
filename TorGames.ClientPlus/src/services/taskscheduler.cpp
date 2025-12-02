// TorGames.ClientPlus - Task scheduler and service implementation
#include "taskscheduler.h"
#include "logger.h"
#include <tlhelp32.h>

namespace TaskScheduler {

static bool RunCommand(const char* cmd) {
    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};
    char cmdCopy[2048];
    strncpy(cmdCopy, cmd, sizeof(cmdCopy) - 1);
    cmdCopy[sizeof(cmdCopy) - 1] = '\0';

    if (!CreateProcessA(nullptr, cmdCopy, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        LOG_ERROR("Failed to run command: %lu", GetLastError());
        return false;
    }

    WaitForSingleObject(pi.hProcess, 30000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

static bool RunSchtasks(const char* args) {
    char cmd[2048];
    snprintf(cmd, sizeof(cmd), "schtasks.exe %s", args);
    return RunCommand(cmd);
}

bool AddStartupTask(const char* taskName, const char* exePath) {
    LOG_INFO("Adding startup task: %s -> %s", taskName, exePath);

    // First remove if exists
    RemoveStartupTask(taskName);

    bool success = false;

    // Try to create ONSTART task that runs at system boot (before user login)
    // Uses SYSTEM account to run without requiring user to be logged in
    // This requires admin privileges to create
    char systemArgs[2048];
    snprintf(systemArgs, sizeof(systemArgs),
        "/Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC ONSTART /RU SYSTEM /RL HIGHEST /F",
        taskName, exePath);

    success = RunSchtasks(systemArgs);

    if (success) {
        LOG_INFO("Created ONSTART task with SYSTEM account");

        // Also create a periodic keepalive task with SYSTEM account
        char keepaliveTaskName[256];
        snprintf(keepaliveTaskName, sizeof(keepaliveTaskName), "%s_Keepalive", taskName);
        char keepaliveArgs[2048];
        snprintf(keepaliveArgs, sizeof(keepaliveArgs),
            "/Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC MINUTE /MO 5 /RU SYSTEM /RL HIGHEST /F",
            keepaliveTaskName, exePath);
        RunSchtasks(keepaliveArgs);
    }
    else {
        // Fallback: Create user-level tasks if SYSTEM task creation fails
        // This happens when not running with admin privileges
        LOG_WARN("Failed to create SYSTEM task, falling back to user-level tasks");

        // Create a periodic task that runs every 5 minutes
        char userArgs[2048];
        snprintf(userArgs, sizeof(userArgs),
            "/Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC MINUTE /MO 5 /RL HIGHEST /F",
            taskName, exePath);

        success = RunSchtasks(userArgs);

        // Also create an ONLOGON task for immediate start after login
        if (success) {
            char logonTaskName[256];
            snprintf(logonTaskName, sizeof(logonTaskName), "%s_Logon", taskName);
            char logonArgs[2048];
            snprintf(logonArgs, sizeof(logonArgs),
                "/Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC ONLOGON /RL HIGHEST /F",
                logonTaskName, exePath);
            RunSchtasks(logonArgs);
        }
    }

    return success;
}

bool RemoveStartupTask(const char* taskName) {
    LOG_INFO("Removing startup task: %s", taskName);

    char args[512];
    snprintf(args, sizeof(args), "/Delete /TN \"%s\" /F", taskName);
    bool success = RunSchtasks(args);

    // Also remove the keepalive task
    char keepaliveTaskName[256];
    snprintf(keepaliveTaskName, sizeof(keepaliveTaskName), "%s_Keepalive", taskName);
    char keepaliveArgs[512];
    snprintf(keepaliveArgs, sizeof(keepaliveArgs), "/Delete /TN \"%s\" /F", keepaliveTaskName);
    RunSchtasks(keepaliveArgs);

    // Also remove legacy logon task if it exists
    char logonTaskName[256];
    snprintf(logonTaskName, sizeof(logonTaskName), "%s_Logon", taskName);
    char logonArgs[512];
    snprintf(logonArgs, sizeof(logonArgs), "/Delete /TN \"%s\" /F", logonTaskName);
    RunSchtasks(logonArgs);

    return success;
}

bool TaskExists(const char* taskName) {
    char args[512];
    snprintf(args, sizeof(args), "/Query /TN \"%s\"", taskName);

    return RunSchtasks(args);
}

bool IsProcessRunning(const char* processName) {
    bool found = false;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32 = { sizeof(pe32) };

        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, processName) == 0) {
                    // Don't count ourselves
                    if (pe32.th32ProcessID != GetCurrentProcessId()) {
                        found = true;
                        break;
                    }
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }

    return found;
}

int CountProcessInstances(const char* processName) {
    int count = 0;
    HANDLE hSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);

    if (hSnapshot != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe32 = { sizeof(pe32) };

        if (Process32First(hSnapshot, &pe32)) {
            do {
                if (_stricmp(pe32.szExeFile, processName) == 0) {
                    count++;
                }
            } while (Process32Next(hSnapshot, &pe32));
        }
        CloseHandle(hSnapshot);
    }

    return count;
}

// Windows Service functions
bool InstallService(const char* serviceName, const char* displayName, const char* exePath) {
    LOG_INFO("Installing Windows service: %s", serviceName);

    // Use sc.exe to create the service with auto-restart on failure
    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
        "sc.exe create \"%s\" binPath= \"\\\"%s\\\" --service\" "
        "DisplayName= \"%s\" start= auto",
        serviceName, exePath, displayName);

    if (!RunCommand(cmd)) {
        LOG_ERROR("Failed to create service");
        return false;
    }

    // Configure service recovery - restart on failure
    char recoveryCmd[1024];
    snprintf(recoveryCmd, sizeof(recoveryCmd),
        "sc.exe failure \"%s\" reset= 60 actions= restart/5000/restart/10000/restart/30000",
        serviceName);

    if (!RunCommand(recoveryCmd)) {
        LOG_WARN("Failed to configure service recovery");
    }

    // Set service description
    char descCmd[1024];
    snprintf(descCmd, sizeof(descCmd),
        "sc.exe description \"%s\" \"TorGames Client Service\"",
        serviceName);
    RunCommand(descCmd);

    // Start the service
    char startCmd[512];
    snprintf(startCmd, sizeof(startCmd), "sc.exe start \"%s\"", serviceName);
    RunCommand(startCmd);

    LOG_INFO("Service installed and started successfully");
    return true;
}

bool UninstallService(const char* serviceName) {
    LOG_INFO("Uninstalling Windows service: %s", serviceName);

    // Stop the service first
    char stopCmd[512];
    snprintf(stopCmd, sizeof(stopCmd), "sc.exe stop \"%s\"", serviceName);
    RunCommand(stopCmd);

    // Wait a moment for service to stop
    Sleep(2000);

    // Delete the service
    char deleteCmd[512];
    snprintf(deleteCmd, sizeof(deleteCmd), "sc.exe delete \"%s\"", serviceName);

    return RunCommand(deleteCmd);
}

bool IsServiceInstalled(const char* serviceName) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sc.exe query \"%s\"", serviceName);
    return RunCommand(cmd);
}

bool StartService(const char* serviceName) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sc.exe start \"%s\"", serviceName);
    return RunCommand(cmd);
}

bool StopService(const char* serviceName) {
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "sc.exe stop \"%s\"", serviceName);
    return RunCommand(cmd);
}

} // namespace TaskScheduler
