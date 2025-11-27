# TorGames - Complete Project Structure & Architecture

> **Last Updated**: November 2024
> **Purpose**: Remote device management and command execution platform (10,000+ concurrent connections)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Diagram](#architecture-diagram)
3. [Technology Stack](#technology-stack)
4. [Project Structure](#project-structure)
5. [Component Deep Dive](#component-deep-dive)
6. [Communication Protocols](#communication-protocols)
7. [Configuration](#configuration)
8. [Data Flow](#data-flow)
9. [Running the System](#running-the-system)

---

## Overview

TorGames is a distributed system with three main components:

| Component | Description | Technology |
|-----------|-------------|------------|
| **TorGames.Server** | Central server handling gRPC clients + REST/SignalR for web app | .NET 10, ASP.NET Core |
| **torgames-app** | Control panel for managing connected clients | Vue 3, Vuetify, Tauri v2 |
| **TorGames.Client/Installer** | Remote agents that connect to the server | .NET 10, gRPC |

---

## Architecture Diagram

```
                    ┌─────────────────────────────────────────────┐
                    │         Control Panel (torgames-app)        │
                    │  ┌───────────────────────────────────────┐  │
                    │  │  Vue 3 + Vuetify + Pinia + TypeScript │  │
                    │  │                                       │  │
                    │  │  Views:     LoginView, DashboardView  │  │
                    │  │  Services:  api.ts, signalr.ts        │  │
                    │  │  Stores:    auth, clients, settings   │  │
                    │  └───────────────────────────────────────┘  │
                    │  ┌───────────────────────────────────────┐  │
                    │  │        Tauri v2 (Rust Desktop)        │  │
                    │  │     Plugins: store, notification      │  │
                    │  └───────────────────────────────────────┘  │
                    └──────────────────┬──────────────────────────┘
                                       │
                         REST API + SignalR WebSocket
                              (Port 5001)
                                       │
                                       ▼
┌──────────────────────────────────────────────────────────────────────────┐
│                          TorGames.Server                                 │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                        ASP.NET Core (Kestrel)                      │  │
│  │                                                                    │  │
│  │   Port 5000 (HTTP/2)          │    Port 5001 (HTTP/1.1)           │  │
│  │   ├── gRPC Service            │    ├── REST Controllers           │  │
│  │   │   └── TorServiceImpl      │    │   └── AuthController         │  │
│  │   │       ├── Connect()       │    ├── SignalR Hub                │  │
│  │   │       ├── UploadFile()    │    │   └── ClientHub              │  │
│  │   │       └── DownloadFile()  │    └── JWT Authentication         │  │
│  │                               │                                    │  │
│  └────────────────────────────────────────────────────────────────────┘  │
│  ┌────────────────────────────────────────────────────────────────────┐  │
│  │                           Services                                 │  │
│  │                                                                    │  │
│  │   ClientManager (Singleton)       SignalRBroadcastService          │  │
│  │   ├── ConcurrentDictionary        ├── Subscribes to ClientManager  │  │
│  │   ├── Client tracking             └── Broadcasts to web clients    │  │
│  │   ├── Command dispatching                                          │  │
│  │   └── Events for UI updates       ClientCleanupService             │  │
│  │                                   └── Removes stale clients (30s)  │  │
│  │   JwtService                                                       │  │
│  │   └── Token generation/validation                                  │  │
│  └────────────────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────┬───────────────────────────────────┘
                                       │
                            gRPC/HTTP2 (Port 5000)
                            Bidirectional Streaming
                                       │
              ┌────────────────────────┼────────────────────────┐
              │                        │                        │
              ▼                        ▼                        ▼
    ┌─────────────────┐     ┌─────────────────┐     ┌─────────────────┐
    │ TorGames.Client │     │TorGames.Installer│    │  More Clients   │
    │  (CLIENT type)  │     │ (INSTALLER type) │    │      ...        │
    │                 │     │                  │    │                 │
    │ GrpcClientService    │ GrpcInstallerService  │                 │
    │ ├── Registration│     │ ├── Registration │    │                 │
    │ ├── Heartbeat   │     │ ├── Heartbeat    │    │                 │
    │ └── Commands    │     │ └── Install/etc  │    │                 │
    └─────────────────┘     └──────────────────┘    └─────────────────┘
```

---

## Technology Stack

### Backend (.NET 10)

| Component | Technology | Purpose |
|-----------|------------|---------|
| Web Framework | ASP.NET Core + Kestrel | Dual-port server (gRPC + REST) |
| RPC Protocol | gRPC with bidirectional streaming | Client communication |
| Serialization | Protocol Buffers (protobuf3) | Type-safe messaging |
| Real-time Web | SignalR | Dashboard updates |
| Authentication | JWT (JSON Web Tokens) | Secure API access |
| Concurrency | ConcurrentDictionary | Thread-safe client management |

### Frontend (Node.js/TypeScript)

| Component | Technology | Purpose |
|-----------|------------|---------|
| UI Framework | Vue 3 (Composition API) | Reactive UI |
| Component Library | Vuetify 3 | Material Design components |
| State Management | Pinia | Centralized stores |
| Routing | Vue Router | SPA navigation |
| Build Tool | Vite | Fast dev server & bundling |
| Language | TypeScript | Type safety |
| Icons | Material Design Icons (@mdi/font) | UI icons |

### Desktop Wrapper

| Component | Technology | Purpose |
|-----------|------------|---------|
| Framework | Tauri v2 (Rust) | Native desktop app |
| Plugins | tauri-plugin-store | Persistent settings |
| | tauri-plugin-notification | Desktop notifications |

---

## Project Structure

```
TorGames/
│
├── TorGames.Server/                    # ASP.NET Core gRPC + REST Server
│   ├── Program.cs                      # Entry point, Kestrel config, DI setup
│   ├── TorGames.Server.csproj          # Project file (.NET 10)
│   ├── appsettings.json                # Configuration (JWT, timeouts, passwords)
│   │
│   ├── Controllers/
│   │   └── AuthController.cs           # REST: /api/auth/login, /api/auth/validate
│   │
│   ├── Hubs/
│   │   └── ClientHub.cs                # SignalR hub for web dashboard
│   │                                   # Methods: GetAllClients, ExecuteCommand, etc.
│   │
│   ├── Models/
│   │   ├── ConnectedClient.cs          # Client state model (registration, heartbeat)
│   │   ├── AuthModels.cs               # LoginRequest, LoginResponse DTOs
│   │   └── SignalRModels.cs            # ClientDto, CommandResultDto for web
│   │
│   └── Services/
│       ├── TorServiceImpl.cs           # gRPC service implementation
│       │                               # Handles Connect(), UploadFile(), DownloadFile()
│       ├── ClientManager.cs            # Thread-safe client tracking (ConcurrentDictionary)
│       │                               # Events: OnClientConnected, OnClientDisconnected, etc.
│       ├── ClientCleanupService.cs     # Background: removes stale clients (30s timeout)
│       ├── JwtService.cs               # JWT token generation & validation
│       └── SignalRBroadcastService.cs  # Broadcasts ClientManager events to web clients
│
├── TorGames.Common/                    # Shared Library (referenced by all projects)
│   ├── TorGames.Common.csproj          # Project file
│   │
│   ├── Protos/
│   │   └── tor_service.proto           # gRPC contract definition
│   │                                   # ClientMessage, ServerMessage, Command, etc.
│   │
│   ├── Hardware/
│   │   └── MachineFingerprint.cs       # Unique hardware-based client identification
│   │                                   # SHA256(CPU_ID + BIOS + Motherboard + MAC)
│   │
│   └── SimpleBackgroundService.cs      # Base class for background services
│
├── TorGames.Client/                    # gRPC Client Agent
│   ├── Program.cs                      # Entry point (console app)
│   ├── TorGames.Client.csproj          # Project file
│   │
│   └── Services/
│       └── GrpcClientService.cs        # Background service
│                                       # - Connects to server
│                                       # - Sends registration & heartbeats
│                                       # - Executes commands (shell, powershell)
│                                       # - Auto-reconnects on failure
│
├── TorGames.Installer/                 # gRPC Installer Agent (similar to Client)
│   ├── Program.cs                      # Entry point
│   ├── TorGames.Installer.csproj       # Project file
│   │
│   └── Services/
│       └── GrpcInstallerService.cs     # Background service
│                                       # - Same as Client, plus install/uninstall
│
├── torgames-app/                       # Vue 3 + Tauri Desktop Application
│   ├── package.json                    # NPM dependencies & scripts
│   ├── vite.config.ts                  # Vite configuration
│   ├── tsconfig.json                   # TypeScript configuration
│   │
│   ├── src/
│   │   ├── main.ts                     # Vue bootstrap (Pinia, Router, Vuetify)
│   │   ├── App.vue                     # Root component (router-view)
│   │   │
│   │   ├── router/
│   │   │   └── index.ts                # Routes: /login, /dashboard
│   │   │
│   │   ├── views/
│   │   │   ├── LoginView.vue           # Authentication form
│   │   │   └── DashboardView.vue       # Main control panel
│   │   │                               # - Client list with search/filter
│   │   │                               # - Context menus for actions
│   │   │                               # - Command execution interface
│   │   │                               # - System metrics display
│   │   │
│   │   ├── stores/                     # Pinia state management
│   │   │   ├── auth.ts                 # Authentication state (token, login)
│   │   │   ├── clients.ts              # Client list, connection state
│   │   │   └── settings.ts             # App settings, notifications prefs
│   │   │
│   │   ├── services/
│   │   │   ├── api.ts                  # REST API calls (login, validate)
│   │   │   └── signalr.ts              # SignalR connection & event handling
│   │   │                               # - Auto-reconnect (exponential backoff)
│   │   │                               # - Events: ClientConnected, CommandResult
│   │   │
│   │   ├── types/                      # TypeScript interfaces
│   │   │   ├── auth.ts                 # LoginRequest, LoginResponse
│   │   │   ├── client.ts               # ClientDto, CommandResultDto
│   │   │   └── user.ts                 # User types
│   │   │
│   │   ├── components/
│   │   │   └── HelloWorld.vue          # Example component (can be removed)
│   │   │
│   │   └── assets/                     # Static assets (images, etc.)
│   │
│   └── src-tauri/                      # Tauri Rust Backend
│       ├── Cargo.toml                  # Rust dependencies
│       ├── tauri.conf.json             # Tauri configuration
│       ├── build.rs                    # Build script
│       │
│       ├── src/
│       │   └── main.rs                 # Tauri entry point
│       │
│       └── capabilities/               # Security permissions
│           └── default.json            # Allowed Tauri APIs
│
└── docs/                               # Documentation
    ├── PROJECT_STRUCTURE.md            # This file
    ├── IMPLEMENTATION_PLAN.md          # Original implementation plan
    └── NETWORKING_OPTIONS.md           # Technology evaluation
```

---

## Component Deep Dive

### TorGames.Server

#### Program.cs
- Configures dual-port Kestrel server
- Port 5000: HTTP/2 for gRPC (client agents)
- Port 5001: HTTP/1.1 for REST API + SignalR (web app)
- Sets up JWT authentication with query string support for SignalR
- Configures CORS for Tauri app origins
- Registers services: ClientManager, JwtService, CleanupService, SignalR

#### Services/ClientManager.cs
- Central hub for all connected clients
- Uses `ConcurrentDictionary<string, ConnectedClient>` for thread safety
- Connection key format: `{hardwareFingerprint}:{CLIENT_TYPE}`
- Raises events for UI updates:
  - `OnClientConnected`
  - `OnClientDisconnected`
  - `OnClientUpdated`
  - `OnCommandResult`
- Methods: `RegisterClient`, `RemoveClient`, `GetClient`, `BroadcastCommand`, `SendCommand`

#### Services/TorServiceImpl.cs
- Implements `TorService.TorServiceBase` from proto
- `Connect()`: Bidirectional streaming RPC
  - Receives: Registration, Heartbeat, CommandResult, SystemMetrics
  - Sends: Command, FileTransferRequest, ConfigUpdate, Ping
- `UploadFile()`: Client streaming for file uploads
- `DownloadFile()`: Server streaming for file downloads

#### Hubs/ClientHub.cs
- SignalR hub at `/hubs/clients`
- Requires JWT authentication
- Methods available to web clients:
  - `GetAllClients()`: List all known clients
  - `GetOnlineClients()`: List only connected clients
  - `ExecuteCommand(clientId, commandType, commandText)`: Send command to client
  - `BroadcastCommand(commandType, commandText)`: Send to all clients
  - `GetStats()`: Server statistics
- Broadcasts to clients:
  - `ClientConnected`, `ClientDisconnected`
  - `ClientUpdated`, `CommandResult`

### TorGames.Common

#### Protos/tor_service.proto
Defines all message types for gRPC communication:

**Client -> Server:**
- `Registration`: Machine info (name, OS, CPU, memory, IP, MAC)
- `Heartbeat`: System state (uptime, CPU%, memory)
- `CommandResult`: Command execution results (exit code, stdout, stderr)
- `SystemMetrics`: Detailed system info (processes, disk space)

**Server -> Client:**
- `Command`: Instructions (shell, powershell, metrics, etc.)
- `FileTransferRequest`: File upload/download initiation
- `ConfigUpdate`: Settings changes
- `Ping`: Keep-alive
- `ConnectionResponse`: Accept/reject connection

#### Hardware/MachineFingerprint.cs
- Generates unique hardware-based identifier
- SHA256 hash of: CPU ID + BIOS Serial + Motherboard Serial + Primary MAC
- Survives OS reinstalls
- Prevents duplicate connections from same machine

### torgames-app

#### src/services/signalr.ts
- Manages SignalR connection lifecycle
- Auto-reconnect with exponential backoff (1s, 2s, 4s, 8s... max 30s)
- Event handlers for all server broadcasts
- Methods mirror ClientHub for RPC calls

#### src/stores/clients.ts
- Pinia store for client state
- Tracks online/offline clients
- Handles SignalR events to update state
- Provides filtered/sorted client lists

#### src/views/DashboardView.vue
- Main control panel UI
- Features:
  - Client list with search/filter
  - Online/offline/all tabs
  - Context menu for client actions
  - Command execution dialog
  - Real-time metrics display
  - Desktop notifications on connect/disconnect

---

## Communication Protocols

### gRPC (Port 5000)

**Direction**: Clients <-> Server
**Protocol**: HTTP/2 with bidirectional streaming
**Authentication**: Hardware fingerprint (no TLS currently)

```
Client                                Server
  │                                     │
  │──── Connect() stream ──────────────►│
  │                                     │
  │──── Registration ─────────────────►│
  │◄─── ConnectionResponse ────────────│
  │                                     │
  │──── Heartbeat (every 10s) ────────►│
  │◄─── Ping (optional) ───────────────│
  │                                     │
  │◄─── Command ───────────────────────│
  │──── CommandResult ────────────────►│
  │                                     │
```

### REST API (Port 5001)

**Endpoints:**
- `POST /api/auth/login` - Authenticate with master password
- `GET /api/auth/validate` - Validate JWT token
- `GET /api/auth/protected` - Test authorization

### SignalR (Port 5001, /hubs/clients)

**Direction**: Web App <-> Server
**Protocol**: WebSocket (with fallbacks)
**Authentication**: JWT token in query string

**Server -> Client Events:**
- `ClientConnected(ClientDto)`
- `ClientDisconnected(clientId)`
- `ClientUpdated(ClientDto)`
- `CommandResult(CommandResultDto)`

**Client -> Server Methods:**
- `GetAllClients()` -> `List<ClientDto>`
- `GetOnlineClients()` -> `List<ClientDto>`
- `ExecuteCommand(clientId, type, text)` -> `bool`
- `BroadcastCommand(type, text)` -> `int`

---

## Configuration

### Server (appsettings.json)

```json
{
  "Server": {
    "HeartbeatTimeoutSeconds": 30,
    "CleanupIntervalSeconds": 10
  },
  "Jwt": {
    "Secret": "TorGames_SuperSecret_JWT_Key_2024_MustBe32CharsLong!",
    "Issuer": "TorGames.Server",
    "Audience": "TorGames.App",
    "ExpirationHours": 24
  },
  "Auth": {
    "MasterPassword": "123456"
  }
}
```

### Server Limits (Program.cs)

| Setting | Value |
|---------|-------|
| Max Concurrent Connections | 15,000 |
| Max Concurrent Upgraded Connections | 15,000 |
| HTTP/2 Streams per Connection | 100 |
| Connection Window Size | 128KB |
| Stream Window Size | 64KB |

### Client Settings

| Setting | Value |
|---------|-------|
| Server Address | http://localhost:5000 |
| Heartbeat Interval | 10 seconds |
| Reconnect Delay | 5 seconds |

---

## Data Flow

### 1. Client Connection Flow

```
TorGames.Client                    TorGames.Server
     │                                   │
     │── gRPC Connect() ────────────────►│
     │                                   │
     │── Registration message ──────────►│
     │   (machine info, fingerprint)     │ ClientManager.RegisterClient()
     │                                   │ SignalRBroadcastService broadcasts
     │◄── ConnectionResponse ────────────│
     │   (accepted: true)                │
     │                                   │
     │── Heartbeat (every 10s) ─────────►│ ClientManager.UpdateHeartbeat()
     │                                   │
```

### 2. Command Execution Flow

```
torgames-app              TorGames.Server              TorGames.Client
     │                          │                            │
     │── ExecuteCommand() ─────►│                            │
     │   (via SignalR)          │                            │
     │                          │── Command message ────────►│
     │                          │   (via gRPC stream)        │
     │                          │                            │── Execute shell
     │                          │                            │
     │                          │◄── CommandResult ──────────│
     │                          │                            │
     │◄── CommandResult ────────│                            │
     │   (via SignalR)          │                            │
```

### 3. Authentication Flow

```
torgames-app              TorGames.Server
     │                          │
     │── POST /api/auth/login ─►│
     │   {password: "123456"}   │ JwtService.GenerateToken()
     │                          │
     │◄── {token: "eyJ..."} ────│
     │                          │
     │── SignalR Connect ──────►│
     │   ?access_token=eyJ...   │ JWT validation
     │                          │
     │◄── Connection OK ────────│
     │                          │
```

---

## Running the System

### Prerequisites

- .NET 10 SDK
- Node.js 18+
- Yarn (with PnP)
- Rust (for Tauri builds)

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
  REST API on: http://0.0.0.0:5001
  SignalR Hub: /hubs/clients
  Max connections: 15,000
========================================
```

### 2. Start the Web App (Development)

```bash
cd torgames-app
yarn install
yarn dev
```

Opens at http://localhost:5173

### 3. Start the Tauri App (Desktop)

```bash
cd torgames-app
yarn tauri dev
```

### 4. Start Client Agents

```bash
# Terminal 1
cd TorGames.Client
dotnet run

# Terminal 2
cd TorGames.Installer
dotnet run
```

### 5. Login to Dashboard

1. Open http://localhost:5173 or the Tauri app
2. Login with password: `123456`
3. See connected clients in the dashboard

---

## Security Notes

Current implementation is for development/testing:

- Master password is hardcoded (`123456`)
- JWT secret is in config file
- No TLS/HTTPS on gRPC port
- Single password for all users

**For Production:**
- Use environment variables for secrets
- Enable TLS on all ports
- Implement proper user management
- Add role-based access control
- Use secure secret storage

---

## File Reference Quick Index

| Need to... | Look at... |
|------------|------------|
| Add new gRPC message | `TorGames.Common/Protos/tor_service.proto` |
| Handle new command type | `TorGames.Client/Services/GrpcClientService.cs` |
| Add REST endpoint | `TorGames.Server/Controllers/` |
| Add SignalR method | `TorGames.Server/Hubs/ClientHub.cs` |
| Add dashboard feature | `torgames-app/src/views/DashboardView.vue` |
| Modify client state | `torgames-app/src/stores/clients.ts` |
| Change server config | `TorGames.Server/appsettings.json` |
| Modify Tauri settings | `torgames-app/src-tauri/tauri.conf.json` |
