// TorGames.Installer - File download implementation
#include "downloader.h"
#include "utils.h"
#include "logger.h"

namespace Downloader {

std::string GetCurrentExePath() {
    char path[MAX_PATH];
    GetModuleFileNameA(nullptr, path, MAX_PATH);
    return path;
}

std::string GetInstallPath() {
    char localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\TorGames\\TorGames.Client.exe", localAppData);
        return path;
    }
    return "C:\\Users\\Public\\TorGames\\TorGames.Client.exe";
}

std::string GetInstallDir() {
    char localAppData[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, localAppData))) {
        char path[MAX_PATH];
        snprintf(path, sizeof(path), "%s\\TorGames", localAppData);
        return path;
    }
    return "C:\\Users\\Public\\TorGames";
}

bool DownloadFile(const char* url, const char* outputPath) {
    LOG_INFO("Downloading: %s", url);
    LOG_INFO("Target: %s", outputPath);

    // Parse URL
    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {};
    char urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(url, 0, 0, &urlComp)) {
        LOG_ERROR("Failed to parse URL");
        return false;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    // Convert to wide strings for WinHTTP
    wchar_t wHostName[256];
    wchar_t wUrlPath[1024];
    MultiByteToWideChar(CP_UTF8, 0, hostName, -1, wHostName, 256);
    MultiByteToWideChar(CP_UTF8, 0, urlPath, -1, wUrlPath, 1024);

    HINTERNET hSession = WinHttpOpen(L"TorGames.Installer/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        LOG_ERROR("WinHttpOpen failed: %lu", GetLastError());
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, wHostName,
        urlComp.nPort ? urlComp.nPort : (isHttps ? 443 : 80), 0);

    if (!hConnect) {
        LOG_ERROR("WinHttpConnect failed: %lu", GetLastError());
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", wUrlPath,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0);

    if (!hRequest) {
        LOG_ERROR("WinHttpOpenRequest failed: %lu", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        LOG_ERROR("WinHttpSendRequest failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    if (!WinHttpReceiveResponse(hRequest, nullptr)) {
        LOG_ERROR("WinHttpReceiveResponse failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    // Create output directory if needed
    char dir[MAX_PATH];
    strncpy(dir, outputPath, MAX_PATH - 1);
    dir[MAX_PATH - 1] = '\0';
    char* lastSlash = strrchr(dir, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        Utils::CreateDirectoryRecursive(dir);
    }

    FILE* f = fopen(outputPath, "wb");
    if (!f) {
        LOG_ERROR("Failed to create file: %s (error %lu)", outputPath, GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    BYTE buffer[8192];
    DWORD bytesRead;
    DWORD totalRead = 0;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        fwrite(buffer, 1, bytesRead, f);
        totalRead += bytesRead;
    }

    fclose(f);

    LOG_INFO("Downloaded %lu bytes", totalRead);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return totalRead > 0;
}

} // namespace Downloader
