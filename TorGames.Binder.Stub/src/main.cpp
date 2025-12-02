// TorGames.Binder.Stub - Main entry point
// This is the stub executable that gets resources embedded into it by the binder.
// When executed, it extracts and runs the embedded files according to configuration.

#include "common.h"
#include "Extractor.h"
#include "Executor.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Load configuration from embedded resource
    BinderConfig config;
    if (!Extractor::LoadConfig(config)) {
        // No config found - this is probably an unbound stub
        MessageBoxA(NULL, "No bound files found. This executable has not been configured.",
                    "TorGames Binder", MB_OK | MB_ICONWARNING);
        return 1;
    }

    // Check if we need admin privileges
    if (config.requireAdmin && !Executor::IsRunningAsAdmin()) {
        // Relaunch with admin privileges
        if (!Executor::RelaunchAsAdmin()) {
            MessageBoxA(NULL, "This application requires administrator privileges.",
                        "TorGames Binder", MB_OK | MB_ICONERROR);
            return 1;
        }
        return 0; // Exit current instance, new elevated instance will run
    }

    // Create temporary extraction folder
    std::string extractionFolder = Extractor::GetTempExtractionFolder();
    if (!Extractor::CreateExtractionFolder(extractionFolder)) {
        MessageBoxA(NULL, "Failed to create extraction folder.",
                    "TorGames Binder", MB_OK | MB_ICONERROR);
        return 1;
    }

    // Execute all bound files
    bool success = Executor::ExecuteAll(config, extractionFolder);

    // Clean up extraction folder (only files not marked for deletion will remain)
    // Give processes a moment to fully start before cleanup
    Sleep(500);
    Extractor::CleanupExtractionFolder(extractionFolder);

    return success ? 0 : 1;
}
