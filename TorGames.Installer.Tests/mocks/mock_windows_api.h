// TorGames.Installer.Tests - Mock Windows API
#pragma once

#include <gmock/gmock.h>
#include <string>

// Interface for Windows API operations
struct IWindowsApi {
    virtual ~IWindowsApi() = default;
    virtual std::string GetMachineName() = 0;
    virtual std::string GetUsername() = 0;
    virtual std::string GetOsVersion() = 0;
    virtual int GetCpuCount() = 0;
    virtual long long GetTotalMemory() = 0;
};

// Mock implementation for testing
class MockWindowsApi : public IWindowsApi {
public:
    MOCK_METHOD(std::string, GetMachineName, (), (override));
    MOCK_METHOD(std::string, GetUsername, (), (override));
    MOCK_METHOD(std::string, GetOsVersion, (), (override));
    MOCK_METHOD(int, GetCpuCount, (), (override));
    MOCK_METHOD(long long, GetTotalMemory, (), (override));
};
