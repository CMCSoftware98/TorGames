# TorGames Server Implementation - COMPLETED

## Overview

This document describes the implemented gRPC-based communication system for TorGames, featuring bidirectional streaming between the server and clients (Installer + Client).

---

## Project Structure

```
TorGames/
├── TorGames.Server/              # gRPC Server
│   ├── Program.cs                # Entry point with Kestrel config
│   ├── TorGames.Server.csproj
│   ├── Services/
│   │   ├── TorServiceImpl.cs     # gRPC service implementation
│   │   ├── ClientManager.cs      # Manages connected clients
│   │   └── ClientCleanupService.cs # Stale client cleanup
│   ├── Models/
│   │   └── ConnectedClient.cs    # Client connection state
│   └── appsettings.json
│
├── TorGames.Common/              # Shared Library
│   ├── TorGames.Common.csproj
│   ├── Protos/
│   │   └── tor_service.proto     # gRPC contract
│   ├── Hardware/
│   │   └── MachineFingerprint.cs # Hardware-based client ID
│   └── Services/
│       └── SimpleBackgroundService.cs
│
├── TorGames.Client/              # gRPC Client
│   ├── Program.cs
│   ├── TorGames.Client.csproj
│   └── Services/
│       └── GrpcClientService.cs
│
├── TorGames.Installer/           # gRPC Installer Client
│   ├── Program.cs
│   ├── TorGames.Installer.csproj
│   └── Services/
│       └── GrpcInstallerService.cs
│
└── docs/
    ├── NETWORKING_OPTIONS.md
    └── IMPLEMENTATION_PLAN.md
```

---

## Configuration

### Server (Port 5000, HTTP/2, No TLS)

| Setting | Value |
|---------|-------|
| Port | 5000 |
| Protocol | HTTP/2 (no TLS) |
| Max Connections | 15,000 |
| Heartbeat Timeout | 30 seconds |
| Cleanup Interval | 10 seconds |

### Clients

| Setting | Value |
|---------|-------|
| Server Address | http://localhost:5000 |
| Heartbeat Interval | 10 seconds |
| Reconnect Delay | 5 seconds |

---

## Client Identification

Clients are identified using a **hardware fingerprint hash**:

```
SHA256(CPU_ID + BIOS_Serial + Motherboard_Serial + Primary_MAC)
```

- Survives application and OS reinstalls
- Unique per physical machine
- Connection key: `{fingerprint}:{CLIENT_TYPE}`
- Duplicate connections are rejected

---

## Supported Commands

### Common (Both Client & Installer)

| Command Type | Description |
|--------------|-------------|
| `shell` / `cmd` | Execute Windows command |
| `powershell` / `ps` | Execute PowerShell command |
| `metrics` | Request system metrics |

### Installer Only

| Command Type | Description |
|--------------|-------------|
| `install` | Run installation |
| `uninstall` | Run uninstallation |
| `update` | Self-update |

Commands are **extensible** - new types can be added by:
1. Adding handler in client service
2. No proto changes required (string-based command types)

---

## Running the System

### 1. Start the Server

```bash
cd TorGames.Server
dotnet run
```

Output:
```
========================================
  TorGames Server Started
  gRPC listening on: http://0.0.0.0:5000
  Max connections: 15,000
========================================
```

### 2. Start a Client

```bash
cd TorGames.Client
dotnet run
```

### 3. Start the Installer

```bash
cd TorGames.Installer
dotnet run
```

---

## Architecture

### Message Flow

```
┌─────────────────┐         ┌─────────────────┐
│  TorGames.Client │◄───────►│ TorGames.Server │
│  (CLIENT type)   │  gRPC   │                 │
└─────────────────┘  Stream  │   ClientManager │
                             │   ┌───────────┐ │
┌─────────────────┐          │   │ Client 1  │ │
│TorGames.Installer│◄────────►│   │ Client 2  │ │
│ (INSTALLER type) │  gRPC   │   │ ...       │ │
└─────────────────┘  Stream  │   └───────────┘ │
                             └─────────────────┘
```

### Connection Lifecycle

1. Client connects and sends `Registration` message
2. Server validates (rejects if duplicate connection key)
3. Server sends `ConnectionResponse` (accepted/rejected)
4. Client enters heartbeat loop (every 10 seconds)
5. Server can send commands at any time
6. Client processes commands and returns `CommandResult`
7. On disconnect, server removes client from `ClientManager`

---

## Key Components

### TorGames.Common

- **tor_service.proto**: Defines all message types and gRPC service
- **MachineFingerprint**: Generates persistent hardware-based client ID

### TorGames.Server

- **TorServiceImpl**: Handles gRPC Connect, UploadFile, DownloadFile
- **ClientManager**: Thread-safe client tracking with ConcurrentDictionary
- **ClientCleanupService**: Background service removing stale clients
- **ConnectedClient**: Stores client state and provides SendCommand methods

### TorGames.Client / TorGames.Installer

- **GrpcClientService / GrpcInstallerService**: Background services that:
  - Connect to server
  - Send registration
  - Maintain heartbeat
  - Process incoming commands
  - Auto-reconnect on failure

---

## File Transfer

The proto supports file transfer via streaming:

- **UploadFile**: Client streams FileChunks to server
- **DownloadFile**: Server streams FileChunks to client
- **Chunk size**: 64KB

---

## Future Enhancements

- [ ] Add CPU usage monitoring
- [ ] Add disk space monitoring
- [ ] Implement full file transfer in clients
- [ ] Add database persistence for client history
- [ ] Add authentication/authorization
- [ ] Add web dashboard for server management
