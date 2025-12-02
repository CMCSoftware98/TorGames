// TorGames.Binder - Main entry point
#include "core/common.h"
#include "ui/MainWindow.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // Initialize COM for shell dialogs
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    MainWindow mainWindow;
    if (!mainWindow.Create(hInstance)) {
        MessageBoxA(NULL, "Failed to create main window.", "Error", MB_OK | MB_ICONERROR);
        CoUninitialize();
        return 1;
    }

    int result = mainWindow.Run();

    CoUninitialize();
    return result;
}
