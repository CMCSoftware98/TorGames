// TorGames.ClientPlus - File explorer implementation
#include "fileexplorer.h"
#include "utils.h"
#include "logger.h"
#include <memory>

namespace FileExplorer {

std::vector<FileEntry> ListDirectory(const char* path) {
    std::vector<FileEntry> entries;

    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

    WIN32_FIND_DATAA fd;
    HANDLE hFind = FindFirstFileA(searchPath, &fd);

    if (hFind == INVALID_HANDLE_VALUE) {
        LOG_ERROR("Failed to list directory: %s (error %lu)", path, GetLastError());
        return entries;
    }

    do {
        if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
            continue;
        }

        FileEntry entry;
        entry.name = fd.cFileName;
        entry.isDirectory = (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.size = (static_cast<long long>(fd.nFileSizeHigh) << 32) | fd.nFileSizeLow;
        entry.modifiedTime = fd.ftLastWriteTime;

        entries.push_back(entry);
    } while (FindNextFileA(hFind, &fd));

    FindClose(hFind);
    return entries;
}

std::string ListDirectoryJson(const char* path) {
    std::vector<FileEntry> entries = ListDirectory(path);
    std::string json = "[";

    for (size_t i = 0; i < entries.size(); i++) {
        if (i > 0) json += ",";

        // Convert FILETIME to Unix timestamp
        ULARGE_INTEGER uli;
        uli.LowPart = entries[i].modifiedTime.dwLowDateTime;
        uli.HighPart = entries[i].modifiedTime.dwHighDateTime;
        long long unixTime = (uli.QuadPart - 116444736000000000LL) / 10000000LL;

        char buf[512];
        snprintf(buf, sizeof(buf),
            "{\"name\":\"%s\",\"isDirectory\":%s,\"size\":%lld,\"modified\":%lld}",
            Utils::JsonEscape(entries[i].name).c_str(),
            entries[i].isDirectory ? "true" : "false",
            entries[i].size,
            unixTime);
        json += buf;
    }

    json += "]";
    return json;
}

bool DeleteFileOrDirectory(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    if (attr == INVALID_FILE_ATTRIBUTES) {
        LOG_ERROR("Path not found: %s", path);
        return false;
    }

    if (attr & FILE_ATTRIBUTE_DIRECTORY) {
        // Recursively delete directory
        char searchPath[MAX_PATH];
        snprintf(searchPath, sizeof(searchPath), "%s\\*", path);

        WIN32_FIND_DATAA fd;
        HANDLE hFind = FindFirstFileA(searchPath, &fd);

        if (hFind != INVALID_HANDLE_VALUE) {
            do {
                if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0) {
                    continue;
                }

                char childPath[MAX_PATH];
                snprintf(childPath, sizeof(childPath), "%s\\%s", path, fd.cFileName);
                DeleteFileOrDirectory(childPath);
            } while (FindNextFileA(hFind, &fd));

            FindClose(hFind);
        }

        return RemoveDirectoryA(path) != 0;
    } else {
        return DeleteFileA(path) != 0;
    }
}

bool DownloadFile(const char* url, const char* outputPath) {
    LOG_INFO("Downloading: %s -> %s", url, outputPath);

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

    HINTERNET hSession = WinHttpOpen(L"TorGames/1.0",
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

bool UploadFile(const char* filePath, const char* uploadUrl) {
    // Read file
    FILE* f = fopen(filePath, "rb");
    if (!f) {
        LOG_ERROR("Failed to open file for upload: %s", filePath);
        return false;
    }

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    fseek(f, 0, SEEK_SET);

    std::unique_ptr<BYTE[]> data(new BYTE[fileSize]);
    fread(data.get(), 1, fileSize, f);
    fclose(f);

    // Parse URL
    URL_COMPONENTSA urlComp = {};
    urlComp.dwStructSize = sizeof(urlComp);

    char hostName[256] = {};
    char urlPath[1024] = {};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = sizeof(hostName);
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = sizeof(urlPath);

    if (!InternetCrackUrlA(uploadUrl, 0, 0, &urlComp)) {
        return false;
    }

    bool isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);

    wchar_t wHostName[256];
    wchar_t wUrlPath[1024];
    MultiByteToWideChar(CP_UTF8, 0, hostName, -1, wHostName, 256);
    MultiByteToWideChar(CP_UTF8, 0, urlPath, -1, wUrlPath, 1024);

    HINTERNET hSession = WinHttpOpen(L"TorGames/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS, 0);

    if (!hSession) {
        return false;
    }

    HINTERNET hConnect = WinHttpConnect(hSession, wHostName,
        urlComp.nPort ? urlComp.nPort : (isHttps ? 443 : 80), 0);

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", wUrlPath,
        nullptr, WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        isHttps ? WINHTTP_FLAG_SECURE : 0);

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool success = WinHttpSendRequest(hRequest,
        L"Content-Type: application/octet-stream", static_cast<DWORD>(-1),
        data.get(), fileSize, fileSize, 0) &&
        WinHttpReceiveResponse(hRequest, nullptr);

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return success;
}

std::string ReadFileContents(const char* path, size_t maxSize) {
    FILE* f = fopen(path, "rb");
    if (!f) return "";

    fseek(f, 0, SEEK_END);
    size_t fileSize = static_cast<size_t>(ftell(f));
    fseek(f, 0, SEEK_SET);

    if (fileSize > maxSize) {
        fileSize = maxSize;
    }

    std::unique_ptr<char[]> data(new char[fileSize + 1]);
    fread(data.get(), 1, fileSize, f);
    data[fileSize] = '\0';
    fclose(f);

    return std::string(data.get());
}

} // namespace FileExplorer
