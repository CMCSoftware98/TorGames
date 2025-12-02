// TorGames.Binder - Utility functions implementation
#include "utils.h"

namespace Utils {

bool FileExists(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && !(attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool DirectoryExists(const char* path) {
    DWORD attrs = GetFileAttributesA(path);
    return (attrs != INVALID_FILE_ATTRIBUTES) && (attrs & FILE_ATTRIBUTE_DIRECTORY);
}

bool CreateDirectoryRecursive(const char* path) {
    char tempPath[MAX_PATH];
    strncpy(tempPath, path, MAX_PATH - 1);
    tempPath[MAX_PATH - 1] = '\0';

    // Replace forward slashes with backslashes
    for (char* p = tempPath; *p; p++) {
        if (*p == '/') *p = '\\';
    }

    // Create each directory in the path
    for (char* p = tempPath; *p; p++) {
        if (*p == '\\') {
            *p = '\0';
            if (tempPath[0] != '\0') {
                CreateDirectoryA(tempPath, NULL);
            }
            *p = '\\';
        }
    }
    return CreateDirectoryA(tempPath, NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

DWORD GetFileSize(const char* path) {
    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return 0;
    }

    DWORD size = ::GetFileSize(hFile, NULL);
    CloseHandle(hFile);
    return size;
}

std::string GetFileExtension(const std::string& filename) {
    size_t pos = filename.rfind('.');
    if (pos == std::string::npos) {
        return "";
    }
    return filename.substr(pos);
}

std::string GetFileName(const std::string& path) {
    size_t pos = path.find_last_of("\\/");
    if (pos == std::string::npos) {
        return path;
    }
    return path.substr(pos + 1);
}

std::string GetExecutableDirectory() {
    char path[MAX_PATH];
    GetModuleFileNameA(NULL, path, MAX_PATH);
    std::string str(path);
    size_t pos = str.find_last_of("\\/");
    if (pos != std::string::npos) {
        return str.substr(0, pos);
    }
    return str;
}

std::string GetTempPath() {
    char path[MAX_PATH];
    ::GetTempPathA(MAX_PATH, path);
    return std::string(path);
}

std::string BrowseForFile(HWND parent, const char* filter, const char* title) {
    char filename[MAX_PATH] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetOpenFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string BrowseForFolder(HWND parent, const char* title) {
    char path[MAX_PATH] = "";

    BROWSEINFOA bi = {};
    bi.hwndOwner = parent;
    bi.lpszTitle = title;
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;

    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        SHGetPathFromIDListA(pidl, path);
        CoTaskMemFree(pidl);
        return std::string(path);
    }
    return "";
}

std::string SaveFileDialog(HWND parent, const char* filter, const char* defaultExt, const char* title) {
    char filename[MAX_PATH] = "";

    OPENFILENAMEA ofn = {};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = parent;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = defaultExt;
    ofn.lpstrTitle = title;
    ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;

    if (GetSaveFileNameA(&ofn)) {
        return std::string(filename);
    }
    return "";
}

std::string WideToUtf8(const wchar_t* wide) {
    if (!wide) return "";
    int size = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    if (size <= 0) return "";
    std::string result(size - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &result[0], size, NULL, NULL);
    return result;
}

std::wstring Utf8ToWide(const char* utf8) {
    if (!utf8) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (size <= 0) return L"";
    std::wstring result(size - 1, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &result[0], size);
    return result;
}

bool ExtractResource(HMODULE hModule, int resourceId, int resourceType, const char* outputPath) {
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(resourceId), MAKEINTRESOURCEA(resourceType));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(hModule, hRes);
    if (!hData) return false;

    LPVOID pData = LockResource(hData);
    DWORD size = SizeofResource(hModule, hRes);
    if (!pData || size == 0) return false;

    HANDLE hFile = CreateFileA(outputPath, GENERIC_WRITE, 0, NULL,
                               CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return false;

    DWORD written;
    BOOL success = WriteFile(hFile, pData, size, &written, NULL);
    CloseHandle(hFile);

    return success && (written == size);
}

std::vector<BYTE> LoadFileToMemory(const char* path) {
    std::vector<BYTE> data;

    HANDLE hFile = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        return data;
    }

    DWORD size = ::GetFileSize(hFile, NULL);
    if (size == INVALID_FILE_SIZE || size == 0) {
        CloseHandle(hFile);
        return data;
    }

    data.resize(size);
    DWORD bytesRead;
    if (!ReadFile(hFile, data.data(), size, &bytesRead, NULL) || bytesRead != size) {
        data.clear();
    }

    CloseHandle(hFile);
    return data;
}

} // namespace Utils
