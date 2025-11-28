# TorGames.InstallerPlus - Ultra-Lightweight Build

Target: **<200KB executable** with no runtime dependencies.

## What Changed from gRPC Version

| Before (gRPC) | After (Raw TCP) |
|--------------|-----------------|
| ~2-5MB | **50-150KB** |
| gRPC + Protobuf libraries | Pure Win32 APIs |
| Complex HTTP/2 protocol | Simple JSON-over-TCP |
| vcpkg dependencies | No external deps |

## Prerequisites

- **Visual Studio 2022** (C++ Desktop workload)
- **CMake 3.20+**
- Optional: **UPX** for compression

## Build Instructions

```powershell
cd TorGames.InstallerPlus

# Configure
cmake -B build -S . -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build --config Release
```

Output: `build/Release/TorGames.InstallerPlus.exe`

## Expected Size

| Build | Size |
|-------|------|
| Debug | ~200-300KB |
| Release | ~80-150KB |
| Release + UPX | **~40-80KB** |

## Optional: UPX Compression

```powershell
# Download UPX from https://upx.github.io/
upx --best --lzma build/Release/TorGames.InstallerPlus.exe
```

## Protocol

Uses simple **JSON-over-TCP** instead of gRPC:

```
Message Format: [4-byte length (big-endian)] + [JSON payload]
```

### Messages

**Registration:**
```json
{
  "type": "registration",
  "clientId": "sha256-hardware-fingerprint",
  "clientType": "INSTALLER",
  "machineName": "DESKTOP-ABC",
  "osVersion": "Windows 10.0.19045",
  "osArch": "x64",
  "cpuCount": 8,
  "totalMemory": 17179869184,
  "username": "User",
  "clientVersion": "1.0.0",
  "ipAddress": "192.168.1.100",
  "macAddress": "AABBCCDDEEFF"
}
```

**Heartbeat:**
```json
{
  "type": "heartbeat",
  "clientId": "...",
  "uptime": 3600,
  "availMemory": 8589934592
}
```

**Command (from server):**
```json
{
  "type": "command",
  "commandId": "uuid",
  "commandType": "shell|powershell|install|uninstall",
  "commandText": "...",
  "timeout": 300
}
```

**Result:**
```json
{
  "type": "result",
  "clientId": "...",
  "commandId": "uuid",
  "success": true,
  "exitCode": 0,
  "stdout": "...",
  "error": ""
}
```

## Server Requirement

The server needs a **TCP listener on port 5050** that speaks this JSON protocol. Add to `TorGames.Server`:

```csharp
// Example: TcpInstallerListener.cs
// Listens on port 5050, parses JSON messages, integrates with ClientManager
```

## Usage

```powershell
# Default (localhost:5050)
TorGames.InstallerPlus.exe

# Custom server
TorGames.InstallerPlus.exe --server 192.168.1.100:5050
```

## Features

- Hardware fingerprint (CPU ID, BIOS, Motherboard, MAC)
- Shell/PowerShell command execution
- Install/Uninstall operations
- Task Scheduler integration
- Auto-reconnect
- Logging to `%TEMP%\TorGames_InstallerPlus.log`

## Why So Small?

1. **No gRPC** - Saves 2-4MB
2. **No C++ STL** - Uses C-style code
3. **Static runtime** - No vcruntime DLLs
4. **Size optimizations** - /O1, /Os, /GS-, LTCG
5. **No exceptions/RTTI** - /EHs-c-, /GR-
