// TorGames.ClientPlus - System information implementation
#include "systeminfo.h"
#include "utils.h"
#include "fingerprint.h"
#include "logger.h"
#include <memory>
#include <algorithm>
#include <pdh.h>
#include <pdhmsg.h>
#include <psapi.h>

#pragma comment(lib, "pdh.lib")

namespace SystemInfo {

// Helper to convert BSTR to std::string
static std::string BstrToString(BSTR bstr) {
    if (!bstr) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, bstr, -1, nullptr, 0, nullptr, nullptr);
    if (len <= 0) return "";
    std::unique_ptr<char[]> buf(new char[len]);
    WideCharToMultiByte(CP_UTF8, 0, bstr, -1, buf.get(), len, nullptr, nullptr);
    return buf.get();
}

// Helper to get WMI string property
static std::string GetWmiString(IWbemClassObject* obj, const wchar_t* prop) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    std::string result;
    if (SUCCEEDED(obj->Get(prop, 0, &vtProp, nullptr, nullptr)) && vtProp.bstrVal) {
        result = BstrToString(vtProp.bstrVal);
    }
    VariantClear(&vtProp);
    return result;
}

// Helper to get WMI int property
static int GetWmiInt(IWbemClassObject* obj, const wchar_t* prop) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    int result = 0;
    if (SUCCEEDED(obj->Get(prop, 0, &vtProp, nullptr, nullptr))) {
        if (vtProp.vt == VT_I4) result = vtProp.intVal;
        else if (vtProp.vt == VT_UI4) result = static_cast<int>(vtProp.uintVal);
    }
    VariantClear(&vtProp);
    return result;
}

// Helper to get WMI unsigned long long property
static unsigned long long GetWmiULongLong(IWbemClassObject* obj, const wchar_t* prop) {
    VARIANT vtProp;
    VariantInit(&vtProp);
    unsigned long long result = 0;
    if (SUCCEEDED(obj->Get(prop, 0, &vtProp, nullptr, nullptr))) {
        if (vtProp.vt == VT_BSTR && vtProp.bstrVal) {
            result = _wtoi64(vtProp.bstrVal);
        } else if (vtProp.vt == VT_UI4) {
            result = vtProp.uintVal;
        } else if (vtProp.vt == VT_I4) {
            result = static_cast<unsigned long long>(vtProp.intVal);
        }
    }
    VariantClear(&vtProp);
    return result;
}

std::string GetGpuInfo() {
    std::string result = "Unknown";

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return result;

    IWbemLocator* loc = nullptr;
    IWbemServices* svc = nullptr;
    IEnumWbemClassObject* enumerator = nullptr;
    IWbemClassObject* obj = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<LPVOID*>(&loc));
        if (FAILED(hr) || !loc) break;

        hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &svc);
        if (FAILED(hr) || !svc) break;

        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        hr = svc->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT Name FROM Win32_VideoController"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (FAILED(hr) || !enumerator) break;

        ULONG ret = 0;
        if (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK && obj) {
            result = GetWmiString(obj, L"Name");
            if (result.empty()) result = "Unknown";
        }
    } while (false);

    if (obj) obj->Release();
    if (enumerator) enumerator->Release();
    if (svc) svc->Release();
    if (loc) loc->Release();

    CoUninitialize();
    return result;
}

CpuInfo GetCpuDetails() {
    CpuInfo info = {};
    info.name = "Unknown";
    info.manufacturer = "Unknown";
    info.architecture = Utils::GetArchitecture();
    info.cores = Utils::GetCpuCount();
    info.logicalProcessors = Utils::GetCpuCount();

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return info;

    IWbemLocator* loc = nullptr;
    IWbemServices* svc = nullptr;
    IEnumWbemClassObject* enumerator = nullptr;
    IWbemClassObject* obj = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<LPVOID*>(&loc));
        if (FAILED(hr) || !loc) break;

        hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &svc);
        if (FAILED(hr) || !svc) break;

        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        hr = svc->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT Name, Manufacturer, NumberOfCores, NumberOfLogicalProcessors, MaxClockSpeed, CurrentClockSpeed, L2CacheSize, L3CacheSize FROM Win32_Processor"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (FAILED(hr) || !enumerator) break;

        ULONG ret = 0;
        if (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK && obj) {
            std::string name = GetWmiString(obj, L"Name");
            if (!name.empty()) info.name = name;

            std::string mfr = GetWmiString(obj, L"Manufacturer");
            if (!mfr.empty()) info.manufacturer = mfr;

            int cores = GetWmiInt(obj, L"NumberOfCores");
            if (cores > 0) info.cores = cores;

            int logical = GetWmiInt(obj, L"NumberOfLogicalProcessors");
            if (logical > 0) info.logicalProcessors = logical;

            info.maxClockSpeedMhz = GetWmiInt(obj, L"MaxClockSpeed");
            info.currentClockSpeedMhz = GetWmiInt(obj, L"CurrentClockSpeed");
            info.l2CacheKb = GetWmiInt(obj, L"L2CacheSize");
            info.l3CacheKb = GetWmiInt(obj, L"L3CacheSize");
        }
    } while (false);

    if (obj) obj->Release();
    if (enumerator) enumerator->Release();
    if (svc) svc->Release();
    if (loc) loc->Release();

    CoUninitialize();
    return info;
}

std::vector<GpuDetailedInfo> GetGpuDetails() {
    std::vector<GpuDetailedInfo> gpus;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return gpus;

    IWbemLocator* loc = nullptr;
    IWbemServices* svc = nullptr;
    IEnumWbemClassObject* enumerator = nullptr;

    do {
        hr = CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, reinterpret_cast<LPVOID*>(&loc));
        if (FAILED(hr) || !loc) break;

        hr = loc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), nullptr, nullptr, 0, 0, nullptr, nullptr, &svc);
        if (FAILED(hr) || !svc) break;

        CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        hr = svc->ExecQuery(_bstr_t(L"WQL"),
            _bstr_t(L"SELECT Name, AdapterCompatibility, AdapterRAM, DriverVersion, CurrentRefreshRate, VideoProcessor, CurrentHorizontalResolution, CurrentVerticalResolution FROM Win32_VideoController"),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (FAILED(hr) || !enumerator) break;

        IWbemClassObject* obj = nullptr;
        ULONG ret = 0;
        while (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK && obj) {
            GpuDetailedInfo gpu = {};
            gpu.name = GetWmiString(obj, L"Name");
            gpu.manufacturer = GetWmiString(obj, L"AdapterCompatibility");
            gpu.videoMemoryBytes = GetWmiULongLong(obj, L"AdapterRAM");
            gpu.driverVersion = GetWmiString(obj, L"DriverVersion");
            gpu.currentRefreshRate = GetWmiInt(obj, L"CurrentRefreshRate");
            gpu.videoProcessor = GetWmiString(obj, L"VideoProcessor");

            int hRes = GetWmiInt(obj, L"CurrentHorizontalResolution");
            int vRes = GetWmiInt(obj, L"CurrentVerticalResolution");
            if (hRes > 0 && vRes > 0) {
                char resBuf[64];
                snprintf(resBuf, sizeof(resBuf), "%dx%d", hRes, vRes);
                gpu.resolution = resBuf;
            }

            if (!gpu.name.empty()) {
                gpus.push_back(gpu);
            }
            obj->Release();
        }
    } while (false);

    if (enumerator) enumerator->Release();
    if (svc) svc->Release();
    if (loc) loc->Release();

    CoUninitialize();
    return gpus;
}

double GetCpuUsagePercent() {
    static PDH_HQUERY cpuQuery = nullptr;
    static PDH_HCOUNTER cpuTotal = nullptr;
    static bool initialized = false;

    if (!initialized) {
        PdhOpenQuery(nullptr, 0, &cpuQuery);
        PdhAddEnglishCounterW(cpuQuery, L"\\Processor(_Total)\\% Processor Time", 0, &cpuTotal);
        PdhCollectQueryData(cpuQuery);
        initialized = true;
        Sleep(100); // Need a small delay for first sample
    }

    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery);
    if (PdhGetFormattedCounterValue(cpuTotal, PDH_FMT_DOUBLE, nullptr, &counterVal) == ERROR_SUCCESS) {
        return counterVal.doubleValue;
    }
    return 0.0;
}

PerformanceInfo GetPerformanceInfo() {
    PerformanceInfo info = {};

    info.cpuUsagePercent = GetCpuUsagePercent();
    info.availableMemoryBytes = Utils::GetAvailableMemory();

    // Get uptime
    info.uptimeSeconds = static_cast<int>(GetTickCount64() / 1000);

    // Get process/thread/handle counts
    PERFORMANCE_INFORMATION perfInfo = { sizeof(PERFORMANCE_INFORMATION) };
    if (GetPerformanceInfo(&perfInfo, sizeof(perfInfo))) {
        info.processCount = static_cast<int>(perfInfo.ProcessCount);
        info.threadCount = static_cast<int>(perfInfo.ThreadCount);
        info.handleCount = static_cast<int>(perfInfo.HandleCount);
    }

    return info;
}

std::vector<std::string> GetDriveInfo() {
    std::vector<std::string> drives;
    char buf[256];
    DWORD len = GetLogicalDriveStringsA(sizeof(buf), buf);

    for (char* p = buf; *p; p += strlen(p) + 1) {
        ULARGE_INTEGER freeBytesAvailable, totalBytes, freeBytes;
        if (GetDiskFreeSpaceExA(p, &freeBytesAvailable, &totalBytes, &freeBytes)) {
            char info[128];
            snprintf(info, sizeof(info), "%s %.1fGB/%.1fGB",
                p,
                static_cast<double>(freeBytes.QuadPart) / (1024 * 1024 * 1024),
                static_cast<double>(totalBytes.QuadPart) / (1024 * 1024 * 1024));
            drives.push_back(info);
        }
    }
    return drives;
}

std::vector<ProcessInfo> GetProcessList() {
    std::vector<ProcessInfo> processes;

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE) return processes;

    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(snapshot, &pe)) {
        do {
            ProcessInfo info;
            info.pid = pe.th32ProcessID;
            info.name = pe.szExeFile;
            info.memoryUsage = 0;

            HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                FALSE, pe.th32ProcessID);
            if (hProcess) {
                PROCESS_MEMORY_COUNTERS pmc;
                if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
                    info.memoryUsage = pmc.WorkingSetSize;
                }
                CloseHandle(hProcess);
            }

            processes.push_back(info);
        } while (Process32Next(snapshot, &pe));
    }

    CloseHandle(snapshot);
    return processes;
}

std::string GetProcessListJson() {
    std::vector<ProcessInfo> processes = GetProcessList();
    std::string json = "[";

    for (size_t i = 0; i < processes.size(); i++) {
        if (i > 0) json += ",";
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"pid\":%lu,\"name\":\"%s\",\"memory\":%llu}",
            processes[i].pid,
            Utils::JsonEscape(processes[i].name).c_str(),
            static_cast<unsigned long long>(processes[i].memoryUsage));
        json += buf;
    }

    json += "]";
    return json;
}

std::string GetSystemInfoJson() {
    char buf[4096];

    std::string gpu = GetGpuInfo();
    std::vector<std::string> drives = GetDriveInfo();

    std::string drivesJson = "[";
    for (size_t i = 0; i < drives.size(); i++) {
        if (i > 0) drivesJson += ",";
        drivesJson += "\"" + Utils::JsonEscape(drives[i]) + "\"";
    }
    drivesJson += "]";

    snprintf(buf, sizeof(buf),
        "{"
        "\"machineName\":\"%s\","
        "\"username\":\"%s\","
        "\"osVersion\":\"%s\","
        "\"architecture\":\"%s\","
        "\"cpuCount\":%d,"
        "\"totalMemory\":%lld,"
        "\"availableMemory\":%lld,"
        "\"localIp\":\"%s\","
        "\"countryCode\":\"%s\","
        "\"isAdmin\":%s,"
        "\"uacEnabled\":%s,"
        "\"gpu\":\"%s\","
        "\"drives\":%s"
        "}",
        Utils::JsonEscape(Utils::GetMachineName()).c_str(),
        Utils::JsonEscape(Utils::GetUsername()).c_str(),
        Utils::JsonEscape(Utils::GetOsVersion()).c_str(),
        Utils::GetArchitecture().c_str(),
        Utils::GetCpuCount(),
        Utils::GetTotalMemory(),
        Utils::GetAvailableMemory(),
        Utils::GetLocalIp().c_str(),
        Utils::GetCountryCode().c_str(),
        Utils::IsRunningAsAdmin() ? "true" : "false",
        Utils::IsUacEnabled() ? "true" : "false",
        Utils::JsonEscape(gpu).c_str(),
        drivesJson.c_str());

    return buf;
}

std::string GetDetailedSystemInfoJson() {
    // Get all detailed info
    CpuInfo cpu = GetCpuDetails();
    std::vector<GpuDetailedInfo> gpus = GetGpuDetails();
    PerformanceInfo perf = GetPerformanceInfo();
    std::vector<ProcessInfo> processes = GetProcessList();
    std::vector<std::string> drives = GetDriveInfo();

    // Build CPU JSON
    char cpuJson[1024];
    snprintf(cpuJson, sizeof(cpuJson),
        "{"
        "\"name\":\"%s\","
        "\"manufacturer\":\"%s\","
        "\"cores\":%d,"
        "\"logicalProcessors\":%d,"
        "\"maxClockSpeedMhz\":%d,"
        "\"currentClockSpeedMhz\":%d,"
        "\"architecture\":\"%s\","
        "\"l2CacheKb\":%d,"
        "\"l3CacheKb\":%d"
        "}",
        Utils::JsonEscape(cpu.name).c_str(),
        Utils::JsonEscape(cpu.manufacturer).c_str(),
        cpu.cores,
        cpu.logicalProcessors,
        cpu.maxClockSpeedMhz,
        cpu.currentClockSpeedMhz,
        Utils::JsonEscape(cpu.architecture).c_str(),
        cpu.l2CacheKb,
        cpu.l3CacheKb);

    // Build GPUs JSON array
    std::string gpusJson = "[";
    for (size_t i = 0; i < gpus.size(); i++) {
        if (i > 0) gpusJson += ",";
        char gpuBuf[1024];
        snprintf(gpuBuf, sizeof(gpuBuf),
            "{"
            "\"name\":\"%s\","
            "\"manufacturer\":\"%s\","
            "\"videoMemoryBytes\":%llu,"
            "\"driverVersion\":\"%s\","
            "\"currentRefreshRate\":%d,"
            "\"videoProcessor\":\"%s\","
            "\"resolution\":\"%s\""
            "}",
            Utils::JsonEscape(gpus[i].name).c_str(),
            Utils::JsonEscape(gpus[i].manufacturer).c_str(),
            gpus[i].videoMemoryBytes,
            Utils::JsonEscape(gpus[i].driverVersion).c_str(),
            gpus[i].currentRefreshRate,
            Utils::JsonEscape(gpus[i].videoProcessor).c_str(),
            Utils::JsonEscape(gpus[i].resolution).c_str());
        gpusJson += gpuBuf;
    }
    gpusJson += "]";

    // Build Memory JSON
    long long totalMem = Utils::GetTotalMemory();
    long long availMem = Utils::GetAvailableMemory();
    int memLoadPercent = totalMem > 0 ? static_cast<int>((1.0 - static_cast<double>(availMem) / totalMem) * 100) : 0;

    char memoryJson[512];
    snprintf(memoryJson, sizeof(memoryJson),
        "{"
        "\"totalPhysicalBytes\":%lld,"
        "\"availablePhysicalBytes\":%lld,"
        "\"memoryLoadPercent\":%d"
        "}",
        totalMem,
        availMem,
        memLoadPercent);

    // Build Performance JSON with top processes
    std::string topProcessesJson = "[";
    // Sort processes by memory and take top 50
    std::sort(processes.begin(), processes.end(),
        [](const ProcessInfo& a, const ProcessInfo& b) { return a.memoryUsage > b.memoryUsage; });
    int procCount = std::min(static_cast<int>(processes.size()), 50);
    for (int i = 0; i < procCount; i++) {
        if (i > 0) topProcessesJson += ",";
        char procBuf[256];
        snprintf(procBuf, sizeof(procBuf),
            "{\"pid\":%lu,\"name\":\"%s\",\"cpuPercent\":0,\"memoryBytes\":%llu}",
            processes[i].pid,
            Utils::JsonEscape(processes[i].name).c_str(),
            static_cast<unsigned long long>(processes[i].memoryUsage));
        topProcessesJson += procBuf;
    }
    topProcessesJson += "]";

    char perfJson[1024];
    snprintf(perfJson, sizeof(perfJson),
        "{"
        "\"cpuUsagePercent\":%.1f,"
        "\"availableMemoryBytes\":%lld,"
        "\"uptimeSeconds\":%d,"
        "\"processCount\":%d,"
        "\"threadCount\":%d,"
        "\"handleCount\":%d,"
        "\"topProcesses\":%s"
        "}",
        perf.cpuUsagePercent,
        perf.availableMemoryBytes,
        perf.uptimeSeconds,
        perf.processCount,
        perf.threadCount,
        perf.handleCount,
        topProcessesJson.c_str());

    // Build OS JSON
    char osJson[512];
    snprintf(osJson, sizeof(osJson),
        "{"
        "\"name\":\"Windows\","
        "\"version\":\"%s\","
        "\"architecture\":\"%s\","
        "\"registeredUser\":\"%s\""
        "}",
        Utils::JsonEscape(Utils::GetOsVersion()).c_str(),
        Utils::JsonEscape(Utils::GetArchitecture()).c_str(),
        Utils::JsonEscape(Utils::GetUsername()).c_str());

    // Build Drives JSON array
    std::string disksJson = "[";
    for (size_t i = 0; i < drives.size(); i++) {
        if (i > 0) disksJson += ",";
        // Parse drive string "C: 42.5GB/120GB"
        std::string driveStr = drives[i];
        std::string letter = driveStr.length() >= 2 ? driveStr.substr(0, 2) : "";
        char diskBuf[256];
        snprintf(diskBuf, sizeof(diskBuf),
            "{\"name\":\"%s\",\"driveLetter\":\"%s\"}",
            Utils::JsonEscape(driveStr).c_str(),
            Utils::JsonEscape(letter).c_str());
        disksJson += diskBuf;
    }
    disksJson += "]";

    // Build Network JSON
    std::string localIp = Utils::GetLocalIp();
    char networkJson[256];
    snprintf(networkJson, sizeof(networkJson),
        "[{\"name\":\"Local Network\",\"ipAddress\":\"%s\",\"status\":\"Up\"}]",
        Utils::JsonEscape(localIp).c_str());

    // Build final JSON
    char* result = new char[16384];
    snprintf(result, 16384,
        "{"
        "\"cpu\":%s,"
        "\"gpus\":%s,"
        "\"memory\":%s,"
        "\"performance\":%s,"
        "\"os\":%s,"
        "\"disks\":%s,"
        "\"networkAdapters\":%s,"
        "\"machineName\":\"%s\","
        "\"isAdmin\":%s,"
        "\"uacEnabled\":%s"
        "}",
        cpuJson,
        gpusJson.c_str(),
        memoryJson,
        perfJson,
        osJson,
        disksJson.c_str(),
        networkJson,
        Utils::JsonEscape(Utils::GetMachineName()).c_str(),
        Utils::IsRunningAsAdmin() ? "true" : "false",
        Utils::IsUacEnabled() ? "true" : "false");

    std::string jsonResult = result;
    delete[] result;
    return jsonResult;
}

bool CaptureScreenshot(const char* outputPath) {
    // Get screen dimensions
    int width = GetSystemMetrics(SM_CXSCREEN);
    int height = GetSystemMetrics(SM_CYSCREEN);

    HDC screenDC = GetDC(nullptr);
    HDC memDC = CreateCompatibleDC(screenDC);
    HBITMAP bitmap = CreateCompatibleBitmap(screenDC, width, height);
    HGDIOBJ oldBitmap = SelectObject(memDC, bitmap);

    BitBlt(memDC, 0, 0, width, height, screenDC, 0, 0, SRCCOPY);

    // Save as BMP (simple format, no extra dependencies)
    BITMAPFILEHEADER bfh = {};
    BITMAPINFOHEADER bih = {};

    bih.biSize = sizeof(bih);
    bih.biWidth = width;
    bih.biHeight = -height;  // Top-down
    bih.biPlanes = 1;
    bih.biBitCount = 24;
    bih.biCompression = BI_RGB;

    int stride = ((width * 3 + 3) / 4) * 4;
    int imageSize = stride * height;

    bfh.bfType = 0x4D42;  // "BM"
    bfh.bfSize = sizeof(bfh) + sizeof(bih) + imageSize;
    bfh.bfOffBits = sizeof(bfh) + sizeof(bih);

    std::unique_ptr<BYTE[]> pixels(new BYTE[imageSize]);
    GetDIBits(memDC, bitmap, 0, height, pixels.get(), reinterpret_cast<BITMAPINFO*>(&bih), DIB_RGB_COLORS);

    bool success = false;
    FILE* f = fopen(outputPath, "wb");
    if (f) {
        fwrite(&bfh, sizeof(bfh), 1, f);
        fwrite(&bih, sizeof(bih), 1, f);
        fwrite(pixels.get(), imageSize, 1, f);
        fclose(f);
        success = true;
    }

    SelectObject(memDC, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memDC);
    ReleaseDC(nullptr, screenDC);

    return success;
}

std::string CaptureScreenshotBase64() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);
    char filePath[MAX_PATH];
    snprintf(filePath, sizeof(filePath), "%stg_screenshot.bmp", tempPath);

    // RAII helper to ensure temp file is always deleted
    struct TempFileDeleter {
        const char* path;
        ~TempFileDeleter() { DeleteFileA(path); }
    } fileDeleter{filePath};

    if (!CaptureScreenshot(filePath)) {
        return "";
    }

    // Read file and base64 encode
    FILE* f = fopen(filePath, "rb");
    if (!f) return "";

    fseek(f, 0, SEEK_END);
    long fileSize = ftell(f);
    if (fileSize <= 0) {
        fclose(f);
        return "";
    }
    fseek(f, 0, SEEK_SET);

    std::unique_ptr<BYTE[]> data(new BYTE[fileSize]);
    size_t bytesRead = fread(data.get(), 1, fileSize, f);
    fclose(f);

    if (bytesRead != static_cast<size_t>(fileSize)) {
        return "";
    }

    // Simple base64 encoding
    static const char* b64chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result;
    result.reserve((fileSize + 2) / 3 * 4);

    for (long i = 0; i < fileSize; i += 3) {
        int val = (static_cast<unsigned char>(data[i]) << 16);
        if (i + 1 < fileSize) val |= (static_cast<unsigned char>(data[i + 1]) << 8);
        if (i + 2 < fileSize) val |= static_cast<unsigned char>(data[i + 2]);

        result += b64chars[(val >> 18) & 0x3F];
        result += b64chars[(val >> 12) & 0x3F];
        result += (i + 1 < fileSize) ? b64chars[(val >> 6) & 0x3F] : '=';
        result += (i + 2 < fileSize) ? b64chars[val & 0x3F] : '=';
    }

    return result;
}

} // namespace SystemInfo
