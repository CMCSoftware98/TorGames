// TorGames.ClientPlus - Main client class
#pragma once

#include "common.h"
#include "protocol.h"

class Client {
public:
    Client();
    ~Client();

    // Initialize and run
    bool Initialize();
    void Run();
    void Stop();

    // Getters
    bool IsRunning() const { return m_running; }

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
    std::string m_hardwareId;

    bool m_running;
    bool m_shouldReconnect;

    HANDLE m_heartbeatThread;
    HANDLE m_messageThread;
    HANDLE m_stopEvent;

    DWORD m_lastHeartbeat;
};
