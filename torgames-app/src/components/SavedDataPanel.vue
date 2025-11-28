<script setup lang="ts">
import { ref, computed, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import {
  getDatabaseStats,
  getDatabaseClients,
  getDatabaseClient,
  updateDatabaseClient,
  deleteDatabaseClient,
  getCommandLogs,
  getDatabaseInfo,
  getDatabaseBackupUrl,
  type DatabaseStats,
  type DatabaseClient,
  type DatabaseClientDetail,
  type CommandLog,
  type DatabaseInfo
} from '@/services/api'

const authStore = useAuthStore()

// Tab state
const activeTab = ref('overview')

// Loading states
const loading = ref(false)
const statsLoading = ref(false)

// Data
const stats = ref<DatabaseStats | null>(null)
const clients = ref<DatabaseClient[]>([])
const commandLogs = ref<CommandLog[]>([])
const dbInfo = ref<DatabaseInfo | null>(null)

// Filter state
const clientFilter = ref<'all' | 'flagged' | 'blocked'>('all')
const search = ref('')

// Client detail dialog
const clientDetailDialog = ref(false)
const selectedClient = ref<DatabaseClientDetail | null>(null)
const clientDetailLoading = ref(false)

// Delete confirmation dialog
const deleteDialog = ref(false)
const clientToDelete = ref<DatabaseClient | null>(null)
const deleteLoading = ref(false)

// Snackbar
const snackbar = ref({
  show: false,
  message: '',
  color: 'success'
})

// Client table headers
const clientHeaders = [
  { title: 'Machine', key: 'machineName', sortable: true },
  { title: 'Username', key: 'username', sortable: true },
  { title: 'IP Address', key: 'lastIpAddress', sortable: true },
  { title: 'Country', key: 'countryCode', sortable: true, width: '80px' },
  { title: 'Version', key: 'clientVersion', sortable: true, width: '100px' },
  { title: 'First Seen', key: 'firstSeenAt', sortable: true },
  { title: 'Last Seen', key: 'lastSeenAt', sortable: true },
  { title: 'Connections', key: 'totalConnections', sortable: true, width: '100px' },
  { title: 'Flags', key: 'flags', sortable: false, width: '100px' },
  { title: 'Actions', key: 'actions', sortable: false, width: '100px', align: 'end' },
]

// Command log headers
const commandHeaders = [
  { title: 'Time', key: 'sentAt', sortable: true, width: '160px' },
  { title: 'Type', key: 'commandType', sortable: true, width: '120px' },
  { title: 'Client', key: 'clientId', sortable: true },
  { title: 'Delivered', key: 'wasDelivered', sortable: true, width: '100px' },
  { title: 'Success', key: 'success', sortable: true, width: '100px' },
]

// Computed filtered clients
const filteredClients = computed(() => {
  let result = clients.value

  if (clientFilter.value === 'flagged') {
    result = result.filter(c => c.isFlagged)
  } else if (clientFilter.value === 'blocked') {
    result = result.filter(c => c.isBlocked)
  }

  return result
})

function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + (sizes[i] ?? '')
}

function formatDate(dateString: string): string {
  const date = new Date(dateString)
  return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
}

function formatRelativeTime(dateString: string): string {
  const date = new Date(dateString)
  const now = new Date()
  const diff = now.getTime() - date.getTime()

  const minutes = Math.floor(diff / 60000)
  const hours = Math.floor(diff / 3600000)
  const days = Math.floor(diff / 86400000)

  if (days > 0) return `${days}d ago`
  if (hours > 0) return `${hours}h ago`
  if (minutes > 0) return `${minutes}m ago`
  return 'Just now'
}

async function loadStats() {
  statsLoading.value = true
  try {
    const token = authStore.sessionToken
    if (!token) return

    stats.value = await getDatabaseStats(token)
    dbInfo.value = await getDatabaseInfo(token)
  } catch (e) {
    console.error('Failed to load stats:', e)
  } finally {
    statsLoading.value = false
  }
}

async function loadClients() {
  loading.value = true
  try {
    const token = authStore.sessionToken
    if (!token) return

    clients.value = await getDatabaseClients(token, { limit: 1000 })
  } catch (e) {
    console.error('Failed to load clients:', e)
  } finally {
    loading.value = false
  }
}

async function loadCommandLogs() {
  loading.value = true
  try {
    const token = authStore.sessionToken
    if (!token) return

    commandLogs.value = await getCommandLogs(token, { limit: 100 })
  } catch (e) {
    console.error('Failed to load command logs:', e)
  } finally {
    loading.value = false
  }
}

async function openClientDetail(client: DatabaseClient) {
  clientDetailLoading.value = true
  clientDetailDialog.value = true

  try {
    const token = authStore.sessionToken
    if (!token) return

    selectedClient.value = await getDatabaseClient(token, client.clientId)
  } catch (e) {
    console.error('Failed to load client detail:', e)
    showSnackbar('Failed to load client details', 'error')
  } finally {
    clientDetailLoading.value = false
  }
}

async function toggleFlag(client: DatabaseClient) {
  try {
    const token = authStore.sessionToken
    if (!token) return

    const result = await updateDatabaseClient(token, client.clientId, {
      isFlagged: !client.isFlagged
    })

    if (result.success) {
      client.isFlagged = !client.isFlagged
      showSnackbar(client.isFlagged ? 'Client flagged' : 'Client unflagged', 'success')
    } else {
      showSnackbar(result.error || 'Failed to update client', 'error')
    }
  } catch (e) {
    showSnackbar('Failed to update client', 'error')
  }
}

async function toggleBlock(client: DatabaseClient) {
  try {
    const token = authStore.sessionToken
    if (!token) return

    const result = await updateDatabaseClient(token, client.clientId, {
      isBlocked: !client.isBlocked
    })

    if (result.success) {
      client.isBlocked = !client.isBlocked
      showSnackbar(client.isBlocked ? 'Client blocked' : 'Client unblocked', 'success')
    } else {
      showSnackbar(result.error || 'Failed to update client', 'error')
    }
  } catch (e) {
    showSnackbar('Failed to update client', 'error')
  }
}

function openDeleteDialog(client: DatabaseClient) {
  clientToDelete.value = client
  deleteDialog.value = true
}

async function confirmDelete() {
  if (!clientToDelete.value) return

  deleteLoading.value = true
  try {
    const token = authStore.sessionToken
    if (!token) return

    const result = await deleteDatabaseClient(token, clientToDelete.value.clientId)

    if (result.success) {
      clients.value = clients.value.filter(c => c.clientId !== clientToDelete.value?.clientId)
      deleteDialog.value = false
      showSnackbar('Client deleted from database', 'success')
      await loadStats()
    } else {
      showSnackbar(result.error || 'Failed to delete client', 'error')
    }
  } catch (e) {
    showSnackbar('Failed to delete client', 'error')
  } finally {
    deleteLoading.value = false
    clientToDelete.value = null
  }
}

async function downloadBackup() {
  try {
    const token = authStore.sessionToken
    if (!token) {
      showSnackbar('Not authenticated', 'error')
      return
    }

    const url = getDatabaseBackupUrl()

    // Create a temporary link with authorization header
    const response = await fetch(url, {
      headers: {
        'Authorization': `Bearer ${token}`
      }
    })

    if (!response.ok) {
      showSnackbar('Failed to download backup', 'error')
      return
    }

    const blob = await response.blob()
    const downloadUrl = window.URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = downloadUrl
    a.download = `torgames_backup_${new Date().toISOString().split('T')[0]}.db`
    document.body.appendChild(a)
    a.click()
    window.URL.revokeObjectURL(downloadUrl)
    document.body.removeChild(a)

    showSnackbar('Database backup downloaded', 'success')
  } catch (e) {
    console.error('Download failed:', e)
    showSnackbar('Failed to download backup', 'error')
  }
}

function showSnackbar(message: string, color: string = 'success') {
  snackbar.value = { show: true, message, color }
}

onMounted(async () => {
  await loadStats()
  await loadClients()
})
</script>

<template>
  <div class="fill-height d-flex flex-column">
    <!-- Toolbar -->
    <div class="d-flex align-center mb-4">
      <div>
        <div class="text-h5 font-weight-light">Saved Data</div>
        <div class="text-caption text-medium-emphasis">Persistent client database and history</div>
      </div>
      <v-spacer />
      <v-btn
        prepend-icon="mdi-refresh"
        variant="text"
        size="small"
        :loading="loading || statsLoading"
        class="glass-card mr-2"
        @click="loadStats(); loadClients()"
      >
        Refresh
      </v-btn>
      <v-btn
        prepend-icon="mdi-download"
        color="primary"
        @click="downloadBackup"
      >
        Download Backup
      </v-btn>
    </div>

    <!-- Tabs -->
    <v-tabs v-model="activeTab" class="mb-4" color="primary" density="compact">
      <v-tab value="overview" class="text-none">
        <v-icon start size="small">mdi-view-dashboard</v-icon>
        Overview
      </v-tab>
      <v-tab value="clients" class="text-none">
        <v-icon start size="small">mdi-account-multiple</v-icon>
        Saved Clients
      </v-tab>
      <v-tab value="commands" class="text-none">
        <v-icon start size="small">mdi-console</v-icon>
        Command History
      </v-tab>
    </v-tabs>

    <!-- Overview Tab -->
    <div v-if="activeTab === 'overview'" class="flex-grow-1" style="overflow-y: auto;">
      <!-- Stats Cards -->
      <v-row class="mb-4">
        <v-col cols="12" md="3">
          <v-card class="glass-panel rounded-xl border-none" variant="flat">
            <v-card-text class="pa-4">
              <div class="d-flex align-center mb-2">
                <v-icon color="primary" size="small" class="mr-2">mdi-account-group</v-icon>
                <span class="text-caption text-medium-emphasis">Total Clients</span>
              </div>
              <div class="text-h4 font-weight-bold">{{ stats?.totalClients ?? 0 }}</div>
              <div class="text-caption text-medium-emphasis mt-1">
                {{ stats?.clientsSeenToday ?? 0 }} seen today
              </div>
            </v-card-text>
          </v-card>
        </v-col>

        <v-col cols="12" md="3">
          <v-card class="glass-panel rounded-xl border-none" variant="flat">
            <v-card-text class="pa-4">
              <div class="d-flex align-center mb-2">
                <v-icon color="warning" size="small" class="mr-2">mdi-flag</v-icon>
                <span class="text-caption text-medium-emphasis">Flagged</span>
              </div>
              <div class="text-h4 font-weight-bold text-warning">{{ stats?.flaggedClients ?? 0 }}</div>
              <div class="text-caption text-medium-emphasis mt-1">
                {{ stats?.blockedClients ?? 0 }} blocked
              </div>
            </v-card-text>
          </v-card>
        </v-col>

        <v-col cols="12" md="3">
          <v-card class="glass-panel rounded-xl border-none" variant="flat">
            <v-card-text class="pa-4">
              <div class="d-flex align-center mb-2">
                <v-icon color="info" size="small" class="mr-2">mdi-connection</v-icon>
                <span class="text-caption text-medium-emphasis">Total Connections</span>
              </div>
              <div class="text-h4 font-weight-bold">{{ stats?.totalConnections ?? 0 }}</div>
              <div class="text-caption text-medium-emphasis mt-1">
                {{ stats?.connectionsToday ?? 0 }} today
              </div>
            </v-card-text>
          </v-card>
        </v-col>

        <v-col cols="12" md="3">
          <v-card class="glass-panel rounded-xl border-none" variant="flat">
            <v-card-text class="pa-4">
              <div class="d-flex align-center mb-2">
                <v-icon color="success" size="small" class="mr-2">mdi-console</v-icon>
                <span class="text-caption text-medium-emphasis">Total Commands</span>
              </div>
              <div class="text-h4 font-weight-bold">{{ stats?.totalCommands ?? 0 }}</div>
              <div class="text-caption text-medium-emphasis mt-1">
                {{ stats?.commandsToday ?? 0 }} today
              </div>
            </v-card-text>
          </v-card>
        </v-col>
      </v-row>

      <!-- Database Info Card -->
      <v-card class="glass-panel rounded-xl border-none" variant="flat">
        <v-card-title class="d-flex align-center px-4 py-3">
          <v-icon class="mr-2" color="primary" size="small">mdi-database</v-icon>
          Database Information
        </v-card-title>
        <v-divider class="border-opacity-10" />
        <v-card-text class="pa-4">
          <v-row>
            <v-col cols="12" md="4">
              <div class="text-caption text-medium-emphasis">File Size</div>
              <div class="text-body-1 font-weight-medium">{{ dbInfo ? formatBytes(dbInfo.fileSizeBytes) : '-' }}</div>
            </v-col>
            <v-col cols="12" md="4">
              <div class="text-caption text-medium-emphasis">Created</div>
              <div class="text-body-1 font-weight-medium">{{ dbInfo ? formatDate(dbInfo.createdAt) : '-' }}</div>
            </v-col>
            <v-col cols="12" md="4">
              <div class="text-caption text-medium-emphasis">Last Modified</div>
              <div class="text-body-1 font-weight-medium">{{ dbInfo ? formatDate(dbInfo.modifiedAt) : '-' }}</div>
            </v-col>
          </v-row>
        </v-card-text>
      </v-card>
    </div>

    <!-- Clients Tab -->
    <div v-else-if="activeTab === 'clients'" class="flex-grow-1 d-flex flex-column">
      <!-- Filter Bar -->
      <div class="d-flex align-center mb-4 ga-2">
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
        <v-btn-toggle v-model="clientFilter" mandatory density="compact" class="glass-card rounded-lg">
          <v-btn value="all" size="small">All</v-btn>
          <v-btn value="flagged" size="small">
            <v-icon start size="small" color="warning">mdi-flag</v-icon>
            Flagged
          </v-btn>
          <v-btn value="blocked" size="small">
            <v-icon start size="small" color="error">mdi-block-helper</v-icon>
            Blocked
          </v-btn>
        </v-btn-toggle>
      </div>

      <!-- Clients Table -->
      <v-card class="glass-panel flex-grow-1 d-flex flex-column rounded-xl border-none" variant="flat">
        <v-data-table
          :headers="clientHeaders"
          :items="filteredClients"
          :items-per-page="50"
          :search="search"
          density="comfortable"
          hover
          fixed-header
          class="saved-clients-table flex-grow-1 bg-transparent"
        >
          <template #item.machineName="{ item }">
            <div class="font-weight-medium">{{ item.machineName }}</div>
            <div class="text-caption text-medium-emphasis font-mono">{{ item.clientId.substring(0, 16) }}...</div>
          </template>

          <template #item.countryCode="{ item }">
            <img
              v-if="item.countryCode"
              :src="`/flags/${item.countryCode.toLowerCase()}.svg`"
              :alt="item.countryCode"
              class="country-flag"
              @error="($event.target as HTMLImageElement).style.display = 'none'"
            />
            <span v-else>-</span>
          </template>

          <template #item.clientVersion="{ item }">
            <v-chip size="small" variant="flat" color="surface-variant">
              {{ item.clientVersion || '-' }}
            </v-chip>
          </template>

          <template #item.firstSeenAt="{ item }">
            <span class="text-caption">{{ formatRelativeTime(item.firstSeenAt) }}</span>
          </template>

          <template #item.lastSeenAt="{ item }">
            <span class="text-caption">{{ formatRelativeTime(item.lastSeenAt) }}</span>
          </template>

          <template #item.flags="{ item }">
            <div class="d-flex ga-1">
              <v-icon
                v-if="item.isFlagged"
                icon="mdi-flag"
                color="warning"
                size="small"
              />
              <v-icon
                v-if="item.isBlocked"
                icon="mdi-block-helper"
                color="error"
                size="small"
              />
              <v-icon
                v-if="item.isAdmin"
                icon="mdi-shield-check"
                color="primary"
                size="small"
              />
            </div>
          </template>

          <template #item.actions="{ item }">
            <div class="d-flex justify-end">
              <v-btn
                icon="mdi-eye"
                variant="text"
                size="small"
                @click="openClientDetail(item)"
              >
                <v-icon>mdi-eye</v-icon>
                <v-tooltip activator="parent" location="top">View Details</v-tooltip>
              </v-btn>
              <v-btn
                :icon="item.isFlagged ? 'mdi-flag-off' : 'mdi-flag'"
                variant="text"
                size="small"
                :color="item.isFlagged ? 'warning' : undefined"
                @click="toggleFlag(item)"
              >
                <v-icon>{{ item.isFlagged ? 'mdi-flag-off' : 'mdi-flag' }}</v-icon>
                <v-tooltip activator="parent" location="top">{{ item.isFlagged ? 'Unflag' : 'Flag' }}</v-tooltip>
              </v-btn>
              <v-btn
                icon="mdi-delete"
                variant="text"
                size="small"
                color="error"
                @click="openDeleteDialog(item)"
              >
                <v-icon>mdi-delete</v-icon>
                <v-tooltip activator="parent" location="top">Delete</v-tooltip>
              </v-btn>
            </div>
          </template>

          <template #no-data>
            <div class="d-flex flex-column align-center justify-center pa-12">
              <v-icon size="64" color="grey-darken-2" class="mb-4">mdi-database-off</v-icon>
              <div class="text-h6 text-grey mb-2">No Saved Clients</div>
              <div class="text-body-2 text-grey">Clients will appear here after they connect</div>
            </div>
          </template>
        </v-data-table>
      </v-card>
    </div>

    <!-- Commands Tab -->
    <div v-else-if="activeTab === 'commands'" class="flex-grow-1 d-flex flex-column">
      <v-btn
        prepend-icon="mdi-refresh"
        variant="text"
        size="small"
        :loading="loading"
        class="glass-card mb-4 align-self-start"
        @click="loadCommandLogs"
      >
        Load Command History
      </v-btn>

      <v-card class="glass-panel flex-grow-1 d-flex flex-column rounded-xl border-none" variant="flat">
        <v-data-table
          :headers="commandHeaders"
          :items="commandLogs"
          :items-per-page="50"
          density="comfortable"
          hover
          fixed-header
          class="commands-table flex-grow-1 bg-transparent"
        >
          <template #item.sentAt="{ item }">
            <span class="text-caption font-mono">{{ formatDate(item.sentAt) }}</span>
          </template>

          <template #item.commandType="{ item }">
            <v-chip size="small" variant="flat" color="surface-variant">
              {{ item.commandType }}
            </v-chip>
          </template>

          <template #item.clientId="{ item }">
            <span v-if="item.isBroadcast" class="text-warning">
              <v-icon size="small" class="mr-1">mdi-broadcast</v-icon>
              Broadcast
            </span>
            <span v-else class="text-caption font-mono">{{ item.clientId?.substring(0, 16) || '-' }}...</span>
          </template>

          <template #item.wasDelivered="{ item }">
            <v-icon
              :icon="item.wasDelivered ? 'mdi-check-circle' : 'mdi-close-circle'"
              :color="item.wasDelivered ? 'success' : 'error'"
              size="small"
            />
          </template>

          <template #item.success="{ item }">
            <v-icon
              v-if="item.success !== null"
              :icon="item.success ? 'mdi-check-circle' : 'mdi-close-circle'"
              :color="item.success ? 'success' : 'error'"
              size="small"
            />
            <span v-else class="text-caption text-medium-emphasis">-</span>
          </template>

          <template #no-data>
            <div class="d-flex flex-column align-center justify-center pa-12">
              <v-icon size="64" color="grey-darken-2" class="mb-4">mdi-console</v-icon>
              <div class="text-h6 text-grey mb-2">No Command History</div>
              <div class="text-body-2 text-grey">Commands will appear here after they are executed</div>
            </div>
          </template>
        </v-data-table>
      </v-card>
    </div>

    <!-- Client Detail Dialog -->
    <v-dialog v-model="clientDetailDialog" max-width="700" scrollable>
      <v-card class="glass-panel rounded-xl border-none">
        <v-card-title class="d-flex align-center px-6 py-4">
          <v-icon class="mr-2" color="primary">mdi-account-details</v-icon>
          Client Details
          <v-spacer />
          <v-btn icon="mdi-close" variant="text" size="small" @click="clientDetailDialog = false" />
        </v-card-title>

        <v-divider class="border-opacity-10" />

        <v-card-text class="pa-6">
          <div v-if="clientDetailLoading" class="d-flex justify-center pa-8">
            <v-progress-circular indeterminate color="primary" />
          </div>

          <div v-else-if="selectedClient">
            <!-- Basic Info -->
            <div class="mb-6">
              <div class="text-overline text-medium-emphasis mb-2">System Information</div>
              <v-row dense>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">Machine Name</div>
                  <div class="font-weight-medium">{{ selectedClient.machineName }}</div>
                </v-col>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">Username</div>
                  <div class="font-weight-medium">{{ selectedClient.username }}</div>
                </v-col>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">OS Version</div>
                  <div class="font-weight-medium">{{ selectedClient.osVersion }} {{ selectedClient.osArchitecture }}</div>
                </v-col>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">Last IP</div>
                  <div class="font-weight-medium">{{ selectedClient.lastIpAddress }}</div>
                </v-col>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">Client Version</div>
                  <div class="font-weight-medium">{{ selectedClient.clientVersion || '-' }}</div>
                </v-col>
                <v-col cols="6">
                  <div class="text-caption text-medium-emphasis">Total Connections</div>
                  <div class="font-weight-medium">{{ selectedClient.totalConnections }}</div>
                </v-col>
              </v-row>
            </div>

            <!-- Flags -->
            <div class="mb-6">
              <div class="text-overline text-medium-emphasis mb-2">Flags</div>
              <div class="d-flex ga-2">
                <v-chip
                  :color="selectedClient.isFlagged ? 'warning' : 'default'"
                  variant="tonal"
                  @click="toggleFlag(selectedClient); selectedClient.isFlagged = !selectedClient.isFlagged"
                >
                  <v-icon start size="small">{{ selectedClient.isFlagged ? 'mdi-flag' : 'mdi-flag-outline' }}</v-icon>
                  {{ selectedClient.isFlagged ? 'Flagged' : 'Not Flagged' }}
                </v-chip>
                <v-chip
                  :color="selectedClient.isBlocked ? 'error' : 'default'"
                  variant="tonal"
                  @click="toggleBlock(selectedClient); selectedClient.isBlocked = !selectedClient.isBlocked"
                >
                  <v-icon start size="small">{{ selectedClient.isBlocked ? 'mdi-block-helper' : 'mdi-check-circle-outline' }}</v-icon>
                  {{ selectedClient.isBlocked ? 'Blocked' : 'Not Blocked' }}
                </v-chip>
                <v-chip v-if="selectedClient.isAdmin" color="primary" variant="tonal">
                  <v-icon start size="small">mdi-shield-check</v-icon>
                  Admin
                </v-chip>
              </div>
            </div>

            <!-- Connection History -->
            <div v-if="selectedClient.connections?.length > 0">
              <div class="text-overline text-medium-emphasis mb-2">Recent Connections</div>
              <v-table density="compact" class="bg-transparent">
                <thead>
                  <tr>
                    <th>Connected</th>
                    <th>Disconnected</th>
                    <th>Duration</th>
                    <th>IP</th>
                  </tr>
                </thead>
                <tbody>
                  <tr v-for="conn in selectedClient.connections.slice(0, 10)" :key="conn.id">
                    <td class="text-caption">{{ formatDate(conn.connectedAt) }}</td>
                    <td class="text-caption">{{ conn.disconnectedAt ? formatDate(conn.disconnectedAt) : 'Active' }}</td>
                    <td class="text-caption">{{ conn.durationSeconds ? `${Math.floor(conn.durationSeconds / 60)}m` : '-' }}</td>
                    <td class="text-caption font-mono">{{ conn.ipAddress }}</td>
                  </tr>
                </tbody>
              </v-table>
            </div>
          </div>
        </v-card-text>
      </v-card>
    </v-dialog>

    <!-- Delete Confirmation Dialog -->
    <v-dialog v-model="deleteDialog" max-width="400">
      <v-card class="glass-panel rounded-xl border-none">
        <v-card-title class="d-flex align-center px-6 py-4">
          <v-icon color="error" class="mr-2">mdi-delete-alert</v-icon>
          Delete Client
        </v-card-title>

        <v-card-text class="px-6">
          Are you sure you want to delete <strong class="text-error">{{ clientToDelete?.machineName }}</strong> from the database?
          This will remove all connection history and cannot be undone.
        </v-card-text>

        <v-card-actions class="px-6 pb-6">
          <v-spacer />
          <v-btn variant="text" :disabled="deleteLoading" @click="deleteDialog = false">
            Cancel
          </v-btn>
          <v-btn color="error" variant="elevated" :loading="deleteLoading" @click="confirmDelete">
            Delete
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Snackbar -->
    <v-snackbar v-model="snackbar.show" :color="snackbar.color" :timeout="3000" location="bottom right">
      {{ snackbar.message }}
    </v-snackbar>
  </div>
</template>

<style scoped>
.saved-clients-table :deep(.v-table__wrapper),
.commands-table :deep(.v-table__wrapper) {
  overflow-y: auto;
}

.saved-clients-table :deep(thead tr th),
.commands-table :deep(thead tr th) {
  background: transparent !important;
  color: rgba(255, 255, 255, 0.7) !important;
  font-weight: 600;
  text-transform: uppercase;
  font-size: 0.75rem;
  letter-spacing: 0.05em;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1) !important;
}

.saved-clients-table :deep(tbody tr:hover),
.commands-table :deep(tbody tr:hover) {
  background: rgba(255, 255, 255, 0.03) !important;
}

.country-flag {
  width: 22px;
  height: 16px;
  border-radius: 2px;
  object-fit: cover;
  box-shadow: 0 1px 2px rgba(0, 0, 0, 0.2);
}
</style>
