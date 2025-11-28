import * as signalR from '@microsoft/signalr'
import type { ClientDto, CommandResultDto, ExecuteCommandRequest, ConnectionStats, DetailedSystemInfoDto } from '@/types/client'

const HUB_URL = 'http://144.91.111.101:5001/hubs/clients'
const RECONNECT_INTERVAL_MS = 10000 // 10 seconds

export type ClientEventHandler = (client: ClientDto) => void
export type CommandResultHandler = (result: CommandResultDto) => void
export type ConnectionStateHandler = (connected: boolean) => void
export type ReconnectingStateHandler = (isReconnecting: boolean) => void

class SignalRService {
  private connection: signalR.HubConnection | null = null
  private token: string | null = null
  private reconnectTimer: ReturnType<typeof setInterval> | null = null
  private manuallyDisconnected: boolean = false

  // Event handlers
  private clientConnectedHandlers: ClientEventHandler[] = []
  private clientDisconnectedHandlers: ClientEventHandler[] = []
  private clientHeartbeatHandlers: ClientEventHandler[] = []
  private commandResultHandlers: CommandResultHandler[] = []
  private connectionStateHandlers: ConnectionStateHandler[] = []
  private reconnectingStateHandlers: ReconnectingStateHandler[] = []

  /**
   * Sets the JWT token for authentication
   */
  setToken(token: string) {
    this.token = token
  }

  /**
   * Connects to the SignalR hub
   */
  async connect(): Promise<void> {
    if (!this.token) {
      throw new Error('Token not set. Call setToken() first.')
    }

    if (this.connection?.state === signalR.HubConnectionState.Connected) {
      console.log('Already connected to SignalR hub')
      return
    }

    // Clear any existing reconnect timer
    this.stopReconnectTimer()
    this.manuallyDisconnected = false

    this.connection = new signalR.HubConnectionBuilder()
      .withUrl(HUB_URL, {
        accessTokenFactory: () => this.token!
      })
      .withAutomaticReconnect({
        nextRetryDelayInMilliseconds: (retryContext) => {
          // Exponential backoff: 0s, 2s, 4s, 8s, 16s, max 30s
          const delay = Math.min(Math.pow(2, retryContext.previousRetryCount) * 1000, 30000)
          console.log(`SignalR reconnecting in ${delay}ms...`)
          return delay
        }
      })
      .configureLogging(signalR.LogLevel.Information)
      .build()

    // Register event handlers
    this.connection.on('ClientConnected', (client: ClientDto) => {
      console.log('Client connected:', client.connectionKey)
      this.clientConnectedHandlers.forEach(handler => handler(client))
    })

    this.connection.on('ClientDisconnected', (client: ClientDto) => {
      console.log('Client disconnected:', client.connectionKey)
      this.clientDisconnectedHandlers.forEach(handler => handler(client))
    })

    this.connection.on('ClientHeartbeat', (client: ClientDto) => {
      this.clientHeartbeatHandlers.forEach(handler => handler(client))
    })

    this.connection.on('CommandResult', (result: CommandResultDto) => {
      console.log('Command result:', result.commandId, result.success)
      this.commandResultHandlers.forEach(handler => handler(result))
    })

    this.connection.onreconnecting(() => {
      console.log('SignalR reconnecting...')
      this.connectionStateHandlers.forEach(handler => handler(false))
      this.reconnectingStateHandlers.forEach(handler => handler(true))
    })

    this.connection.onreconnected(() => {
      console.log('SignalR reconnected')
      this.connectionStateHandlers.forEach(handler => handler(true))
      this.reconnectingStateHandlers.forEach(handler => handler(false))
    })

    this.connection.onclose((error) => {
      console.log('SignalR connection closed', error ? `with error: ${error}` : '')
      this.connectionStateHandlers.forEach(handler => handler(false))
      this.reconnectingStateHandlers.forEach(handler => handler(false))

      // Start manual reconnection if not manually disconnected
      if (!this.manuallyDisconnected) {
        this.startReconnectTimer()
      }
    })

    try {
      await this.connection.start()
      console.log('Connected to SignalR hub')
      this.connectionStateHandlers.forEach(handler => handler(true))
      this.reconnectingStateHandlers.forEach(handler => handler(false))
    } catch (error) {
      console.error('Failed to connect to SignalR hub:', error)
      // Start reconnect timer on initial connection failure
      if (!this.manuallyDisconnected) {
        this.startReconnectTimer()
      }
      throw error
    }
  }

  /**
   * Disconnects from the SignalR hub
   */
  async disconnect(): Promise<void> {
    this.manuallyDisconnected = true
    this.stopReconnectTimer()

    if (this.connection) {
      await this.connection.stop()
      this.connection = null
    }
  }

  /**
   * Starts the reconnection timer (every 10 seconds)
   */
  private startReconnectTimer(): void {
    if (this.reconnectTimer) return // Already running

    console.log(`Starting reconnection timer (every ${RECONNECT_INTERVAL_MS / 1000}s)...`)
    this.reconnectingStateHandlers.forEach(handler => handler(true))

    this.reconnectTimer = setInterval(async () => {
      if (this.manuallyDisconnected) {
        this.stopReconnectTimer()
        return
      }

      if (this.connection?.state === signalR.HubConnectionState.Connected) {
        console.log('Already connected, stopping reconnect timer')
        this.stopReconnectTimer()
        return
      }

      console.log('Attempting to reconnect to SignalR hub...')

      try {
        // Need to rebuild the connection if it was closed
        if (!this.connection || this.connection.state === signalR.HubConnectionState.Disconnected) {
          await this.connect()
        }

        // Successfully connected, timer will be stopped in connect()
        console.log('Reconnection successful!')
      } catch (error) {
        console.log('Reconnection attempt failed, will retry in', RECONNECT_INTERVAL_MS / 1000, 'seconds')
        // Timer continues running, will try again
      }
    }, RECONNECT_INTERVAL_MS)
  }

  /**
   * Stops the reconnection timer
   */
  private stopReconnectTimer(): void {
    if (this.reconnectTimer) {
      console.log('Stopping reconnection timer')
      clearInterval(this.reconnectTimer)
      this.reconnectTimer = null
      this.reconnectingStateHandlers.forEach(handler => handler(false))
    }
  }

  /**
   * Gets the current connection state
   */
  get isConnected(): boolean {
    return this.connection?.state === signalR.HubConnectionState.Connected
  }

  /**
   * Gets whether the service is attempting to reconnect
   */
  get isReconnecting(): boolean {
    return this.reconnectTimer !== null ||
           this.connection?.state === signalR.HubConnectionState.Reconnecting
  }

  // Event subscriptions
  onClientConnected(handler: ClientEventHandler): () => void {
    this.clientConnectedHandlers.push(handler)
    return () => {
      const index = this.clientConnectedHandlers.indexOf(handler)
      if (index > -1) this.clientConnectedHandlers.splice(index, 1)
    }
  }

  onClientDisconnected(handler: ClientEventHandler): () => void {
    this.clientDisconnectedHandlers.push(handler)
    return () => {
      const index = this.clientDisconnectedHandlers.indexOf(handler)
      if (index > -1) this.clientDisconnectedHandlers.splice(index, 1)
    }
  }

  onClientHeartbeat(handler: ClientEventHandler): () => void {
    this.clientHeartbeatHandlers.push(handler)
    return () => {
      const index = this.clientHeartbeatHandlers.indexOf(handler)
      if (index > -1) this.clientHeartbeatHandlers.splice(index, 1)
    }
  }

  onCommandResult(handler: CommandResultHandler): () => void {
    this.commandResultHandlers.push(handler)
    return () => {
      const index = this.commandResultHandlers.indexOf(handler)
      if (index > -1) this.commandResultHandlers.splice(index, 1)
    }
  }

  onConnectionStateChanged(handler: ConnectionStateHandler): () => void {
    this.connectionStateHandlers.push(handler)
    return () => {
      const index = this.connectionStateHandlers.indexOf(handler)
      if (index > -1) this.connectionStateHandlers.splice(index, 1)
    }
  }

  onReconnectingStateChanged(handler: ReconnectingStateHandler): () => void {
    this.reconnectingStateHandlers.push(handler)
    return () => {
      const index = this.reconnectingStateHandlers.indexOf(handler)
      if (index > -1) this.reconnectingStateHandlers.splice(index, 1)
    }
  }

  // Hub methods
  async getAllClients(): Promise<ClientDto[]> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<ClientDto[]>('GetAllClients')
  }

  async getClient(connectionKey: string): Promise<ClientDto | null> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<ClientDto | null>('GetClient', connectionKey)
  }

  async getOnlineClients(): Promise<ClientDto[]> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<ClientDto[]>('GetOnlineClients')
  }

  async executeCommand(request: ExecuteCommandRequest): Promise<boolean> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<boolean>('ExecuteCommand', request)
  }

  /**
   * Executes a command and waits for the result
   */
  async executeCommandWithResult(request: ExecuteCommandRequest): Promise<CommandResultDto | null> {
    if (!this.connection) throw new Error('Not connected')

    return new Promise((resolve) => {
      const commandId = crypto.randomUUID()
      const timeout = (request.timeoutSeconds || 30) * 1000

      // Create timeout
      const timeoutId = setTimeout(() => {
        cleanup()
        resolve({
          commandId,
          connectionKey: request.connectionKey,
          success: false,
          exitCode: -1,
          stdout: '',
          stderr: '',
          errorMessage: 'Command timed out'
        })
      }, timeout)

      // Handler to listen for result
      const resultHandler = (result: CommandResultDto) => {
        if (result.connectionKey === request.connectionKey) {
          cleanup()
          resolve(result)
        }
      }

      const cleanup = () => {
        clearTimeout(timeoutId)
        const index = this.commandResultHandlers.indexOf(resultHandler)
        if (index > -1) {
          this.commandResultHandlers.splice(index, 1)
        }
      }

      // Add handler
      this.commandResultHandlers.push(resultHandler)

      // Send command
      this.connection!.invoke<boolean>('ExecuteCommand', request).catch((err) => {
        cleanup()
        resolve({
          commandId,
          connectionKey: request.connectionKey,
          success: false,
          exitCode: -1,
          stdout: '',
          stderr: '',
          errorMessage: err.message || 'Failed to execute command'
        })
      })
    })
  }

  async broadcastCommand(clientType: string, request: ExecuteCommandRequest): Promise<number> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<number>('BroadcastCommand', clientType, request)
  }

  async getStats(): Promise<ConnectionStats> {
    if (!this.connection) throw new Error('Not connected')
    return await this.connection.invoke<ConnectionStats>('GetStats')
  }

  async requestSystemInfo(connectionKey: string): Promise<{ data: DetailedSystemInfoDto | null; responseTimeMs: number }> {
    if (!this.connection) throw new Error('Not connected')
    const startTime = performance.now()
    const data = await this.connection.invoke<DetailedSystemInfoDto | null>('RequestSystemInfo', connectionKey)
    const responseTimeMs = Math.round(performance.now() - startTime)
    return { data, responseTimeMs }
  }
}

// Export singleton instance
export const signalRService = new SignalRService()
