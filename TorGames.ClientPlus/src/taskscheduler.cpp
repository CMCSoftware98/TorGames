// TorGames.ClientPlus - Task scheduler implementation
#include "taskscheduler.h"
#include "logger.h"

namespace TaskScheduler {

static bool RunSchtasks(const char* args) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd), "schtasks.exe %s", args);

    STARTUPINFOA si = { sizeof(si) };
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi = {};

    if (!CreateProcessA(nullptr, cmd, nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        LOG_ERROR("Failed to run schtasks: %lu", GetLastError());
        return false;
    }

    WaitForSingleObject(pi.hProcess, 10000);

    DWORD exitCode = 1;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    return exitCode == 0;
}

bool AddStartupTask(const char* taskName, const char* exePath) {
    LOG_INFO("Adding startup task: %s -> %s", taskName, exePath);

    // First remove if exists
    RemoveStartupTask(taskName);

    char args[1024];
    snprintf(args, sizeof(args),
        "/Create /TN \"%s\" /TR \"\\\"%s\\\"\" /SC ONLOGON /RL HIGHEST /F",
        taskName, exePath);

    return RunSchtasks(args);
}

bool RemoveStartupTask(const char* taskName) {
    LOG_INFO("Removing startup task: %s", taskName);

    char args[512];
    snprintf(args, sizeof(args), "/Delete /TN \"%s\" /F", taskName);

    return RunSchtasks(args);
}

bool TaskExists(const char* taskName) {
    char args[512];
    snprintf(args, sizeof(args), "/Query /TN \"%s\"", taskName);

    return RunSchtasks(args);
}

} // namespace TaskScheduler
