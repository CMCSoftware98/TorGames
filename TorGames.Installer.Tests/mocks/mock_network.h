// TorGames.Installer.Tests - Mock Network
#pragma once

#include <gmock/gmock.h>
#include <string>

// Interface for network operations
struct INetwork {
    virtual ~INetwork() = default;
    virtual bool Connect(const char* host, int port) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual bool Send(const void* data, int len) = 0;
    virtual bool Receive(void* data, int len, int timeoutMs) = 0;
    virtual bool DownloadFile(const char* url, const char* path) = 0;
};

// Mock implementation for testing
class MockNetwork : public INetwork {
public:
    MOCK_METHOD(bool, Connect, (const char*, int), (override));
    MOCK_METHOD(void, Disconnect, (), (override));
    MOCK_METHOD(bool, IsConnected, (), (const, override));
    MOCK_METHOD(bool, Send, (const void*, int), (override));
    MOCK_METHOD(bool, Receive, (void*, int, int), (override));
    MOCK_METHOD(bool, DownloadFile, (const char*, const char*), (override));
};
