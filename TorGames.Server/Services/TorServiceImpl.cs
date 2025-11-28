using Grpc.Core;
using TorGames.Common.Protos;
using TorGames.Server.Models;

namespace TorGames.Server.Services;

/// <summary>
/// gRPC service implementation for handling client connections and communication.
/// </summary>
public class TorServiceImpl : TorService.TorServiceBase
{
    private readonly ILogger<TorServiceImpl> _logger;
    private readonly ClientManager _clientManager;

    public TorServiceImpl(ILogger<TorServiceImpl> logger, ClientManager clientManager)
    {
        _logger = logger;
        _clientManager = clientManager;
    }

    /// <summary>
    /// Handles bidirectional streaming connection from clients.
    /// </summary>
    public override async Task Connect(
        IAsyncStreamReader<ClientMessage> requestStream,
        IServerStreamWriter<ServerMessage> responseStream,
        ServerCallContext context)
    {
        ConnectedClient? client = null;
        var connectionKey = string.Empty;

        try
        {
            // Wait for first message (should be registration)
            if (!await requestStream.MoveNext(context.CancellationToken))
            {
                _logger.LogWarning("Client disconnected before sending registration");
                return;
            }

            var firstMessage = requestStream.Current;

            if (firstMessage.PayloadCase != ClientMessage.PayloadOneofCase.Registration)
            {
                _logger.LogWarning("First message was not registration from client {ClientId}", firstMessage.ClientId);
                await SendConnectionResponse(responseStream, false, "First message must be registration", context.CancellationToken);
                return;
            }

            // Create client object
            client = new ConnectedClient
            {
                ClientId = firstMessage.ClientId,
                ClientType = firstMessage.ClientType.ToUpperInvariant(),
                ResponseStream = responseStream,
                CancellationToken = context.CancellationToken
            };
            client.UpdateFromRegistration(firstMessage.Registration);
            connectionKey = client.ConnectionKey;

            // Try to register
            var (success, rejectionReason) = _clientManager.TryRegisterClient(client);

            if (!success)
            {
                await SendConnectionResponse(responseStream, false, rejectionReason!, context.CancellationToken);
                return;
            }

            // Send acceptance
            await SendConnectionResponse(responseStream, true, null, context.CancellationToken, client.ClientId);

            _logger.LogInformation(
                "Client connected: {ConnectionKey} from {MachineName} ({IpAddress})",
                connectionKey,
                client.MachineName,
                client.IpAddress);

            // Process incoming messages
            while (await requestStream.MoveNext(context.CancellationToken))
            {
                var message = requestStream.Current;
                await ProcessClientMessage(client, message);
            }
        }
        catch (OperationCanceledException)
        {
            _logger.LogDebug("Client connection cancelled: {ConnectionKey}", connectionKey);
        }
        catch (IOException ex) when (IsConnectionAbortException(ex))
        {
            // Client disconnected abruptly (network issue, crash, etc.) - this is normal
            _logger.LogDebug("Client connection aborted: {ConnectionKey}", connectionKey);
        }
        catch (RpcException ex) when (ex.StatusCode == StatusCode.Cancelled)
        {
            _logger.LogDebug("Client RPC cancelled: {ConnectionKey}", connectionKey);
        }
        catch (Exception ex)
        {
            // Only log unexpected errors
            _logger.LogWarning(ex, "Unexpected error handling client connection: {ConnectionKey}", connectionKey);
        }
        finally
        {
            if (!string.IsNullOrEmpty(connectionKey))
            {
                _clientManager.RemoveClient(connectionKey);
                _logger.LogInformation("Client disconnected: {ConnectionKey}", connectionKey);
            }
        }
    }

    private async Task ProcessClientMessage(ConnectedClient client, ClientMessage message)
    {
        switch (message.PayloadCase)
        {
            case ClientMessage.PayloadOneofCase.Heartbeat:
                client.UpdateFromHeartbeat(message.Heartbeat);
                _logger.LogDebug("Heartbeat from {ConnectionKey}", client.ConnectionKey);
                break;

            case ClientMessage.PayloadOneofCase.CommandResult:
                await HandleCommandResult(client, message.CommandResult);
                break;

            case ClientMessage.PayloadOneofCase.Metrics:
                HandleMetrics(client, message.Metrics);
                break;

            case ClientMessage.PayloadOneofCase.Registration:
                // Update registration info (reconnection scenario)
                client.UpdateFromRegistration(message.Registration);
                _logger.LogInformation("Client re-registered: {ConnectionKey}", client.ConnectionKey);
                break;

            case ClientMessage.PayloadOneofCase.DetailedSystemInfo:
                HandleDetailedSystemInfo(client, message.DetailedSystemInfo);
                break;

            default:
                _logger.LogWarning("Unknown message type from {ConnectionKey}: {PayloadCase}",
                    client.ConnectionKey, message.PayloadCase);
                break;
        }
    }

    private void HandleDetailedSystemInfo(ConnectedClient client, DetailedSystemInfo info)
    {
        _logger.LogInformation(
            "Detailed system info received from {ConnectionKey} (RequestId={RequestId}): CPU={CpuName}, RAM={TotalRam}GB",
            client.ConnectionKey,
            info.RequestId,
            info.Cpu?.Name ?? "Unknown",
            info.Memory?.TotalPhysicalBytes / 1024 / 1024 / 1024 ?? 0);

        // Complete the pending request using the request ID from the response
        client.OnDetailedSystemInfoReceived(info.RequestId, info);

        // Also notify SignalR clients
        _clientManager.NotifyDetailedSystemInfo(client.ConnectionKey, info);
    }

    private Task HandleCommandResult(ConnectedClient client, CommandResult result)
    {
        _logger.LogInformation(
            "Command result from {ConnectionKey}: CommandId={CommandId}, Success={Success}, ExitCode={ExitCode}",
            client.ConnectionKey,
            result.CommandId,
            result.Success,
            result.ExitCode);

        // Notify SignalR clients about the command result
        _clientManager.NotifyCommandResult(client.ConnectionKey, result);

        return Task.CompletedTask;
    }

    private void HandleMetrics(ConnectedClient client, SystemMetrics metrics)
    {
        client.CpuUsagePercent = metrics.CpuUsagePercent;
        client.AvailableMemoryBytes = metrics.AvailableMemoryBytes;

        _logger.LogDebug(
            "Metrics from {ConnectionKey}: CPU={Cpu}%, Memory={Memory}MB free",
            client.ConnectionKey,
            metrics.CpuUsagePercent,
            metrics.AvailableMemoryBytes / 1024 / 1024);
    }

    private static async Task SendConnectionResponse(
        IServerStreamWriter<ServerMessage> stream,
        bool accepted,
        string? rejectionReason,
        CancellationToken ct,
        string? assignedId = null)
    {
        var response = new ServerMessage
        {
            MessageId = Guid.NewGuid().ToString(),
            Timestamp = DateTimeOffset.UtcNow.ToUnixTimeMilliseconds(),
            ConnectionResponse = new ConnectionResponse
            {
                Accepted = accepted,
                RejectionReason = rejectionReason ?? string.Empty,
                AssignedId = assignedId ?? string.Empty
            }
        };

        await stream.WriteAsync(response, ct);
    }

    /// <summary>
    /// Handles file uploads from clients.
    /// </summary>
    public override async Task<FileResponse> UploadFile(
        IAsyncStreamReader<FileChunk> requestStream,
        ServerCallContext context)
    {
        var transferId = string.Empty;
        var fileName = string.Empty;
        long bytesReceived = 0;

        try
        {
            // Create temp file for upload
            var tempPath = Path.GetTempFileName();

            await using (var fileStream = File.Create(tempPath))
            {
                await foreach (var chunk in requestStream.ReadAllAsync(context.CancellationToken))
                {
                    if (string.IsNullOrEmpty(transferId))
                    {
                        transferId = chunk.TransferId;
                        fileName = chunk.FileName;
                        _logger.LogInformation("Starting file upload: {TransferId} - {FileName}", transferId, fileName);
                    }

                    await fileStream.WriteAsync(chunk.Data.Memory, context.CancellationToken);
                    bytesReceived += chunk.Data.Length;

                    if (chunk.IsLast)
                    {
                        _logger.LogInformation(
                            "File upload complete: {TransferId} - {BytesReceived} bytes",
                            transferId,
                            bytesReceived);
                    }
                }
            }

            // TODO: Move temp file to final location, process, etc.

            return new FileResponse
            {
                TransferId = transferId,
                Success = true,
                BytesReceived = bytesReceived
            };
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "File upload failed: {TransferId}", transferId);
            return new FileResponse
            {
                TransferId = transferId,
                Success = false,
                ErrorMessage = ex.Message,
                BytesReceived = bytesReceived
            };
        }
    }

    /// <summary>
    /// Handles file download requests from clients.
    /// </summary>
    public override async Task DownloadFile(
        FileRequest request,
        IServerStreamWriter<FileChunk> responseStream,
        ServerCallContext context)
    {
        const int chunkSize = 64 * 1024; // 64KB chunks

        _logger.LogInformation("Starting file download: {TransferId} - {FilePath}",
            request.TransferId, request.FilePath);

        try
        {
            if (!File.Exists(request.FilePath))
            {
                _logger.LogWarning("File not found for download: {FilePath}", request.FilePath);
                return;
            }

            var fileInfo = new FileInfo(request.FilePath);
            var totalSize = fileInfo.Length;
            long offset = 0;

            await using var fileStream = File.OpenRead(request.FilePath);
            var buffer = new byte[chunkSize];

            int bytesRead;
            while ((bytesRead = await fileStream.ReadAsync(buffer, context.CancellationToken)) > 0)
            {
                var isLast = offset + bytesRead >= totalSize;

                var chunk = new FileChunk
                {
                    TransferId = request.TransferId,
                    FileName = fileInfo.Name,
                    TotalSize = totalSize,
                    Offset = offset,
                    Data = Google.Protobuf.ByteString.CopyFrom(buffer, 0, bytesRead),
                    IsLast = isLast
                };

                await responseStream.WriteAsync(chunk, context.CancellationToken);
                offset += bytesRead;
            }

            _logger.LogInformation("File download complete: {TransferId} - {BytesSent} bytes",
                request.TransferId, offset);
        }
        catch (Exception ex)
        {
            _logger.LogError(ex, "File download failed: {TransferId}", request.TransferId);
            throw;
        }
    }
}
