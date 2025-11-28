<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useClientsStore } from '@/stores/clients'
import { useSettingsStore } from '@/stores/settings'
import { signalRService } from '@/services/signalr'
import ClientManagerDialog from '@/components/ClientManagerDialog.vue'
import type { ClientDto } from '@/types/client'

const router = useRouter()
const authStore = useAuthStore()
const clientsStore = useClientsStore()
const settingsStore = useSettingsStore()

const activeTab = ref('clients')

// Context menu state
const contextMenu = ref(false)
const contextMenuX = ref(0)
const contextMenuY = ref(0)
const contextMenuClient = ref<ClientDto | null>(null)

// Client Manager dialog state
const clientManagerDialog = ref(false)
const clientManagerClient = ref<ClientDto | null>(null)

// Selection state
const selectedClients = ref<ClientDto[]>([])

// Search state
const search = ref('')

// Connection status
const connectionError = ref<string | null>(null)

// Snackbar state
const snackbar = ref({
  show: false,
  message: '',
  color: 'success'
})

// Transform ClientDto to display format
interface DisplayClient {
  id: string
  connectionKey: string
  clientId: string
  version: string
  location: string
  ipAddress: string
  machineName: string
  operatingSystem: string
  isAdmin: boolean
  cpus: number
  type: string
  firstSeen: string
  lastSeen: string
  status: 'online' | 'offline'
  raw: ClientDto
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

    if (days > 0) return `${days}D ${hours % 24}H ${minutes % 60}M`
    if (hours > 0) return `${hours}H ${minutes % 60}M`
    if (minutes > 0) return `${minutes}M`
    return `${seconds}s`
  }

  const timeSinceHeartbeat = now.getTime() - lastHeartbeat.getTime()
  const lastSeenText = client.isOnline
    ? 'Online Now'
    : `${formatDuration(timeSinceHeartbeat)} ago`

  return {
    id: client.connectionKey,
    connectionKey: client.connectionKey,
    clientId: client.clientId.substring(0, 20) || client.machineName,
    version: client.clientVersion || '1.0',
    location: client.ipAddress.split('.')[0] || 'Unknown',
    ipAddress: client.ipAddress,
    machineName: client.machineName,
    operatingSystem: `${client.osVersion} ${client.osArchitecture}`,
    isAdmin: client.isAdmin,
    cpus: client.cpuCount,
    type: client.clientType,
    firstSeen: formatDuration(now.getTime() - connectedDate.getTime()),
    lastSeen: lastSeenText,
    status: client.isOnline ? 'online' : 'offline',
    raw: client
  }
}

// Real clients from store
const displayClients = computed(() => {
  return clientsStore.clientList.map(transformClient)
})

// Sorted clients - online first, then offline
const sortedClients = computed(() => {
  return [...displayClients.value].sort((a, b) => {
    if (a.status === 'online' && b.status === 'offline') return -1
    if (a.status === 'offline' && b.status === 'online') return 1
    return 0
  })
})

const headers = [
  { title: 'Client ID', key: 'clientId', sortable: true },
  { title: 'Version', key: 'version', sortable: true, width: '80px' },
  { title: 'Location', key: 'location', sortable: true, width: '80px' },
  { title: 'IP Address', key: 'ipAddress', sortable: true },
  { title: 'Machine Name', key: 'machineName', sortable: true },
  { title: 'Operating System', key: 'operatingSystem', sortable: true },
  { title: 'Admin', key: 'isAdmin', sortable: true, width: '70px' },
  { title: 'CPUs', key: 'cpus', sortable: true, width: '60px' },
  { title: 'Type', key: 'type', sortable: true, width: '90px' },
  { title: 'First Seen', key: 'firstSeen', sortable: true },
  { title: 'Last Seen', key: 'lastSeen', sortable: true },
]

const onlineCount = computed(() => clientsStore.onlineCount)
const totalCount = computed(() => clientsStore.clientCount)
const selectedCount = computed(() => selectedClients.value.length)
const isConnected = computed(() => clientsStore.isConnected)
const isConnecting = computed(() => clientsStore.isConnecting)
const isReconnecting = computed(() => clientsStore.isReconnecting)

const showContextMenu = (e: MouseEvent, item: DisplayClient) => {
  e.preventDefault()
  contextMenuX.value = e.clientX
  contextMenuY.value = e.clientY
  contextMenuClient.value = item.raw

  // If the clicked item is not in selection, select only this item
  if (!selectedClients.value.find(c => c.connectionKey === item.connectionKey)) {
    selectedClients.value = [item as any]
  }

  contextMenu.value = true
}

// Row class for styling different client types - can be used with v-data-table's :row-props
function getRowClass(item: DisplayClient): string {
  if (item.type === 'INSTALLER') return 'new-bot-row'
  return ''
}

// Export to avoid unused variable warning (can be used in template)
defineExpose({ getRowClass })

async function handleLogout() {
  await clientsStore.disconnect()
  await authStore.logout()
  router.push({ name: 'login' })
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

function toggleNotifications() {
  settingsStore.setNotificationsEnabled(!settingsStore.notificationsEnabled)
}

function openClientManager() {
  if (contextMenuClient.value) {
    clientManagerClient.value = contextMenuClient.value
    clientManagerDialog.value = true
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

    if (success) {
      snackbar.value = {
        show: true,
        message: `Disable UAC command sent to ${contextMenuClient.value.machineName}`,
        color: 'success'
      }
    } else {
      snackbar.value = {
        show: true,
        message: `Failed to send Disable UAC command to ${contextMenuClient.value.machineName}`,
        color: 'error'
      }
    }
  } catch (e) {
    snackbar.value = {
      show: true,
      message: e instanceof Error ? e.message : 'Failed to disable UAC',
      color: 'error'
    }
  }
}

async function shutdownClient() {
  await sendPowerCommand('shutdown', 'Shut Down')
}

async function restartClient() {
  await sendPowerCommand('restart', 'Restart')
}

async function signOutClient() {
  await sendPowerCommand('signout', 'Sign Out')
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

    if (success) {
      snackbar.value = {
        show: true,
        message: `${actionName} command sent to ${contextMenuClient.value.machineName}`,
        color: 'success'
      }
    } else {
      snackbar.value = {
        show: true,
        message: `Failed to send ${actionName} command to ${contextMenuClient.value.machineName}`,
        color: 'error'
      }
    }
  } catch (e) {
    snackbar.value = {
      show: true,
      message: e instanceof Error ? e.message : `Failed to ${actionName.toLowerCase()}`,
      color: 'error'
    }
  }
}

onMounted(async () => {
  // Initialize settings
  await settingsStore.initialize()
  // Connect to SignalR hub
  await connectToServer()
})

onUnmounted(async () => {
  // Disconnect when leaving dashboard
  await clientsStore.disconnect()
})
</script>

<template>
  <v-app>
    <v-main class="d-flex flex-column main-content">
      <!-- Header Toolbar with Logout -->
      <v-toolbar density="compact" color="surface" class="px-2" style="flex: 0 0 auto;">
        <v-icon class="mr-2">mdi-shield-lock</v-icon>
        <v-toolbar-title class="text-subtitle-1 font-weight-bold">
          TorGames Control Panel
        </v-toolbar-title>
        <v-spacer />

        <!-- Connection Status -->
        <v-chip
          :color="isConnected ? 'success' : (isConnecting || isReconnecting) ? 'warning' : 'error'"
          size="small"
          class="mr-2"
        >
          <v-icon start size="small">
            {{ isConnected ? 'mdi-wifi' : (isConnecting || isReconnecting) ? 'mdi-wifi-strength-1' : 'mdi-wifi-off' }}
          </v-icon>
          {{ isConnected ? 'Connected' : isReconnecting ? 'Reconnecting...' : isConnecting ? 'Connecting...' : 'Disconnected' }}
        </v-chip>

        <!-- Notification Toggle -->
        <v-btn
          :icon="settingsStore.notificationsEnabled ? 'mdi-bell' : 'mdi-bell-off'"
          variant="text"
          size="small"
          :color="settingsStore.notificationsEnabled ? 'primary' : 'grey'"
          @click="toggleNotifications"
          class="mr-2"
        >
          <v-tooltip activator="parent" location="bottom">
            {{ settingsStore.notificationsEnabled ? 'Disable notifications' : 'Enable notifications' }}
          </v-tooltip>
        </v-btn>

        <v-btn
          variant="text"
          prepend-icon="mdi-logout"
          @click="handleLogout"
        >
          Logout
        </v-btn>
      </v-toolbar>

      <v-divider />

      <!-- Connection Error Alert -->
      <v-alert
        v-if="connectionError"
        type="error"
        closable
        density="compact"
        class="mx-2 mt-2"
        @click:close="connectionError = null"
      >
        {{ connectionError }}
        <template #append>
          <v-btn variant="text" size="small" @click="connectToServer">Retry</v-btn>
        </template>
      </v-alert>

      <!-- Tabs -->
      <v-tabs v-model="activeTab" bg-color="surface" density="compact" grow style="flex: 0 0 auto;">
        <v-tab value="clients" prepend-icon="mdi-monitor-multiple">
          MACHINE LIST
        </v-tab>
        <v-tab value="command" prepend-icon="mdi-console">
          COMMAND AND CONTROL
        </v-tab>
        <v-tab value="config" prepend-icon="mdi-cog">
          STATION CONFIGURATION
        </v-tab>
        <v-tab value="data" prepend-icon="mdi-database">
          SAVED DATA
        </v-tab>
        <v-tab value="server" prepend-icon="mdi-server">
          SERVER MANAGEMENT
        </v-tab>
      </v-tabs>

      <v-divider />

      <!-- Tab Content -->
      <v-window v-model="activeTab" class="flex-grow-1" style="overflow: hidden; min-height: 0;">
        <!-- Machine List Tab -->
        <v-window-item value="clients" class="fill-height" style="overflow: hidden;">
          <v-data-table
            v-model="selectedClients"
            :headers="headers"
            :items="sortedClients"
            :items-per-page="-1"
            :search="search"
            item-value="id"
            show-select
            density="compact"
            hover
            fixed-header
            hide-default-footer
            class="clients-table fill-height"
            @contextmenu:row="(e: any, { item }: any) => showContextMenu(e, item)"
          >
            <template #top>
              <v-toolbar density="compact" color="transparent">
                <v-text-field
                  v-model="search"
                  prepend-inner-icon="mdi-magnify"
                  label="Search clients..."
                  density="compact"
                  variant="outlined"
                  hide-details
                  clearable
                  class="mx-2"
                  style="max-width: 300px"
                />
                <v-spacer />
                <v-btn-group density="compact" variant="outlined">
                  <v-btn prepend-icon="mdi-refresh" :loading="isConnecting" @click="refreshClients">
                    Refresh
                  </v-btn>
                  <v-btn prepend-icon="mdi-filter">Filter</v-btn>
                </v-btn-group>
              </v-toolbar>
            </template>

            <!-- Empty state -->
            <template #no-data>
              <div class="d-flex flex-column align-center justify-center pa-8">
                <v-icon size="64" color="grey" class="mb-4">mdi-monitor-off</v-icon>
                <div class="text-h6 text-grey mb-2">No Clients Connected</div>
                <div class="text-body-2 text-grey mb-4">
                  {{ isConnected ? 'Waiting for clients to connect...' : 'Connect to the server to see clients' }}
                </div>
                <v-btn
                  v-if="!isConnected"
                  color="primary"
                  prepend-icon="mdi-refresh"
                  :loading="isConnecting"
                  @click="connectToServer"
                >
                  Connect to Server
                </v-btn>
              </div>
            </template>

            <template #item.clientId="{ item }">
              <div class="d-flex align-center">
                <v-icon
                  :color="item.status === 'online' ? 'success' : 'grey'"
                  size="small"
                  class="mr-2"
                >
                  mdi-circle
                </v-icon>
                {{ item.clientId }}
              </div>
            </template>

            <template #item.isAdmin="{ item }">
              <v-icon
                :color="item.isAdmin ? 'success' : 'error'"
                size="small"
              >
                {{ item.isAdmin ? 'mdi-check' : 'mdi-close' }}
              </v-icon>
            </template>

            <template #item.type="{ item }">
              <v-chip
                :color="item.type === 'INSTALLER' ? 'info' : item.type === 'CLIENT' ? 'success' : 'default'"
                size="small"
                variant="tonal"
              >
                {{ item.type }}
              </v-chip>
            </template>

            <template #item.lastSeen="{ item }">
              <span :class="item.lastSeen === 'Online Now' ? 'text-success' : ''">
                {{ item.lastSeen }}
              </span>
            </template>
          </v-data-table>
        </v-window-item>

        <!-- Other tabs placeholder -->
        <v-window-item value="command" class="fill-height pa-4">
          <v-card class="fill-height d-flex align-center justify-center">
            <v-card-text class="text-center">
              <v-icon size="64" color="grey">mdi-console</v-icon>
              <div class="text-h6 mt-4 text-grey">Command and Control</div>
              <div class="text-body-2 text-grey">Coming soon...</div>
            </v-card-text>
          </v-card>
        </v-window-item>

        <v-window-item value="config" class="fill-height pa-4">
          <v-card class="fill-height d-flex align-center justify-center">
            <v-card-text class="text-center">
              <v-icon size="64" color="grey">mdi-cog</v-icon>
              <div class="text-h6 mt-4 text-grey">Station Configuration</div>
              <div class="text-body-2 text-grey">Coming soon...</div>
            </v-card-text>
          </v-card>
        </v-window-item>

        <v-window-item value="data" class="fill-height pa-4">
          <v-card class="fill-height d-flex align-center justify-center">
            <v-card-text class="text-center">
              <v-icon size="64" color="grey">mdi-database</v-icon>
              <div class="text-h6 mt-4 text-grey">Saved Data</div>
              <div class="text-body-2 text-grey">Coming soon...</div>
            </v-card-text>
          </v-card>
        </v-window-item>

        <v-window-item value="server" class="fill-height pa-4">
          <v-card class="fill-height d-flex align-center justify-center">
            <v-card-text class="text-center">
              <v-icon size="64" color="grey">mdi-server</v-icon>
              <div class="text-h6 mt-4 text-grey">Server Management</div>
              <div class="text-body-2 text-grey">Coming soon...</div>
            </v-card-text>
          </v-card>
        </v-window-item>
      </v-window>

      <!-- Status Bar -->
      <v-footer height="32" color="surface" class="px-4 border-t" style="flex: 0 0 32px; min-height: 32px;">
        <v-icon size="small" color="success" class="mr-1">mdi-circle</v-icon>
        <span class="text-body-2 mr-4">Clients Online: {{ onlineCount }}</span>

        <v-icon size="small" color="primary" class="mr-1">mdi-cursor-default-click</v-icon>
        <span class="text-body-2 mr-4">Clients Selected: {{ selectedCount }}</span>

        <v-icon size="small" color="grey" class="mr-1">mdi-sigma</v-icon>
        <span class="text-body-2">Total Clients: {{ totalCount }}</span>

        <v-spacer />

        <v-btn variant="text" size="small" prepend-icon="mdi-access-point">
          Port Management / Listen on Ports
          <v-icon end size="small">mdi-menu-down</v-icon>
        </v-btn>
      </v-footer>
    </v-main>

    <!-- Context Menu -->
    <v-overlay
      v-model="contextMenu"
      :scrim="false"
      class="context-menu-overlay"
      @click="contextMenu = false"
    >
      <v-card
        class="context-menu-card"
        :style="{ position: 'fixed', left: contextMenuX + 'px', top: contextMenuY + 'px' }"
        @click.stop
      >
        <v-list density="compact" nav>
          <v-list-item v-if="contextMenuClient" class="context-menu-header">
            <template #prepend>
              <v-icon :color="contextMenuClient.isOnline ? 'success' : 'grey'" size="small">mdi-circle</v-icon>
            </template>
            <v-list-item-title class="font-weight-bold">{{ contextMenuClient.machineName }}</v-list-item-title>
            <v-list-item-subtitle>{{ contextMenuClient.ipAddress }}</v-list-item-subtitle>
          </v-list-item>
          <v-divider class="my-1" />
          <v-list-item prepend-icon="mdi-monitor" title="Client Manager" @click="openClientManager" />
          <v-list-item prepend-icon="mdi-shield-off" title="Disable UAC" @click="disableUac" />
          <v-divider class="my-1" />
          <v-list-item prepend-icon="mdi-remote-desktop" title="Remote Desktop" @click="contextMenu = false" />
          <v-list-item prepend-icon="mdi-webcam" title="Remote Webcam" @click="contextMenu = false" />
          <v-divider class="my-1" />
          <v-list-item prepend-icon="mdi-lan" title="Networking" @click="contextMenu = false">
            <template #append>
              <v-icon size="small">mdi-chevron-right</v-icon>
            </template>
          </v-list-item>
          <v-menu open-on-hover location="end" :close-on-content-click="false">
            <template #activator="{ props }">
              <v-list-item v-bind="props" prepend-icon="mdi-dots-horizontal" title="Miscellaneous">
                <template #append>
                  <v-icon size="small">mdi-chevron-right</v-icon>
                </template>
              </v-list-item>
            </template>
            <v-list density="compact" class="py-0">
              <v-list-item prepend-icon="mdi-power" title="Shut Down" @click="shutdownClient" />
              <v-list-item prepend-icon="mdi-restart" title="Restart" @click="restartClient" />
              <v-list-item prepend-icon="mdi-logout" title="Sign Out" @click="signOutClient" />
            </v-list>
          </v-menu>
          <v-divider class="my-1" />
          <v-list-item prepend-icon="mdi-information" title="Client General" @click="contextMenu = false">
            <template #append>
              <v-icon size="small">mdi-chevron-right</v-icon>
            </template>
          </v-list-item>
          <v-list-item prepend-icon="mdi-content-save" title="Saved Data" @click="contextMenu = false">
            <template #append>
              <v-icon size="small">mdi-chevron-right</v-icon>
            </template>
          </v-list-item>
          <v-divider class="my-1" />
          <v-list-item prepend-icon="mdi-pound" title="Select # of Clients" @click="contextMenu = false" />
          <v-list-item prepend-icon="mdi-select-all" title="Select All Clients" @click="contextMenu = false" />
        </v-list>
      </v-card>
    </v-overlay>

    <!-- Client Manager Dialog -->
    <ClientManagerDialog
      v-model="clientManagerDialog"
      :client="clientManagerClient"
    />

    <!-- Snackbar for notifications -->
    <v-snackbar v-model="snackbar.show" :color="snackbar.color" :timeout="3000">
      {{ snackbar.message }}
    </v-snackbar>
  </v-app>
</template>

<style scoped>
.main-content {
  height: 100vh !important;
  max-height: 100vh !important;
  min-height: 0 !important;
  overflow: hidden !important;
  padding: 0 !important;
}

.main-content :deep(.v-main__wrap) {
  display: flex !important;
  flex-direction: column !important;
  height: 100% !important;
  max-height: 100% !important;
  overflow: hidden !important;
  padding: 0 !important;
}

.clients-table {
  display: flex !important;
  flex-direction: column !important;
  overflow: hidden !important;
  height: 100% !important;
  max-height: 100% !important;
}

.clients-table :deep(.v-table__wrapper) {
  flex: 1 1 0 !important;
  min-height: 0 !important;
  overflow-y: auto !important;
}

.clients-table :deep(.v-data-table__tr:hover) {
  background-color: rgba(var(--v-theme-primary), 0.1) !important;
}

.clients-table :deep(.v-data-table__tr--selected) {
  background-color: rgba(var(--v-theme-primary), 0.2) !important;
}

.new-bot-row {
  background-color: rgba(var(--v-theme-info), 0.1);
}

.context-menu-overlay {
  align-items: flex-start !important;
  justify-content: flex-start !important;
}

.context-menu-card {
  z-index: 9999;
  min-width: 280px;
}

.context-menu-card :deep(.v-list-item) {
  cursor: pointer;
}

.context-menu-card :deep(.v-list-item:hover:not(.context-menu-header)) {
  background-color: rgba(var(--v-theme-primary), 0.1);
}

.context-menu-header {
  pointer-events: none;
  background-color: rgba(var(--v-theme-surface-variant), 0.3);
}
</style>
