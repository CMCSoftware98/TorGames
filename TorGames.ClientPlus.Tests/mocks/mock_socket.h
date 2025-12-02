// TorGames.ClientPlus.Tests - Mock Socket
#pragma once

#include <gmock/gmock.h>
#include <string>

// Interface for socket operations
struct ISocket {
    virtual ~ISocket() = default;
    virtual bool Connect(const char* host, int port) = 0;
    virtual void Disconnect() = 0;
    virtual bool IsConnected() const = 0;
    virtual bool Send(const void* data, int len) = 0;
    virtual bool Receive(void* data, int len, int timeoutMs) = 0;
};

// Mock implementation for testing
class MockSocket : public ISocket {
public:
    MOCK_METHOD(bool, Connect, (const char*, int), (override));
    MOCK_METHOD(void, Disconnect, (), (override));
    MOCK_METHOD(bool, IsConnected, (), (const, override));
    MOCK_METHOD(bool, Send, (const void*, int), (override));
    MOCK_METHOD(bool, Receive, (void*, int, int), (override));
};
