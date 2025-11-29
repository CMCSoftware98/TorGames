// TorGames.ClientPlus - Entry point
// Dual-mode client: INSTALLER (first run) and CLIENT (normal operation)

#include "common.h"
#include "client.h"
#include "updater.h"
#include "taskscheduler.h"
#include "logger.h"
#include "utils.h"

// Determine if running from installed location
bool IsInstalledLocation() {
    std::string currentPath = Updater::GetCurrentExePath();
    std::string installPath = Updater::GetInstallPath();

    // Normalize paths for comparison
    return _stricmp(currentPath.c_str(), installPath.c_str()) == 0;
}

// Determine client mode
ClientMode DetermineMode(int argc, char* argv[]) {
    // Check for explicit mode argument
    for (int i = 1; i < argc; i++) {
        if (_stricmp(argv[i], "--installer") == 0 || _stricmp(argv[i], "-i") == 0) {
            return ClientMode::Installer;
        }
        if (_stricmp(argv[i], "--client") == 0 || _stricmp(argv[i], "-c") == 0) {
            return ClientMode::Client;
        }
    }

    // Auto-detect: if not in installed location, assume installer mode
    if (!IsInstalledLocation()) {
        return ClientMode::Installer;
    }

    return ClientMode::Client;
}

// Handle installer mode setup
// In INSTALLER mode, we connect to server and wait for a download command
// The server will send us the URL to download the full CLIENT
bool HandleInstallerMode() {
    LOG_INFO("Running in INSTALLER mode");
    LOG_INFO("Will connect to server and wait for download command...");

    // In installer mode, we don't copy ourselves - we wait for the server
    // to send a download command with the URL to the actual CLIENT binary.
    // The HandleDownload command in commands.cpp handles the download,
    // installation, startup task creation, and launching of the client.

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

int main(int argc, char* argv[]) {
    // Initialize COM for WMI
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);

    // Set up console handler
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    LOG_INFO("TorGames.ClientPlus v%s starting...", CLIENT_VERSION);
    LOG_INFO("Running from: %s", Updater::GetCurrentExePath().c_str());

    // Determine mode
    ClientMode mode = DetermineMode(argc, argv);

    if (mode == ClientMode::Installer) {
        // Handle installation first
        if (!HandleInstallerMode()) {
            LOG_ERROR("Installation failed");
        }

        // Continue running as installer to report to server
        // (allows server to track successful installations)
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
