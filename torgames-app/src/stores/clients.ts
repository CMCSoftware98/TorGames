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
  const lastRefreshTime = ref<Date>(new Date())

  // Periodic refresh timer
  let refreshInterval: ReturnType<typeof setInterval> | null = null
  const REFRESH_INTERVAL_MS = 30000 // Refresh clients every 30 seconds

  // Helper to check if client is an installer
  // Uses server-provided isInstalling flag which covers:
  // - ClientType == "INSTALLER"
  // - ActivityStatus contains "Installing" or "Updating"
  const isInstallerClient = (client: ClientDto): boolean => {
    // Prefer server-provided flag if available
    if (client.isInstalling !== undefined) return client.isInstalling
    // Fallback for older server versions
    if (client.clientType?.toUpperCase() === 'INSTALLER') return true
    const activity = (client.activityStatus || '').toLowerCase()
    if (activity.includes('installing') || activity.includes('updating') || activity.includes('downloading')) return true
    return false
  }

  // Computed
  const clientList = computed(() => Array.from(clients.value.values()))
  const onlineClients = computed(() => clientList.value.filter(c => c.isOnline && !isInstallerClient(c)))
  const installingClients = computed(() => clientList.value.filter(c => c.isOnline && isInstallerClient(c)))
  const offlineClients = computed(() => clientList.value.filter(c => !c.isOnline && !isInstallerClient(c)))
  const clientCount = computed(() => clients.value.size)
  const onlineCount = computed(() => onlineClients.value.length)
  const installingCount = computed(() => installingClients.value.length)

  // Internal functions
  async function doRefresh() {
    if (!isConnected.value) return
    try {
      const freshClients = await signalRService.getAllClients()
      clients.value.clear()
      freshClients.forEach(client => {
        clients.value.set(client.connectionKey, client)
      })
      lastRefreshTime.value = new Date()
    } catch (err) {
      console.error('Failed to refresh clients:', err)
    }
  }

  function startPeriodicRefresh() {
    if (refreshInterval) return
    refreshInterval = setInterval(doRefresh, REFRESH_INTERVAL_MS)
  }

  function stopPeriodicRefresh() {
    if (refreshInterval) {
      clearInterval(refreshInterval)
      refreshInterval = null
    }
  }

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
      lastRefreshTime.value = new Date()

      isConnected.value = true

      // Start periodic refresh to ensure we don't miss updates
      startPeriodicRefresh()
    } catch (error) {
      lastError.value = error instanceof Error ? error.message : 'Failed to connect'
      console.error('Failed to connect to SignalR:', error)
    } finally {
      isConnecting.value = false
    }
  }

  async function disconnect(): Promise<void> {
    stopPeriodicRefresh()
    await signalRService.disconnect()
    // Mark all clients as offline but keep them in the list (persistent)
    clients.value.forEach((client, key) => {
      clients.value.set(key, { ...client, isOnline: false })
    })
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
    // Update client to show offline status instead of removing
    // The server sends the full client data with isOnline = false
    clients.value.set(client.connectionKey, client)
  }

  function handleClientHeartbeat(client: ClientDto) {
    // Update client data (always set, not just if exists)
    clients.value.set(client.connectionKey, client)
    // Update last refresh time on heartbeat as well
    lastRefreshTime.value = new Date()
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
      // Restart periodic refresh
      startPeriodicRefresh()
    } else {
      // Connection lost - stop periodic refresh
      stopPeriodicRefresh()
      // Mark all clients as offline but keep them in list
      clients.value.forEach((client, key) => {
        clients.value.set(key, { ...client, isOnline: false })
      })
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

  function removeClient(clientId: string) {
    // Find and remove client by clientId (not connectionKey)
    for (const [key, client] of clients.value.entries()) {
      if (client.clientId === clientId) {
        clients.value.delete(key)
        break
      }
    }
  }

  // Public refresh function
  async function refresh(): Promise<void> {
    await doRefresh()
  }

  return {
    // State
    clients,
    isConnected,
    isConnecting,
    isReconnecting,
    lastError,
    commandResults,
    lastRefreshTime,
    // Computed
    clientList,
    onlineClients,
    installingClients,
    offlineClients,
    clientCount,
    onlineCount,
    installingCount,
    // Actions
    connect,
    disconnect,
    executeCommand,
    getClient,
    clearCommandResults,
    removeClient,
    refresh
  }
})
