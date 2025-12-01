<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, watch } from 'vue'

import { useAuthStore } from '@/stores/auth'
import { useClientsStore } from '@/stores/clients'
import { useSettingsStore } from '@/stores/settings'
import { signalRService } from '@/services/signalr'
import { getVersions, deleteDatabaseClient, bulkDeleteDatabaseClients } from '@/services/api'
import MainLayout from '@/layouts/MainLayout.vue'
import ClientManagerDialog from '@/components/ClientManagerDialog.vue'
import FileExplorerDialog from '@/components/FileExplorerDialog.vue'
import VersionManagement from '@/components/VersionManagement.vue'
import SavedDataPanel from '@/components/SavedDataPanel.vue'
import type { ClientDto } from '@/types/client'
import type { VersionInfo } from '@/types/version'

const authStore = useAuthStore()
const clientsStore = useClientsStore()
const settingsStore = useSettingsStore()

const activeTab = ref('clients')

// Machine list sub-tab state
const machineListTab = ref<'online' | 'offline'>('online')

// Context menu state
const contextMenu = ref(false)
const contextMenuX = ref(0)
const contextMenuY = ref(0)
const contextMenuClient = ref<ClientDto | null>(null)

// Client Manager dialog state
const clientManagerDialog = ref(false)
const clientManagerClient = ref<ClientDto | null>(null)

// File Explorer dialog state
const fileExplorerDialog = ref(false)
const fileExplorerClient = ref<ClientDto | null>(null)

// Selection state - use IDs instead of objects to avoid stale references
const selectedClientIds = ref<string[]>([])

// Search state
const search = ref('')

// Connection status
const connectionError = ref<string | null>(null)

// Auto-refresh state
const lastUpdateTime = ref<Date>(new Date())
const autoRefreshInterval = ref<ReturnType<typeof setInterval> | null>(null)

// Tick counter to force reactivity for lastUpdateText
const updateTick = ref(0)
let updateTickInterval: ReturnType<typeof setInterval> | null = null

// Format relative time since last update
const lastUpdateText = computed(() => {
  // Reference updateTick to ensure reactivity
  void updateTick.value

  const now = new Date()
  const diff = Math.floor((now.getTime() - lastUpdateTime.value.getTime()) / 1000)

  if (diff < 1) return 'Just now'
  if (diff === 1) return '1 second ago'
  if (diff < 60) return `${diff} seconds ago`
  if (diff < 120) return '1 minute ago'
  if (diff < 3600) return `${Math.floor(diff / 60)} minutes ago`
  return `${Math.floor(diff / 3600)} hours ago`
})

// Latest version from server
const latestVersion = ref<VersionInfo | null>(null)

// Snackbar state
const snackbar = ref({
  show: false,
  message: '',
  color: 'success'
})

// Uninstall dialog state
const uninstallDialog = ref(false)
const uninstallTarget = ref<ClientDto | null>(null)
const uninstallAndDelete = ref(false)

// Delete dialog state
const deleteDialog = ref(false)
const deleteTarget = ref<DisplayClient | null>(null)
const deleteLoading = ref(false)

// Offline context menu state
const offlineContextMenu = ref(false)
const offlineContextMenuX = ref(0)
const offlineContextMenuY = ref(0)
const offlineContextMenuClient = ref<DisplayClient | null>(null)

// Transform ClientDto to display format
interface DisplayClient {
  id: string
  connectionKey: string
  clientId: string
  version: string
  isOutdated: boolean
  ipAddress: string
  machineName: string
  operatingSystem: string
  isAdmin: boolean
  cpus: number
  type: string
  firstSeen: string
  lastSeen: string
  status: 'online' | 'offline'
  countryCode: string
  activityStatus: string
  raw: ClientDto
}

// Compare version strings (format: YYYY.MM.DD.BUILD)
function isVersionOutdated(clientVersion: string, latestVer: VersionInfo | null): boolean {
  if (!latestVer) return false
  if (!clientVersion || clientVersion === '0.0.0' || clientVersion === '1.0.0') return true

  const parseVersion = (v: string) => v.split('.').map(p => parseInt(p, 10) || 0)
  const client = parseVersion(clientVersion)
  const latest = parseVersion(latestVer.version)

  for (let i = 0; i < 4; i++) {
    if ((client[i] || 0) < (latest[i] || 0)) return true
    if ((client[i] || 0) > (latest[i] || 0)) return false
  }
  return false
}

const transformClient = (client: ClientDto): DisplayClient => {
  const connectedDate = new Date(client.connectedAt)
  const lastHeartbeat = new Date(client.lastHeartbeat)
  const now = new Date()

  const formatDuration = (ms: number): string => {
    const seconds = Math.floor(ms / 1000)
    const minutes = Math.floor(seconds / 60)
    const hours = Math.floor(minutes / 60)
    const days = Math.floor(hours / 24)

    if (days > 0) return `${days}D ${hours % 24}H`
    if (hours > 0) return `${hours}H ${minutes % 60}M`
    if (minutes > 0) return `${minutes}M`
    return `${seconds}s`
  }

  const timeSinceHeartbeat = now.getTime() - lastHeartbeat.getTime()
  const lastSeenText = client.isOnline
    ? 'Online Now'
    : `${formatDuration(timeSinceHeartbeat)} ago`

  const clientVer = client.clientVersion || '0.0.0'

  return {
    id: client.connectionKey,
    connectionKey: client.connectionKey,
    clientId: client.clientId.substring(0, 20) || client.machineName,
    version: clientVer,
    isOutdated: isVersionOutdated(clientVer, latestVersion.value),
    ipAddress: client.ipAddress,
    machineName: client.machineName,
    operatingSystem: `${client.osVersion} ${client.osArchitecture}`,
    isAdmin: client.isAdmin,
    cpus: client.cpuCount,
    type: client.clientType,
    firstSeen: formatDuration(now.getTime() - connectedDate.getTime()),
    lastSeen: lastSeenText,
    status: client.isOnline ? 'online' : 'offline',
    countryCode: client.countryCode?.toLowerCase() || 'xx',
    activityStatus: client.activityStatus || 'Idling',
    raw: client
  }
}

// Real clients from store - filter out any null/undefined entries
const displayClients = computed(() => {
  return clientsStore.clientList
    .filter(client => client != null && client.connectionKey)
    .map(transformClient)
})

// Online clients only
const onlineClients = computed(() => {
  return displayClients.value.filter(c => c.status === 'online')
})

// Offline clients only
const offlineClients = computed(() => {
  return displayClients.value.filter(c => c.status === 'offline')
})

// Current tab's clients
const currentTabClients = computed(() => {
  return machineListTab.value === 'online' ? onlineClients.value : offlineClients.value
})

// Clean up stale selections and context menu refs when client list changes
watch(displayClients, (newClients) => {
  // Get current valid connection keys
  const validKeys = new Set(newClients.map(c => c.connectionKey))

  // Filter out selected IDs that no longer exist
  const filteredIds = selectedClientIds.value.filter(id => validKeys.has(id))
  if (filteredIds.length !== selectedClientIds.value.length) {
    selectedClientIds.value = filteredIds
  }

  // Clear context menu client if it no longer exists or is offline
  if (contextMenuClient.value) {
    const client = newClients.find(c => c.connectionKey === contextMenuClient.value?.connectionKey)
    if (!client || client.status !== 'online') {
      contextMenu.value = false
      contextMenuClient.value = null
    }
  }
})

const headers = [
  { title: 'Status', key: 'status', sortable: true, width: '80px', align: 'center' as const },
  { title: 'Machine Name', key: 'machineName', sortable: true },
  { title: 'IP Address', key: 'ipAddress', sortable: true },
  { title: 'OS', key: 'operatingSystem', sortable: true },
  { title: 'Version', key: 'version', sortable: true, width: '120px' },
  { title: 'Admin', key: 'isAdmin', sortable: true, width: '80px', align: 'center' as const },
  { title: 'Activity', key: 'activityStatus', sortable: true, width: '150px' },
]

const isConnected = computed(() => clientsStore.isConnected)
const isConnecting = computed(() => clientsStore.isConnecting)

const showContextMenu = (e: MouseEvent, item: DisplayClient) => {
  e.preventDefault()

  // Show different context menu for offline clients
  if (item.status === 'offline') {
    offlineContextMenuX.value = e.clientX
    offlineContextMenuY.value = e.clientY
    offlineContextMenuClient.value = item

    // If the clicked item is not in selection, select only this item
    if (!selectedClientIds.value.includes(item.connectionKey)) {
      selectedClientIds.value = [item.connectionKey]
    }

    offlineContextMenu.value = true
    return
  }

  contextMenuX.value = e.clientX
  contextMenuY.value = e.clientY
  contextMenuClient.value = item.raw

  // If the clicked item is not in selection, select only this item
  if (!selectedClientIds.value.includes(item.connectionKey)) {
    selectedClientIds.value = [item.connectionKey]
  }

  contextMenu.value = true
}

// Row props for styling different client types
function getRowProps(data: { item: DisplayClient }): Record<string, any> {
  if (data.item.type === 'INSTALLER') {
    return { class: 'new-bot-row' }
  }
  return {}
}

async function connectToServer() {
  connectionError.value = null
  try {
    await clientsStore.connect()
  } catch (error) {
    connectionError.value = error instanceof Error ? error.message : 'Failed to connect'
  }
}

async function refreshClients() {
  if (!isConnected.value) {
    await connectToServer()
  }
}

function openClientManager() {
  if (contextMenuClient.value) {
    clientManagerClient.value = contextMenuClient.value
    clientManagerDialog.value = true
    contextMenu.value = false
  }
}

function openFileExplorer() {
  if (contextMenuClient.value) {
    fileExplorerClient.value = contextMenuClient.value
    fileExplorerDialog.value = true
    contextMenu.value = false
  }
}

async function disableUac() {
  if (!contextMenuClient.value) return
  contextMenu.value = false
  try {
    const command = {
      connectionKey: contextMenuClient.value.connectionKey,
      commandType: 'disableuac',
      commandText: '',
      timeoutSeconds: 10
    }
    const success = await signalRService.executeCommand(command)
    showSnackbar(success ? `Disable UAC command sent` : `Failed to send Disable UAC command`, success ? 'success' : 'error')
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to disable UAC', 'error')
  }
}

async function shutdownClient() { await sendPowerCommand('shutdown', 'Shut Down') }
async function restartClient() { await sendPowerCommand('restart', 'Restart') }
async function signOutClient() { await sendPowerCommand('signout', 'Sign Out') }

function requestUninstall(andDelete: boolean = false) {
  if (!contextMenuClient.value) return
  contextMenu.value = false
  uninstallTarget.value = contextMenuClient.value
  uninstallAndDelete.value = andDelete
  uninstallDialog.value = true
}

async function confirmUninstall() {
  if (!uninstallTarget.value) return
  const shouldDelete = uninstallAndDelete.value
  const clientId = uninstallTarget.value.clientId
  const machineName = uninstallTarget.value.machineName
  uninstallDialog.value = false

  try {
    const command = {
      connectionKey: uninstallTarget.value.connectionKey,
      commandType: 'uninstall',
      commandText: '',
      timeoutSeconds: 30
    }
    const success = await signalRService.executeCommand(command)

    if (success && shouldDelete) {
      // Wait a moment for the uninstall to process, then delete from DB
      setTimeout(async () => {
        const token = authStore.sessionToken
        if (token) {
          const deleteResult = await deleteDatabaseClient(token, clientId)
          if (deleteResult.success) {
            // Remove from local client store
            clientsStore.removeClient(clientId)
            showSnackbar(`Uninstalled and deleted ${machineName}`, 'success')
          } else {
            showSnackbar(`Uninstalled but failed to delete from database: ${deleteResult.error}`, 'warning')
          }
        }
      }, 2000)
    } else {
      showSnackbar(
        success ? `Uninstall command sent to ${machineName}` : 'Failed to send uninstall command',
        success ? 'success' : 'error'
      )
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to uninstall', 'error')
  }
  uninstallTarget.value = null
  uninstallAndDelete.value = false
}

// Delete offline client from database
function requestDeleteClient(client: DisplayClient) {
  offlineContextMenu.value = false
  deleteTarget.value = client
  deleteDialog.value = true
}

async function confirmDeleteClient() {
  if (!deleteTarget.value) return
  deleteLoading.value = true

  const token = authStore.sessionToken
  if (!token) {
    showSnackbar('Not authenticated', 'error')
    deleteLoading.value = false
    return
  }

  try {
    // Check if multiple clients are selected
    const selectedCount = selectedClientIds.value.length
    if (selectedCount > 1 && selectedClientIds.value.includes(deleteTarget.value.connectionKey)) {
      // Bulk delete - get clientIds from display clients
      const clientIdsToDelete = offlineClients.value
        .filter(c => selectedClientIds.value.includes(c.connectionKey))
        .map(c => c.raw.clientId)

      const result = await bulkDeleteDatabaseClients(token, clientIdsToDelete)
      if (result.success && result.result) {
        // Remove from local store
        clientIdsToDelete.forEach(id => clientsStore.removeClient(id))
        selectedClientIds.value = []
        showSnackbar(`Deleted ${result.result.deletedCount} client(s)`, 'success')
        if (result.result.failedIds.length > 0) {
          showSnackbar(`Failed to delete ${result.result.failedIds.length} client(s)`, 'warning')
        }
      } else {
        showSnackbar(result.error || 'Failed to delete clients', 'error')
      }
    } else {
      // Single delete
      const result = await deleteDatabaseClient(token, deleteTarget.value.raw.clientId)
      if (result.success) {
        clientsStore.removeClient(deleteTarget.value.raw.clientId)
        showSnackbar(`Deleted ${deleteTarget.value.machineName}`, 'success')
      } else {
        showSnackbar(result.error || 'Failed to delete client', 'error')
      }
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to delete', 'error')
  }

  deleteLoading.value = false
  deleteDialog.value = false
  deleteTarget.value = null
}

async function sendPowerCommand(commandType: string, actionName: string) {
  if (!contextMenuClient.value) return
  contextMenu.value = false
  try {
    const command = {
      connectionKey: contextMenuClient.value.connectionKey,
      commandType: commandType,
      commandText: '',
      timeoutSeconds: 10
    }
    const success = await signalRService.executeCommand(command)
    showSnackbar(success ? `${actionName} command sent` : `Failed to send ${actionName} command`, success ? 'success' : 'error')
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : `Failed to ${actionName.toLowerCase()}`, 'error')
  }
}



async function forceUpdateClient() {
  if (!contextMenuClient.value) return
  contextMenu.value = false
  try {
    const command = {
      connectionKey: contextMenuClient.value.connectionKey,
      commandType: 'update',
      commandText: '',
      timeoutSeconds: 300
    }
    const success = await signalRService.executeCommand(command)
    showSnackbar(success ? `Force update command sent` : `Failed to send update command`, success ? 'success' : 'error')
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to send update command', 'error')
  }
}

async function loadLatestVersion() {
  try {
    const token = authStore.sessionToken
    if (!token) return
    const versions = await getVersions(token)
    if (versions.length > 0) {
      latestVersion.value = versions[0] ?? null
    }
  } catch (e) {
    console.error('Failed to load latest version:', e)
  }
}

function showSnackbar(message: string, color: string = 'success') {
  snackbar.value = { show: true, message, color }
}

onMounted(async () => {
  await settingsStore.initialize()
  await loadLatestVersion()
  await connectToServer()
  lastUpdateTime.value = new Date()

  // Start tick interval for updating "Last Update" text every second
  updateTickInterval = setInterval(() => {
    updateTick.value++
  }, 1000)

  // Start auto-refresh every 5 seconds
  autoRefreshInterval.value = setInterval(async () => {
    if (isConnected.value) {
      await refreshClients()
      lastUpdateTime.value = new Date()
    }
  }, 5000)
})

onUnmounted(async () => {
  // Clear tick interval
  if (updateTickInterval) {
    clearInterval(updateTickInterval)
    updateTickInterval = null
  }
  // Clear auto-refresh interval
  if (autoRefreshInterval.value) {
    clearInterval(autoRefreshInterval.value)
    autoRefreshInterval.value = null
  }
  await clientsStore.disconnect()
})
</script>

<template>
  <MainLayout v-model:activeTab="activeTab">
    <!-- Machine List Tab -->
    <div v-if="activeTab === 'clients'" class="d-flex flex-column fill-height">
      <!-- Toolbar -->
      <div class="d-flex align-center mb-4">
        <v-text-field
          v-model="search"
          prepend-inner-icon="mdi-magnify"
          label="Search clients..."
          density="compact"
          variant="outlined"
          hide-details
          class="glass-card rounded-lg"
          style="max-width: 300px"
          bg-color="transparent"
        />
        <v-spacer />
        <div class="d-flex align-center mr-4 text-medium-emphasis">
          <v-icon size="small" class="mr-1">mdi-clock-outline</v-icon>
          <span class="text-body-2">Last Update: {{ lastUpdateText }}</span>
        </div>
        <v-btn prepend-icon="mdi-filter-variant" variant="text" class="glass-card">
          Filter
        </v-btn>
      </div>

      <!-- Online/Offline Tabs -->
      <v-tabs v-model="machineListTab" class="mb-4" color="primary" density="compact">
        <v-tab value="online" class="text-none">
          <v-icon start size="small" color="success">mdi-circle</v-icon>
          Online
          <v-chip size="x-small" class="ml-2" color="success" variant="flat">{{ onlineClients.length }}</v-chip>
        </v-tab>
        <v-tab value="offline" class="text-none">
          <v-icon start size="small" color="grey">mdi-circle-outline</v-icon>
          Offline
          <v-chip size="x-small" class="ml-2" color="grey" variant="flat">{{ offlineClients.length }}</v-chip>
        </v-tab>
      </v-tabs>

      <!-- Data Table -->
      <v-data-table
        v-model="selectedClientIds"
        :headers="headers"
        :items="currentTabClients"
        :items-per-page="-1"
        :search="search"
        item-value="connectionKey"
        show-select
        density="comfortable"
        hover
        fixed-header
        hide-default-footer
        :row-props="getRowProps"
        class="clients-table flex-grow-1 bg-transparent"
        @contextmenu:row="(e: any, { item }: any) => showContextMenu(e, item)"
      >
        <!-- Status Column with Country Flag -->
        <template #item.status="{ item }">
          <div class="d-flex align-center justify-center ga-2">
            <img
              v-if="item.countryCode && item.countryCode !== 'xx'"
              :src="`/flags/${item.countryCode}.svg`"
              :alt="item.countryCode.toUpperCase()"
              :title="item.countryCode.toUpperCase()"
              class="country-flag"
              @error="($event.target as HTMLImageElement).style.display = 'none'"
            />
            <v-badge
              dot
              :color="item.status === 'online' ? 'success' : 'grey'"
              location="bottom end"
              offset-x="2"
              offset-y="2"
            >
              <v-avatar size="28" :color="item.status === 'online' ? 'rgba(34, 197, 94, 0.1)' : 'rgba(148, 163, 184, 0.1)'">
                <v-icon :icon="item.type === 'INSTALLER' ? 'mdi-robot' : 'mdi-monitor'" size="16" :color="item.status === 'online' ? 'success' : 'grey'"></v-icon>
              </v-avatar>
            </v-badge>
          </div>
        </template>

        <!-- Machine Name -->
        <template #item.machineName="{ item }">
          <div class="font-weight-medium">{{ item.machineName }}</div>
          <div class="text-caption text-medium-emphasis">{{ item.clientId }}</div>
        </template>

        <!-- OS -->
        <template #item.operatingSystem="{ item }">
          <div class="d-flex align-center">
            <v-icon
              :icon="item.operatingSystem.toLowerCase().includes('windows') ? 'mdi-microsoft-windows' : 'mdi-linux'"
              size="small"
              class="mr-2 text-medium-emphasis"
            ></v-icon>
            {{ item.operatingSystem }}
          </div>
        </template>

        <!-- Version -->
        <template #item.version="{ item }">
          <v-chip
            size="small"
            :color="item.isOutdated ? 'error' : 'surface-variant'"
            variant="flat"
            class="font-weight-medium"
          >
            {{ item.version }}
            <v-icon v-if="item.isOutdated" end icon="mdi-alert-circle" size="small"></v-icon>
          </v-chip>
        </template>

        <!-- Admin -->
        <template #item.isAdmin="{ item }">
          <div class="d-flex justify-center">
            <v-icon v-if="item.isAdmin" icon="mdi-shield-check" color="primary" size="small"></v-icon>
          </div>
        </template>

        <!-- Activity Status -->
        <template #item.activityStatus="{ item }">
          <v-chip
            size="small"
            :color="item.activityStatus === 'Idling' ? 'surface-variant' : 'warning'"
            variant="flat"
            class="font-weight-medium"
          >
            <v-icon
              v-if="item.activityStatus !== 'Idling'"
              start
              size="small"
              class="activity-spinner"
            >
              mdi-loading
            </v-icon>
            {{ item.activityStatus }}
          </v-chip>
        </template>

        <!-- Empty State -->
        <template #no-data>
          <div class="d-flex flex-column align-center justify-center pa-12">
            <v-icon size="64" color="grey-darken-2" class="mb-4">mdi-monitor-off</v-icon>
            <div class="text-h6 text-grey mb-2">No Clients Connected</div>
            <div class="text-body-2 text-grey mb-6">
              {{ isConnected ? 'Waiting for clients to connect...' : 'Connect to the server to see clients' }}
            </div>
            <v-btn
              v-if="!isConnected"
              color="primary"
              prepend-icon="mdi-wifi"
              :loading="isConnecting"
              @click="connectToServer"
              class="px-6"
            >
              Connect to Server
            </v-btn>
          </div>
        </template>
      </v-data-table>
    </div>

    <!-- Command & Control Tab -->
    <div v-else-if="activeTab === 'command'" class="fill-height d-flex align-center justify-center">
      <div class="text-center">
        <v-icon size="64" color="surface-variant" class="mb-4">mdi-console-line</v-icon>
        <div class="text-h5 font-weight-light text-medium-emphasis">Command & Control</div>
        <div class="text-caption text-disabled mt-2">Module under construction</div>
      </div>
    </div>

    <!-- Station Config Tab -->
    <div v-else-if="activeTab === 'config'" class="fill-height d-flex align-center justify-center">
      <div class="text-center">
        <v-icon size="64" color="surface-variant" class="mb-4">mdi-cog-box</v-icon>
        <div class="text-h5 font-weight-light text-medium-emphasis">Station Configuration</div>
        <div class="text-caption text-disabled mt-2">Module under construction</div>
      </div>
    </div>

    <!-- Saved Data Tab -->
    <div v-else-if="activeTab === 'data'" class="fill-height">
      <SavedDataPanel />
    </div>

    <!-- Server Manager Tab -->
    <div v-else-if="activeTab === 'server'" class="fill-height">
      <VersionManagement />
    </div>

    <!-- Context Menu -->
    <v-menu
      v-model="contextMenu"
      :style="{ position: 'fixed', left: contextMenuX + 'px', top: contextMenuY + 'px' }"
      location-strategy="connected"
    >
      <v-list density="compact" class="glass-panel py-2" width="220">
        <v-list-item v-if="contextMenuClient" class="mb-2 px-4">
          <template #prepend>
            <v-avatar size="24" :color="contextMenuClient.isOnline ? 'success' : 'grey'">
              <v-icon icon="mdi-monitor" size="14" color="white"></v-icon>
            </v-avatar>
          </template>
          <v-list-item-title class="font-weight-bold">{{ contextMenuClient.machineName }}</v-list-item-title>
          <v-list-item-subtitle class="text-caption">{{ contextMenuClient.ipAddress }}</v-list-item-subtitle>
        </v-list-item>
        
        <v-divider class="mb-2 border-opacity-25"></v-divider>
        
        <v-list-item prepend-icon="mdi-monitor-dashboard" title="Client Manager" @click="openClientManager" />
        <v-list-item prepend-icon="mdi-folder-open" title="File Explorer" @click="openFileExplorer" />
        <v-list-item v-if="contextMenuClient?.isUacEnabled" prepend-icon="mdi-shield-off" title="Disable UAC" @click="disableUac" />
        <v-list-item
          prepend-icon="mdi-update"
          title="Manual Update"
          @click="forceUpdateClient"
        />
        
        <v-divider class="my-2 border-opacity-25"></v-divider>
        
        <v-list-item prepend-icon="mdi-power" title="Shut Down" @click="shutdownClient" class="text-error" />
        <v-list-item prepend-icon="mdi-restart" title="Restart" @click="restartClient" class="text-warning" />
        <v-list-item prepend-icon="mdi-logout" title="Sign Out" @click="signOutClient" />

        <v-divider class="my-2 border-opacity-25"></v-divider>

        <v-list-item prepend-icon="mdi-delete-outline" title="Uninstall Client" @click="requestUninstall(false)" class="text-error" />
        <v-list-item prepend-icon="mdi-delete-forever" title="Uninstall & Delete" @click="requestUninstall(true)" class="text-error" />
      </v-list>
    </v-menu>

    <!-- Offline Client Context Menu -->
    <v-menu
      v-model="offlineContextMenu"
      :style="{ position: 'fixed', left: offlineContextMenuX + 'px', top: offlineContextMenuY + 'px' }"
      location-strategy="connected"
    >
      <v-list density="compact" class="glass-panel py-2" width="220">
        <v-list-item v-if="offlineContextMenuClient" class="mb-2 px-4">
          <template #prepend>
            <v-avatar size="24" color="grey">
              <v-icon icon="mdi-monitor" size="14" color="white"></v-icon>
            </v-avatar>
          </template>
          <v-list-item-title class="font-weight-bold">{{ offlineContextMenuClient.machineName }}</v-list-item-title>
          <v-list-item-subtitle class="text-caption">{{ offlineContextMenuClient.ipAddress }}</v-list-item-subtitle>
        </v-list-item>

        <v-divider class="mb-2 border-opacity-25"></v-divider>

        <v-list-item prepend-icon="mdi-delete-forever" title="Delete from Database" @click="requestDeleteClient(offlineContextMenuClient!)" class="text-error" />
        <v-list-item v-if="selectedClientIds.length > 1" prepend-icon="mdi-delete-sweep" :title="`Delete ${selectedClientIds.length} Selected`" @click="requestDeleteClient(offlineContextMenuClient!)" class="text-error" />
      </v-list>
    </v-menu>

    <!-- Client Manager Dialog -->
    <ClientManagerDialog
      v-model="clientManagerDialog"
      :client="clientManagerClient"
    />

    <!-- File Explorer Dialog -->
    <FileExplorerDialog
      v-model="fileExplorerDialog"
      :client="fileExplorerClient"
      :connection-key="fileExplorerClient?.connectionKey"
    />

    <!-- Uninstall Confirmation Dialog -->
    <v-dialog v-model="uninstallDialog" width="450" persistent>
      <v-card class="glass-panel">
        <v-card-title class="d-flex align-center">
          <v-icon color="error" class="mr-2">mdi-alert-circle</v-icon>
          {{ uninstallAndDelete ? 'Confirm Uninstall & Delete' : 'Confirm Uninstall' }}
        </v-card-title>
        <v-card-text>
          <p>Are you sure you want to <strong>completely uninstall</strong> the client from:</p>
          <v-chip class="mt-2" color="primary" variant="tonal">
            {{ uninstallTarget?.machineName }}
          </v-chip>
          <p class="mt-3 text-caption text-medium-emphasis">
            This will remove the scheduled task, all program files, logs, and restore UAC settings.
            The client will disconnect and cannot be recovered.
          </p>
          <p v-if="uninstallAndDelete" class="mt-2 text-caption text-error">
            The client will also be permanently deleted from the database.
          </p>
        </v-card-text>
        <v-card-actions class="justify-end pa-4">
          <v-btn variant="text" @click="uninstallDialog = false">Cancel</v-btn>
          <v-btn color="error" variant="flat" @click="confirmUninstall">
            <v-icon start>mdi-delete-forever</v-icon>
            {{ uninstallAndDelete ? 'Uninstall & Delete' : 'Uninstall' }}
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Delete Client Confirmation Dialog -->
    <v-dialog v-model="deleteDialog" width="450" persistent>
      <v-card class="glass-panel">
        <v-card-title class="d-flex align-center">
          <v-icon color="error" class="mr-2">mdi-alert-circle</v-icon>
          Confirm Delete
        </v-card-title>
        <v-card-text>
          <template v-if="selectedClientIds.length > 1 && deleteTarget && selectedClientIds.includes(deleteTarget.connectionKey)">
            <p>Are you sure you want to <strong>permanently delete</strong> the following {{ selectedClientIds.length }} clients?</p>
            <div class="mt-2 d-flex flex-wrap ga-1">
              <v-chip
                v-for="id in selectedClientIds.slice(0, 5)"
                :key="id"
                size="small"
                color="primary"
                variant="tonal"
              >
                {{ offlineClients.find(c => c.connectionKey === id)?.machineName || id }}
              </v-chip>
              <v-chip v-if="selectedClientIds.length > 5" size="small" color="grey" variant="tonal">
                +{{ selectedClientIds.length - 5 }} more
              </v-chip>
            </div>
          </template>
          <template v-else>
            <p>Are you sure you want to <strong>permanently delete</strong> this client?</p>
            <v-chip class="mt-2" color="primary" variant="tonal">
              {{ deleteTarget?.machineName }}
            </v-chip>
          </template>
          <p class="mt-3 text-caption text-medium-emphasis">
            This will remove all client data, connection history, and command logs from the database.
            This action cannot be undone.
          </p>
        </v-card-text>
        <v-card-actions class="justify-end pa-4">
          <v-btn variant="text" @click="deleteDialog = false" :disabled="deleteLoading">Cancel</v-btn>
          <v-btn color="error" variant="flat" @click="confirmDeleteClient" :loading="deleteLoading">
            <v-icon start>mdi-delete-forever</v-icon>
            Delete
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Snackbar -->
    <v-snackbar v-model="snackbar.show" :color="snackbar.color" :timeout="3000" location="bottom right">
      {{ snackbar.message }}
    </v-snackbar>
  </MainLayout>
</template>

<style scoped>
.clients-table :deep(.v-table__wrapper) {
  overflow-y: auto;
}

.clients-table :deep(thead tr th) {
  background: transparent !important;
  color: rgba(255, 255, 255, 0.7) !important;
  font-weight: 600;
  text-transform: uppercase;
  font-size: 0.75rem;
  letter-spacing: 0.05em;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1) !important;
}

.clients-table :deep(tbody tr:hover) {
  background: rgba(255, 255, 255, 0.03) !important;
}

.new-bot-row {
  background: rgba(59, 130, 246, 0.1);
}

.country-flag {
  width: 22px;
  height: 16px;
  border-radius: 2px;
  object-fit: cover;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.2);
}

.activity-spinner {
  animation: spin 1s linear infinite;
}

@keyframes spin {
  from {
    transform: rotate(0deg);
  }
  to {
    transform: rotate(360deg);
  }
}
</style>
