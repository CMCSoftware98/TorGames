import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { signalRService } from '@/services/signalr'
import { useAuthStore } from './auth'
import { useSettingsStore } from './settings'
import type { ClientDto, CommandResultDto } from '@/types/client'

export const useClientsStore = defineStore('clients', () => {
  // State
  const clients = ref<Map<string, ClientDto>>(new Map())
  const isConnected = ref(false)
  const isConnecting = ref(false)
  const isReconnecting = ref(false)
  const lastError = ref<string | null>(null)
  const commandResults = ref<CommandResultDto[]>([])

  // Computed
  const clientList = computed(() => Array.from(clients.value.values()))
  const onlineClients = computed(() => clientList.value.filter(c => c.isOnline))
  const clientCount = computed(() => clients.value.size)
  const onlineCount = computed(() => onlineClients.value.length)

  // Actions
  async function connect(): Promise<void> {
    const authStore = useAuthStore()

    if (!authStore.sessionToken) {
      lastError.value = 'Not authenticated'
      return
    }

    if (isConnected.value || isConnecting.value) {
      return
    }

    isConnecting.value = true
    lastError.value = null

    try {
      signalRService.setToken(authStore.sessionToken)

      // Subscribe to events before connecting
      signalRService.onClientConnected(handleClientConnected)
      signalRService.onClientDisconnected(handleClientDisconnected)
      signalRService.onClientHeartbeat(handleClientHeartbeat)
      signalRService.onCommandResult(handleCommandResult)
      signalRService.onConnectionStateChanged(handleConnectionStateChanged)
      signalRService.onReconnectingStateChanged(handleReconnectingStateChanged)

      await signalRService.connect()

      // Fetch initial client list
      const initialClients = await signalRService.getAllClients()
      clients.value.clear()
      initialClients.forEach(client => {
        clients.value.set(client.connectionKey, client)
      })

      isConnected.value = true
    } catch (error) {
      lastError.value = error instanceof Error ? error.message : 'Failed to connect'
      console.error('Failed to connect to SignalR:', error)
    } finally {
      isConnecting.value = false
    }
  }

  async function disconnect(): Promise<void> {
    await signalRService.disconnect()
    clients.value.clear()
    isConnected.value = false
  }

  function handleClientConnected(client: ClientDto) {
    clients.value.set(client.connectionKey, client)

    // Send desktop notification
    const settingsStore = useSettingsStore()
    settingsStore.showNotification(
      'New Client Connected',
      `${client.machineName} (${client.username}) - ${client.ipAddress}`
    )
  }

  function handleClientDisconnected(client: ClientDto) {
    clients.value.delete(client.connectionKey)
  }

  function handleClientHeartbeat(client: ClientDto) {
    // Update existing client data
    if (clients.value.has(client.connectionKey)) {
      clients.value.set(client.connectionKey, client)
    }
  }

  function handleCommandResult(result: CommandResultDto) {
    commandResults.value.push(result)
    // Keep only last 100 results
    if (commandResults.value.length > 100) {
      commandResults.value = commandResults.value.slice(-100)
    }
  }

  function handleConnectionStateChanged(connected: boolean) {
    isConnected.value = connected
    if (connected) {
      // Successfully connected, fetch clients
      signalRService.getAllClients().then(clientList => {
        clients.value.clear()
        clientList.forEach(client => {
          clients.value.set(client.connectionKey, client)
        })
      }).catch(err => {
        console.error('Failed to fetch clients after reconnect:', err)
      })
    } else {
      // Connection lost, clear clients
      clients.value.clear()
    }
  }

  function handleReconnectingStateChanged(reconnecting: boolean) {
    isReconnecting.value = reconnecting
  }

  async function executeCommand(
    connectionKey: string,
    commandType: string,
    commandText: string,
    timeoutSeconds: number = 30
  ): Promise<boolean> {
    if (!isConnected.value) {
      throw new Error('Not connected')
    }

    return signalRService.executeCommand({
      connectionKey,
      commandType,
      commandText,
      timeoutSeconds
    })
  }

  function getClient(connectionKey: string): ClientDto | undefined {
    return clients.value.get(connectionKey)
  }

  function clearCommandResults() {
    commandResults.value = []
  }

  return {
    // State
    clients,
    isConnected,
    isConnecting,
    isReconnecting,
    lastError,
    commandResults,
    // Computed
    clientList,
    onlineClients,
    clientCount,
    onlineCount,
    // Actions
    connect,
    disconnect,
    executeCommand,
    getClient,
    clearCommandResults
  }
})
