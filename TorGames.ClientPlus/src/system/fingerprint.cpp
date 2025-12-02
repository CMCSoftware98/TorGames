// TorGames.ClientPlus - Hardware fingerprinting implementation
#include "fingerprint.h"
#include "utils.h"
#include "logger.h"
#include <memory>

namespace Fingerprint {

static std::string QueryWmi(const wchar_t* wqlClass, const wchar_t* property) {
    std::string result;

    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) return result;

    hr = CoInitializeSecurity(nullptr, -1, nullptr, nullptr, RPC_C_AUTHN_LEVEL_DEFAULT,
        RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE, nullptr);

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

        hr = CoSetProxyBlanket(svc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr,
            RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);

        wchar_t query[256];
        swprintf(query, 256, L"SELECT %s FROM %s", property, wqlClass);

        hr = svc->ExecQuery(_bstr_t(L"WQL"), _bstr_t(query),
            WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, nullptr, &enumerator);
        if (FAILED(hr) || !enumerator) break;

        ULONG ret = 0;
        if (enumerator->Next(WBEM_INFINITE, 1, &obj, &ret) == S_OK && obj) {
            VARIANT vtProp;
            VariantInit(&vtProp);

            if (SUCCEEDED(obj->Get(property, 0, &vtProp, nullptr, nullptr))) {
                if (vtProp.vt == VT_BSTR && vtProp.bstrVal) {
                    int len = WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0) {
                        std::unique_ptr<char[]> buf(new char[len]);
                        WideCharToMultiByte(CP_UTF8, 0, vtProp.bstrVal, -1, buf.get(), len, nullptr, nullptr);
                        result = buf.get();
                    }
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
    return Utils::Trim(result);
}

std::string GetCpuId() {
    return QueryWmi(L"Win32_Processor", L"ProcessorId");
}

std::string GetBiosSerial() {
    return QueryWmi(L"Win32_BIOS", L"SerialNumber");
}

std::string GetMotherboardSerial() {
    return QueryWmi(L"Win32_BaseBoard", L"SerialNumber");
}

std::string GetMacAddress() {
    std::string result;
    ULONG bufLen = 0;

    if (GetAdaptersInfo(nullptr, &bufLen) == ERROR_BUFFER_OVERFLOW) {
        std::unique_ptr<BYTE[]> buffer(new BYTE[bufLen]);
        PIP_ADAPTER_INFO adapters = reinterpret_cast<PIP_ADAPTER_INFO>(buffer.get());

        if (GetAdaptersInfo(adapters, &bufLen) == NO_ERROR) {
            for (PIP_ADAPTER_INFO adapter = adapters; adapter; adapter = adapter->Next) {
                if (adapter->Type == MIB_IF_TYPE_ETHERNET ||
                    adapter->Type == IF_TYPE_IEEE80211) {
                    char mac[32];
                    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
                        adapter->Address[0], adapter->Address[1], adapter->Address[2],
                        adapter->Address[3], adapter->Address[4], adapter->Address[5]);
                    result = mac;
                    break;
                }
            }
        }
    }
    return result;
}

std::string GetHardwareId() {
    std::string cpuId = GetCpuId();
    std::string biosSerial = GetBiosSerial();
    std::string mbSerial = GetMotherboardSerial();
    std::string mac = GetMacAddress();

    LOG_DEBUG("CPU ID: %s", cpuId.c_str());
    LOG_DEBUG("BIOS Serial: %s", biosSerial.c_str());
    LOG_DEBUG("MB Serial: %s", mbSerial.c_str());
    LOG_DEBUG("MAC: %s", mac.c_str());

    std::string combined = cpuId + "|" + biosSerial + "|" + mbSerial + "|" + mac;
    return Utils::Sha256(combined);
}

} // namespace Fingerprint
