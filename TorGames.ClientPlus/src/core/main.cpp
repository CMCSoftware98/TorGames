// TorGames.ClientPlus - Entry point
// Supports: INSTALLER mode, CLIENT mode, and Windows SERVICE mode

#include "common.h"
#include "client.h"
#include "updater.h"
#include "taskscheduler.h"
#include "logger.h"
#include "utils.h"

// Global service variables
static SERVICE_STATUS g_ServiceStatus = {};
static SERVICE_STATUS_HANDLE g_ServiceStatusHandle = nullptr;
static HANDLE g_ServiceStopEvent = nullptr;
static Client* g_ServiceClient = nullptr;
static bool g_RunningAsService = false;

// Forward declarations
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv);
void WINAPI ServiceCtrlHandler(DWORD ctrlCode);
void RunServiceClient();

// Determine if running from installed location
bool IsInstalledLocation() {
    std::string currentPath = Updater::GetCurrentExePath();
    std::string installPath = Updater::GetInstallPath();
    return _stricmp(currentPath.c_str(), installPath.c_str()) == 0;
}

// Get the executable name from path
std::string GetExeName() {
    std::string path = Updater::GetCurrentExePath();
    size_t pos = path.rfind('\\');
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

// Determine client mode
ClientMode DetermineMode(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--installer") == 0 || _stricmp(argv[i], "-i") == 0) {
            return ClientMode::Installer;
        }
        if (_stricmp(argv[i], "--client") == 0 || _stricmp(argv[i], "-c") == 0) {
            return ClientMode::Client;
        }
    }

    if (!IsInstalledLocation()) {
        return ClientMode::Installer;
    }

    return ClientMode::Client;
}

// Check if --service argument is present
bool IsServiceMode(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--service") == 0 || _stricmp(argv[i], "-s") == 0) {
            return true;
        }
    }
    return false;
}

// Check if --install-service argument is present
bool IsInstallServiceMode(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--install-service") == 0) {
            return true;
        }
    }
    return false;
}

// Check if --uninstall-service argument is present
bool IsUninstallServiceMode(int argc, char* argv[]) {
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--uninstall-service") == 0) {
            return true;
        }
    }
    return false;
}

// Handle installer mode
bool HandleInstallerMode() {
    LOG_INFO("Running in INSTALLER mode");
    LOG_INFO("Will connect to server and wait for download command...");
    return true;
}

// Console control handler
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        LOG_INFO("Received shutdown signal");
        return TRUE;
    }
    return FALSE;
}

// Service control handler
void WINAPI ServiceCtrlHandler(DWORD ctrlCode) {
    switch (ctrlCode) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            LOG_INFO("Service stop requested");
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            g_ServiceStatus.dwWaitHint = 5000;
            SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

            if (g_ServiceClient) {
                g_ServiceClient->Stop();
            }
            if (g_ServiceStopEvent) {
                SetEvent(g_ServiceStopEvent);
            }
            break;

        case SERVICE_CONTROL_INTERROGATE:
            SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
            break;

        default:
            break;
    }
}

// Run the client in service mode
void RunServiceClient() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    LOG_INFO("Service client starting...");

    Client client;
    g_ServiceClient = &client;

    if (client.Initialize(ClientMode::Client)) {
        // Update service status to running
        g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
        g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

        // Run client
        client.Run();
    }

    g_ServiceClient = nullptr;

    // Update service status to stopped
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    g_ServiceStatus.dwWin32ExitCode = 0;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    CoUninitialize();
    LOG_INFO("Service client stopped");
}

// Service main entry point
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerW(L"TorGamesClient", ServiceCtrlHandler);
    if (!g_ServiceStatusHandle) {
        return;
    }

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = 0;
    g_ServiceStatus.dwWin32ExitCode = 0;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 3000;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!g_ServiceStopEvent) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        return;
    }

    RunServiceClient();

    CloseHandle(g_ServiceStopEvent);
}

// Start as Windows Service
bool StartAsService() {
    SERVICE_TABLE_ENTRYW serviceTable[] = {
        { (LPWSTR)L"TorGamesClient", ServiceMain },
        { nullptr, nullptr }
    };

    g_RunningAsService = true;
    return StartServiceCtrlDispatcherW(serviceTable) != 0;
}

int main(int argc, char* argv[]) {
    // Check for service installation/uninstallation first
    if (IsInstallServiceMode(argc, argv)) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        std::string exePath = Updater::GetCurrentExePath();
        bool success = TaskScheduler::InstallService(SERVICE_NAME, SERVICE_DISPLAY_NAME, exePath.c_str());
        CoUninitialize();
        return success ? 0 : 1;
    }

    if (IsUninstallServiceMode(argc, argv)) {
        CoInitializeEx(nullptr, COINIT_MULTITHREADED);
        bool success = TaskScheduler::UninstallService(SERVICE_NAME);
        CoUninitialize();
        return success ? 0 : 1;
    }

    // Check if running as service
    if (IsServiceMode(argc, argv)) {
        if (!StartAsService()) {
            // If service dispatch fails, it might be running interactively
            // Fall through to normal execution
        } else {
            return 0;
        }
    }

    // Initialize COM for WMI
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Set up console handler
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    LOG_INFO("TorGames.ClientPlus v%s starting...", CLIENT_VERSION);
    LOG_INFO("Running from: %s", Updater::GetCurrentExePath().c_str());

    // Determine mode
    ClientMode mode = DetermineMode(argc, argv);

    // Use a named mutex to ensure only one instance runs (across different exe names)
    HANDLE hMutex = CreateMutexA(nullptr, TRUE, "Global\\TorGamesClientMutex");
    if (hMutex == nullptr || GetLastError() == ERROR_ALREADY_EXISTS) {
        LOG_INFO("Another instance is already running (mutex), exiting...");
        if (hMutex) CloseHandle(hMutex);
        CoUninitialize();
        return 0;
    }

    if (mode == ClientMode::Installer) {
        if (!HandleInstallerMode()) {
            LOG_ERROR("Installation failed");
        }
    }

    // Create and run client
    Client client;

    if (!client.Initialize(mode)) {
        LOG_ERROR("Failed to initialize client");
        CoUninitialize();
        return 1;
    }

    // Run client (blocks until stopped)
    client.Run();

    LOG_INFO("Client stopped");

    CoUninitialize();
    return 0;
}

// Windows subsystem entry point (for non-console builds)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
