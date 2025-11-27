# TorGames Server Networking Options

## Overview

This document evaluates three networking approaches for building a high-performance .NET server capable of handling **10,000+ concurrent connections** with bidirectional real-time communication.

### Requirements
- Scale to 10,000+ concurrent connections on a single server
- Bidirectional real-time communication
- Support file transfers, commands, and unrestricted binary data
- .NET-to-.NET communication (Installer and Client applications)
- Fast development priority

---

## Option 1: gRPC with Bidirectional Streaming (RECOMMENDED)

### Overview
gRPC is Google's high-performance RPC framework using Protocol Buffers for serialization and HTTP/2 for transport. It provides strongly-typed contracts and native support for bidirectional streaming.

### Pros
- **Excellent Performance**: Only 5-10% slower than raw TCP, 60% faster than .NET Core 3.1
- **Fast Development**: Strongly-typed contracts auto-generate client/server code
- **Bidirectional Streaming**: Native support for real-time two-way communication
- **File Transfer**: Built-in chunked streaming for large files
- **Low Memory**: 92% reduction in per-request allocation in .NET 5+ (330 bytes per request)
- **Type Safety**: Compile-time validation of message contracts
- **Multiplexing**: Single HTTP/2 connection supports 100+ concurrent streams

### Cons
- Requires HTTP/2 (not an issue for .NET-to-.NET)
- Slightly more overhead than raw TCP
- Learning curve for Protocol Buffers

### Performance Metrics
- **Throughput**: Highest requests/second among .NET implementations
- **Memory**: ~330 bytes per request allocation
- **Concurrent Streams**: 100 per connection (configurable)
- **Latency**: Sub-millisecond for small messages

### Code Example

**Proto Definition** (`Protos/torservice.proto`):
```protobuf
syntax = "proto3";

option csharp_namespace = "TorGames.Common.Protos";

package torservice;

service TorService {
  // Bidirectional streaming for real-time communication
  rpc Connect(stream ClientMessage) returns (stream ServerMessage);

  // File transfer with streaming
  rpc UploadFile(stream FileChunk) returns (UploadResponse);
  rpc DownloadFile(FileRequest) returns (stream FileChunk);

  // Command execution
  rpc ExecuteCommand(CommandRequest) returns (CommandResponse);
}

message ClientMessage {
  string client_id = 1;
  oneof payload {
    Heartbeat heartbeat = 2;
    SystemInfo system_info = 3;
    CommandResult command_result = 4;
  }
}

message ServerMessage {
  oneof payload {
    Command command = 1;
    FileTransferRequest file_request = 2;
    ConfigUpdate config_update = 3;
  }
}

message Heartbeat {
  int64 timestamp = 1;
}

message SystemInfo {
  string machine_name = 1;
  string os_version = 2;
  int32 cpu_count = 3;
  int64 memory_bytes = 4;
}

message Command {
  string command_id = 1;
  string command_type = 2;
  bytes payload = 3;
}

message CommandResult {
  string command_id = 1;
  bool success = 2;
  bytes result = 3;
}

message FileChunk {
  string file_id = 1;
  int64 offset = 2;
  bytes data = 3;
  bool is_last = 4;
}

message FileRequest {
  string file_path = 1;
}

message UploadResponse {
  bool success = 1;
  string file_id = 2;
}

message CommandRequest {
  string command = 1;
  repeated string arguments = 2;
}

message CommandResponse {
  int32 exit_code = 1;
  string stdout = 2;
  string stderr = 3;
}

message FileTransferRequest {
  string file_path = 1;
  bool upload = 2;
}

message ConfigUpdate {
  map<string, string> settings = 1;
}
```

**Server Implementation**:
```csharp
public class TorServiceImpl : TorService.TorServiceBase
{
    private readonly ILogger<TorServiceImpl> _logger;
    private readonly ClientManager _clientManager;

    public TorServiceImpl(ILogger<TorServiceImpl> logger, ClientManager clientManager)
    {
        _logger = logger;
        _clientManager = clientManager;
    }

    public override async Task Connect(
        IAsyncStreamReader<ClientMessage> requestStream,
        IServerStreamWriter<ServerMessage> responseStream,
        ServerCallContext context)
    {
        var clientId = string.Empty;

        // Register client connection
        var client = new ConnectedClient(responseStream, context.CancellationToken);

        try
        {
            await foreach (var message in requestStream.ReadAllAsync(context.CancellationToken))
            {
                if (message.PayloadCase == ClientMessage.PayloadOneofCase.Heartbeat)
                {
                    clientId = message.ClientId;
                    _clientManager.UpdateHeartbeat(clientId);
                }
                else if (message.PayloadCase == ClientMessage.PayloadOneofCase.SystemInfo)
                {
                    clientId = message.ClientId;
                    _clientManager.RegisterClient(clientId, client, message.SystemInfo);
                }
                else if (message.PayloadCase == ClientMessage.PayloadOneofCase.CommandResult)
                {
                    _clientManager.HandleCommandResult(clientId, message.CommandResult);
                }
            }
        }
        finally
        {
            _clientManager.RemoveClient(clientId);
            _logger.LogInformation("Client {ClientId} disconnected", clientId);
        }
    }
}
```

**Client Implementation**:
```csharp
public class GrpcClientService : BackgroundService
{
    private readonly ILogger<GrpcClientService> _logger;
    private readonly string _serverAddress;

    public GrpcClientService(ILogger<GrpcClientService> logger, IConfiguration config)
    {
        _logger = logger;
        _serverAddress = config["ServerAddress"] ?? "https://localhost:5001";
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        using var channel = GrpcChannel.ForAddress(_serverAddress);
        var client = new TorService.TorServiceClient(channel);

        while (!stoppingToken.IsCancellationRequested)
        {
            try
            {
                using var call = client.Connect(cancellationToken: stoppingToken);

                // Send initial system info
                await call.RequestStream.WriteAsync(new ClientMessage
                {
                    ClientId = Environment.MachineName,
                    SystemInfo = new SystemInfo
                    {
                        MachineName = Environment.MachineName,
                        OsVersion = Environment.OSVersion.ToString(),
                        CpuCount = Environment.ProcessorCount,
                        MemoryBytes = GC.GetTotalMemory(false)
                    }
                });

                // Start receiving commands in background
                var receiveTask = ReceiveCommandsAsync(call.ResponseStream, stoppingToken);

                // Send heartbeats
                while (!stoppingToken.IsCancellationRequested)
                {
                    await call.RequestStream.WriteAsync(new ClientMessage
                    {
                        ClientId = Environment.MachineName,
                        Heartbeat = new Heartbeat { Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds() }
                    });
                    await Task.Delay(5000, stoppingToken);
                }
            }
            catch (RpcException ex) when (ex.StatusCode == StatusCode.Unavailable)
            {
                _logger.LogWarning("Server unavailable, retrying in 5 seconds...");
                await Task.Delay(5000, stoppingToken);
            }
        }
    }

    private async Task ReceiveCommandsAsync(
        IAsyncStreamReader<ServerMessage> responseStream,
        CancellationToken ct)
    {
        await foreach (var message in responseStream.ReadAllAsync(ct))
        {
            if (message.PayloadCase == ServerMessage.PayloadOneofCase.Command)
            {
                _logger.LogInformation("Received command: {Type}", message.Command.CommandType);
                // Handle command...
            }
        }
    }
}
```

**Kestrel Configuration for 10K+ Connections**:
```csharp
var builder = WebApplication.CreateBuilder(args);

builder.WebHost.ConfigureKestrel(options =>
{
    options.Limits.MaxConcurrentConnections = 50000;
    options.Limits.MaxConcurrentUpgradedConnections = 50000;
    options.Limits.Http2.MaxStreamsPerConnection = 250;
    options.Limits.Http2.InitialConnectionWindowSize = 1024 * 1024; // 1 MB
    options.Limits.Http2.InitialStreamWindowSize = 768 * 1024; // 768 KB

    options.ListenAnyIP(5001, listenOptions =>
    {
        listenOptions.Protocols = HttpProtocols.Http2;
        listenOptions.UseHttps();
    });
});

builder.Services.AddGrpc();
builder.Services.AddSingleton<ClientManager>();
```

---

## Option 2: Raw TCP with System.IO.Pipelines

### Overview
System.IO.Pipelines is the high-performance I/O library that powers Kestrel internally. It provides maximum control and performance for custom binary protocols.

### Pros
- **Maximum Performance**: Lowest possible latency and overhead
- **Full Control**: Custom binary protocol design
- **Zero HTTP Overhead**: Direct TCP communication
- **Minimal Allocations**: Designed for zero-allocation scenarios
- **Ultimate Scalability**: Powers Kestrel's millions of connections

### Cons
- **High Complexity**: Significant development time required
- **Manual Protocol Design**: Must implement framing, serialization, error handling
- **No Built-in Features**: Must implement reconnection, heartbeats, etc.
- **Debugging Difficulty**: Binary protocols harder to debug

### Performance Metrics
- **Throughput**: Maximum possible for .NET
- **Memory**: Near-zero allocation per message possible
- **Latency**: Microsecond-level
- **Connections**: Limited only by OS and hardware

### Code Example

**Server**:
```csharp
public class TcpServerService : BackgroundService
{
    private readonly ILogger<TcpServerService> _logger;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        var listener = new Socket(AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp);
        listener.Bind(new IPEndPoint(IPAddress.Any, 5000));
        listener.Listen(10000);

        _logger.LogInformation("TCP Server listening on port 5000");

        while (!stoppingToken.IsCancellationRequested)
        {
            var client = await listener.AcceptAsync(stoppingToken);
            _ = HandleClientAsync(client, stoppingToken);
        }
    }

    private async Task HandleClientAsync(Socket client, CancellationToken ct)
    {
        var pipe = new Pipe();
        var stream = new NetworkStream(client, ownsSocket: true);

        var writing = FillPipeAsync(stream, pipe.Writer, ct);
        var reading = ReadPipeAsync(pipe.Reader, ct);

        await Task.WhenAll(reading, writing);
    }

    private async Task FillPipeAsync(Stream stream, PipeWriter writer, CancellationToken ct)
    {
        const int minimumBufferSize = 512;

        while (!ct.IsCancellationRequested)
        {
            var memory = writer.GetMemory(minimumBufferSize);
            var bytesRead = await stream.ReadAsync(memory, ct);

            if (bytesRead == 0) break;

            writer.Advance(bytesRead);
            var result = await writer.FlushAsync(ct);

            if (result.IsCompleted) break;
        }

        await writer.CompleteAsync();
    }

    private async Task ReadPipeAsync(PipeReader reader, CancellationToken ct)
    {
        while (!ct.IsCancellationRequested)
        {
            var result = await reader.ReadAsync(ct);
            var buffer = result.Buffer;

            while (TryParseMessage(ref buffer, out var message))
            {
                await ProcessMessageAsync(message);
            }

            reader.AdvanceTo(buffer.Start, buffer.End);

            if (result.IsCompleted) break;
        }

        await reader.CompleteAsync();
    }

    private bool TryParseMessage(ref ReadOnlySequence<byte> buffer, out Message message)
    {
        // Custom protocol parsing logic
        // Example: 4-byte length prefix + payload
        message = default;

        if (buffer.Length < 4) return false;

        var lengthSlice = buffer.Slice(0, 4);
        var length = BitConverter.ToInt32(lengthSlice.ToArray());

        if (buffer.Length < 4 + length) return false;

        var payloadSlice = buffer.Slice(4, length);
        message = ParsePayload(payloadSlice);
        buffer = buffer.Slice(4 + length);

        return true;
    }
}
```

---

## Option 3: SignalR with MessagePack

### Overview
SignalR is Microsoft's real-time communication framework with automatic protocol negotiation and reconnection. MessagePack serialization provides binary performance.

### Pros
- **Easiest Implementation**: High-level API, minimal boilerplate
- **Automatic Reconnection**: Built-in connection management
- **MessagePack**: Binary serialization for good performance
- **Hub Pattern**: Intuitive RPC-style communication
- **Flexible Transport**: WebSockets, SSE, Long Polling fallback

### Cons
- **HTTP Overhead**: Even with WebSockets, SignalR adds protocol overhead
- **Less Control**: Abstracted wire protocol
- **Memory Per Connection**: Higher than gRPC or raw TCP
- **Scaling Complexity**: Requires Redis backplane for multi-server

### Performance Metrics
- **Throughput**: ~2x faster than JSON with MessagePack
- **Memory**: Higher per-connection overhead
- **Connections**: 10K+ achievable with tuning
- **Latency**: Good, but higher than gRPC/TCP

### Code Example

**Server Hub**:
```csharp
public class TorHub : Hub
{
    private readonly ClientManager _clientManager;

    public TorHub(ClientManager clientManager)
    {
        _clientManager = clientManager;
    }

    public override async Task OnConnectedAsync()
    {
        _clientManager.AddConnection(Context.ConnectionId);
        await base.OnConnectedAsync();
    }

    public override async Task OnDisconnectedAsync(Exception? exception)
    {
        _clientManager.RemoveConnection(Context.ConnectionId);
        await base.OnDisconnectedAsync(exception);
    }

    public async Task RegisterClient(SystemInfo info)
    {
        _clientManager.RegisterClient(Context.ConnectionId, info);
    }

    public async Task Heartbeat()
    {
        _clientManager.UpdateHeartbeat(Context.ConnectionId);
    }

    public async Task SendCommandResult(string commandId, bool success, byte[] result)
    {
        _clientManager.HandleCommandResult(Context.ConnectionId, commandId, success, result);
    }

    // Server-to-client methods called via Clients.Client(id).SendAsync("MethodName", ...)
}
```

**Server Configuration**:
```csharp
var builder = WebApplication.CreateBuilder(args);

builder.WebHost.ConfigureKestrel(options =>
{
    options.Limits.MaxConcurrentConnections = 50000;
    options.Limits.MaxConcurrentUpgradedConnections = 50000;
});

builder.Services.AddSignalR()
    .AddMessagePackProtocol();

var app = builder.Build();
app.MapHub<TorHub>("/torhub");
```

**Client**:
```csharp
public class SignalRClientService : BackgroundService
{
    private HubConnection _connection;

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        _connection = new HubConnectionBuilder()
            .WithUrl("https://localhost:5001/torhub")
            .AddMessagePackProtocol()
            .WithAutomaticReconnect()
            .Build();

        _connection.On<Command>("ExecuteCommand", async cmd =>
        {
            // Handle command from server
        });

        await _connection.StartAsync(stoppingToken);

        await _connection.InvokeAsync("RegisterClient", new SystemInfo
        {
            MachineName = Environment.MachineName,
            OsVersion = Environment.OSVersion.ToString()
        }, stoppingToken);

        while (!stoppingToken.IsCancellationRequested)
        {
            await _connection.InvokeAsync("Heartbeat", stoppingToken);
            await Task.Delay(5000, stoppingToken);
        }
    }
}
```

---

## Comparison Matrix

| Feature | gRPC Streaming | Raw TCP/Pipelines | SignalR/MessagePack |
|---------|---------------|-------------------|---------------------|
| **Performance** | Excellent | Maximum | Good |
| **Development Speed** | Fast | Slow | Fastest |
| **Complexity** | Medium | High | Low |
| **Type Safety** | Excellent (Proto) | Manual | Runtime |
| **Bidirectional** | Native | Manual | Native |
| **File Transfer** | Built-in Streaming | Manual | Manual |
| **Reconnection** | Manual | Manual | Automatic |
| **10K+ Connections** | Yes | Yes | Yes (tuned) |
| **Memory/Connection** | Low | Very Low | Medium |
| **Debug Ease** | Good | Hard | Easy |

---

## Recommendation: gRPC with Bidirectional Streaming

For TorGames, **gRPC with Bidirectional Streaming** is the recommended approach because:

1. **Best Balance**: Excellent performance (only 5-10% slower than raw TCP) with much faster development time

2. **Native Streaming**: Built-in support for bidirectional streaming perfectly matches your real-time communication needs

3. **File Transfers**: Chunked streaming is ideal for sending files of any size

4. **Type Safety**: Proto contracts catch errors at compile time and auto-generate client/server code

5. **Future-Proof**: Can easily add new message types and services without breaking existing clients

6. **Production Ready**: Used by Google, Netflix, and Microsoft at massive scale

7. **.NET Optimized**: .NET's gRPC implementation is one of the fastest available

### Next Steps

1. Create `TorGames.Server` project with gRPC services
2. Add `.proto` files to `TorGames.Common` for shared contracts
3. Configure Kestrel for 10K+ connections
4. Implement `ClientManager` for connection tracking
5. Update `TorGames.Client` and `TorGames.Installer` with gRPC clients

---

## Configuration Checklist for 10K+ Connections

### Application Level (All Options)
```csharp
// Kestrel limits
builder.WebHost.ConfigureKestrel(options =>
{
    options.Limits.MaxConcurrentConnections = 50000;
    options.Limits.MaxConcurrentUpgradedConnections = 50000;
});

// Thread pool tuning
ThreadPool.SetMinThreads(
    Environment.ProcessorCount * 4,
    Environment.ProcessorCount * 4
);
```

### OS Level (Windows)
```powershell
# Increase max connections (run as admin)
netsh int ipv4 set dynamicport tcp start=1025 num=64510

# Increase max user port
Set-ItemProperty -Path "HKLM:\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters" -Name "MaxUserPort" -Value 65534
```

### OS Level (Linux)
```bash
# /etc/sysctl.conf
fs.file-max = 1000000
net.core.somaxconn = 65535
net.ipv4.tcp_max_syn_backlog = 65535
net.ipv4.ip_local_port_range = 1024 65535

# /etc/security/limits.conf
* soft nofile 1000000
* hard nofile 1000000
```
