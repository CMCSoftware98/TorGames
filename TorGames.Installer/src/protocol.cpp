// TorGames.Installer - Network protocol implementation
#include "protocol.h"
#include "utils.h"
#include "logger.h"

Protocol::Protocol() {
    InitializeCriticalSection(&m_sendCs);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

Protocol::~Protocol() {
    Disconnect();
    DeleteCriticalSection(&m_sendCs);
    WSACleanup();
}

bool Protocol::Connect(const char* host, int port) {
    if (m_connected) {
        Disconnect();
    }

    LOG_INFO("Connecting to %s:%d...", host, port);

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket: %d", WSAGetLastError());
        return false;
    }

    // Set socket options
    int timeout = 30000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout, sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout, sizeof(timeout));

    BOOL keepAlive = TRUE;
    setsockopt(m_socket, SOL_SOCKET, SO_KEEPALIVE, (char*)&keepAlive, sizeof(keepAlive));

    // Resolve host
    struct addrinfo hints = {}, *result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char portStr[16];
    snprintf(portStr, sizeof(portStr), "%d", port);

    if (getaddrinfo(host, portStr, &hints, &result) != 0) {
        LOG_ERROR("Failed to resolve host: %s", host);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    if (connect(m_socket, result->ai_addr, (int)result->ai_addrlen) == SOCKET_ERROR) {
        LOG_ERROR("Connect failed: %d", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

    freeaddrinfo(result);
    m_connected = true;
    LOG_INFO("Connected successfully");
    return true;
}

void Protocol::Disconnect() {
    if (m_socket != INVALID_SOCKET) {
        shutdown(m_socket, SD_BOTH);
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
    }
    m_connected = false;
}

bool Protocol::Send(const char* data, int len) {
    if (!m_connected || m_socket == INVALID_SOCKET) {
        return false;
    }

    EnterCriticalSection(&m_sendCs);

    // Send length prefix (4 bytes, big-endian)
    unsigned char lenBytes[4];
    lenBytes[0] = (len >> 24) & 0xFF;
    lenBytes[1] = (len >> 16) & 0xFF;
    lenBytes[2] = (len >> 8) & 0xFF;
    lenBytes[3] = len & 0xFF;

    int sent = send(m_socket, (char*)lenBytes, 4, 0);
    if (sent != 4) {
        LeaveCriticalSection(&m_sendCs);
        m_connected = false;
        return false;
    }

    // Send data
    int totalSent = 0;
    while (totalSent < len) {
        sent = send(m_socket, data + totalSent, len - totalSent, 0);
        if (sent <= 0) {
            LeaveCriticalSection(&m_sendCs);
            m_connected = false;
            return false;
        }
        totalSent += sent;
    }

    LeaveCriticalSection(&m_sendCs);
    LOG_DEBUG("Sent: %s", data);
    return true;
}

bool Protocol::Receive(std::string& data, int timeoutMs) {
    if (!m_connected || m_socket == INVALID_SOCKET) {
        return false;
    }

    // Set timeout
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeoutMs, sizeof(timeoutMs));

    // Read length prefix
    unsigned char lenBytes[4];
    int received = recv(m_socket, (char*)lenBytes, 4, MSG_WAITALL);
    if (received != 4) {
        if (WSAGetLastError() == WSAETIMEDOUT) {
            return false;
        }
        m_connected = false;
        return false;
    }

    int len = (lenBytes[0] << 24) | (lenBytes[1] << 16) | (lenBytes[2] << 8) | lenBytes[3];
    if (len <= 0 || len > BUFFER_SIZE) {
        m_connected = false;
        return false;
    }

    // Read data
    data.resize(len);
    int totalReceived = 0;
    while (totalReceived < len) {
        received = recv(m_socket, &data[totalReceived], len - totalReceived, 0);
        if (received <= 0) {
            m_connected = false;
            return false;
        }
        totalReceived += received;
    }

    LOG_DEBUG("Received: %s", data.c_str());
    return true;
}

bool Protocol::SendRegistration(const char* clientId, const char* machineName,
    const char* osVersion, const char* osArch, int cpuCount, long long totalMemory,
    const char* username, const char* version, const char* ipAddress,
    const char* macAddress) {

    char json[2048];
    snprintf(json, sizeof(json),
        "{\"type\":\"registration\","
        "\"clientId\":\"%s\","
        "\"clientType\":\"%s\","
        "\"machineName\":\"%s\","
        "\"osVersion\":\"%s\","
        "\"osArch\":\"%s\","
        "\"cpuCount\":%d,"
        "\"totalMemory\":%lld,"
        "\"username\":\"%s\","
        "\"clientVersion\":\"%s\","
        "\"ipAddress\":\"%s\","
        "\"macAddress\":\"%s\"}",
        clientId,
        CLIENT_TYPE_INSTALLER,
        machineName,
        osVersion,
        osArch,
        cpuCount,
        totalMemory,
        username,
        version,
        ipAddress,
        macAddress);

    return Send(json, (int)strlen(json));
}

bool Protocol::SendHeartbeat(long long uptime, long long availMemory) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":\"heartbeat\",\"uptime\":%lld,\"availMemory\":%lld}",
        uptime, availMemory);
    return Send(json, (int)strlen(json));
}

bool Protocol::SendCommandResponse(const char* commandId, bool success, const char* result) {
    char json[BUFFER_SIZE];
    snprintf(json, sizeof(json),
        "{\"type\":\"result\","
        "\"commandId\":\"%s\","
        "\"success\":%s,"
        "\"exitCode\":%d,"
        "\"stdout\":\"%s\","
        "\"error\":\"%s\"}",
        commandId,
        success ? "true" : "false",
        success ? 0 : 1,
        Utils::JsonEscape(result).c_str(),
        success ? "" : Utils::JsonEscape(result).c_str());

    return Send(json, (int)strlen(json));
}

bool Protocol::ReceiveMessage(ServerMessage& msg, int timeoutMs) {
    std::string json;
    if (!Receive(json, timeoutMs)) {
        return false;
    }

    msg.type = Utils::JsonGetString(json.c_str(), "type");
    msg.commandId = Utils::JsonGetString(json.c_str(), "commandId");
    msg.commandType = Utils::JsonGetString(json.c_str(), "commandType");
    msg.commandText = Utils::JsonGetString(json.c_str(), "commandText");
    msg.timeout = static_cast<int>(Utils::JsonGetInt(json.c_str(), "timeout"));

    LOG_DEBUG("Received message type: %s", msg.type.c_str());
    return true;
}
