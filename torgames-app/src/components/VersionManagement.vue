<script setup lang="ts">
import { ref, onMounted, onUnmounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { getVersions, uploadVersion, deleteVersion, getVersionDownloadUrl, getServerStats, type ServerStats } from '@/services/api'
import type { VersionInfo } from '@/types/version'

const authStore = useAuthStore()

// Server stats
const serverStats = ref<ServerStats | null>(null)
const statsLoading = ref(false)
let statsInterval: ReturnType<typeof setInterval> | null = null

const versions = ref<VersionInfo[]>([])
const loading = ref(false)
const error = ref<string | null>(null)

// Upload dialog
const uploadDialog = ref(false)
const uploadLoading = ref(false)
const selectedFile = ref<File | null>(null)
const releaseNotes = ref('')
const uploadError = ref<string | null>(null)

// Delete dialog
const deleteDialog = ref(false)
const deleteLoading = ref(false)
const versionToDelete = ref<VersionInfo | null>(null)

// Snackbar
const snackbar = ref({
  show: false,
  message: '',
  color: 'success'
})

const headers = [
  { title: 'Version', key: 'version', sortable: true },
  { title: 'Size', key: 'fileSize', sortable: true },
  { title: 'Uploaded', key: 'uploadedAt', sortable: true },
  { title: 'Release Notes', key: 'releaseNotes', sortable: false },
  { title: 'Actions', key: 'actions', sortable: false, width: '120px', align: 'end' },
]

function formatBytesShort(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function formatDate(dateString: string): string {
  const date = new Date(dateString)
  return date.toLocaleDateString() + ' ' + date.toLocaleTimeString()
}

async function loadVersions() {
  loading.value = true
  error.value = null

  try {
    const token = authStore.sessionToken
    if (!token) {
      error.value = 'Not authenticated'
      return
    }

    versions.value = await getVersions(token)
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'Failed to load versions'
  } finally {
    loading.value = false
  }
}

function openUploadDialog() {
  selectedFile.value = null
  releaseNotes.value = ''
  uploadError.value = null
  uploadDialog.value = true
}

function onFileSelected(event: Event) {
  const input = event.target as HTMLInputElement
  const file = input.files?.[0]
  if (file) {
    selectedFile.value = file
  }
}

async function handleUpload() {
  if (!selectedFile.value) {
    uploadError.value = 'Please select a file'
    return
  }

  uploadLoading.value = true
  uploadError.value = null

  try {
    const token = authStore.sessionToken
    if (!token) {
      uploadError.value = 'Not authenticated'
      return
    }

    const result = await uploadVersion(token, selectedFile.value, releaseNotes.value)

    if (result.success) {
      uploadDialog.value = false
      snackbar.value = {
        show: true,
        message: `Version ${result.version?.version} uploaded successfully`,
        color: 'success'
      }
      await loadVersions()
    } else {
      uploadError.value = result.error || 'Upload failed'
    }
  } catch (e) {
    uploadError.value = e instanceof Error ? e.message : 'Upload failed'
  } finally {
    uploadLoading.value = false
  }
}

function openDeleteDialog(version: VersionInfo) {
  versionToDelete.value = version
  deleteDialog.value = true
}

async function handleDelete() {
  if (!versionToDelete.value) return

  deleteLoading.value = true

  try {
    const token = authStore.sessionToken
    if (!token) {
      snackbar.value = {
        show: true,
        message: 'Not authenticated',
        color: 'error'
      }
      return
    }

    const result = await deleteVersion(token, versionToDelete.value.version)

    if (result.success) {
      deleteDialog.value = false
      snackbar.value = {
        show: true,
        message: `Version ${versionToDelete.value.version} deleted`,
        color: 'success'
      }
      await loadVersions()
    } else {
      snackbar.value = {
        show: true,
        message: result.error || 'Delete failed',
        color: 'error'
      }
    }
  } catch (e) {
    snackbar.value = {
      show: true,
      message: e instanceof Error ? e.message : 'Delete failed',
      color: 'error'
    }
  } finally {
    deleteLoading.value = false
    versionToDelete.value = null
  }
}

function downloadVersion(version: VersionInfo) {
  const url = getVersionDownloadUrl(version.version)
  window.open(url, '_blank')
}

async function loadServerStats() {
  const token = authStore.sessionToken
  if (!token) return

  statsLoading.value = true
  try {
    serverStats.value = await getServerStats(token)
  } catch (e) {
    console.error('Failed to load server stats:', e)
  } finally {
    statsLoading.value = false
  }
}

function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function formatUptime(seconds: number): string {
  const days = Math.floor(seconds / 86400)
  const hours = Math.floor((seconds % 86400) / 3600)
  const minutes = Math.floor((seconds % 3600) / 60)

  const parts = []
  if (days > 0) parts.push(`${days}d`)
  if (hours > 0) parts.push(`${hours}h`)
  if (minutes > 0) parts.push(`${minutes}m`)

  return parts.join(' ') || '< 1m'
}

function getUsageColor(percent: number): string {
  if (percent >= 90) return 'error'
  if (percent >= 70) return 'warning'
  return 'success'
}

function getUsageColorText(percent: number): string {
  if (percent >= 90) return 'text-error'
  if (percent >= 70) return 'text-warning'
  return 'text-success'
}

onMounted(() => {
  loadVersions()
  loadServerStats()
  // Refresh stats every 10 seconds
  statsInterval = setInterval(loadServerStats, 10000)
})

onUnmounted(() => {
  if (statsInterval) {
    clearInterval(statsInterval)
    statsInterval = null
  }
})
</script>

<template>
  <div class="fill-height d-flex flex-column">
    <!-- Toolbar -->
    <div class="d-flex align-center mb-6">
      <div>
        <div class="text-h5 font-weight-light">Server Management</div>
        <div class="text-caption text-medium-emphasis">Manage client versions and updates</div>
      </div>
      <v-spacer />
      <v-btn
        prepend-icon="mdi-refresh"
        variant="text"
        size="small"
        :loading="loading"
        class="glass-card mr-2"
        @click="loadVersions"
      >
        Refresh
      </v-btn>
      <v-btn
        prepend-icon="mdi-upload"
        color="primary"
        @click="openUploadDialog"
        class="font-weight-bold"
      >
        Upload Version
      </v-btn>
    </div>

    <!-- Server Resources -->
    <v-row class="mb-4">
      <v-col cols="12" sm="6" md="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="primary" class="mr-2">mdi-chip</v-icon>
            <span class="text-caption text-medium-emphasis">CPU USAGE</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h5 font-weight-bold" :class="serverStats ? getUsageColorText(serverStats.cpuUsagePercent) : ''">
              {{ serverStats?.cpuUsagePercent?.toFixed(1) ?? '-' }}%
            </div>
          </div>
          <v-progress-linear
            :model-value="serverStats?.cpuUsagePercent ?? 0"
            :color="getUsageColor(serverStats?.cpuUsagePercent ?? 0)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" md="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="secondary" class="mr-2">mdi-memory</v-icon>
            <span class="text-caption text-medium-emphasis">MEMORY</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h5 font-weight-bold" :class="serverStats ? getUsageColorText((serverStats.memoryUsedBytes / serverStats.memoryTotalBytes) * 100) : ''">
              {{ serverStats ? Math.round((serverStats.memoryUsedBytes / serverStats.memoryTotalBytes) * 100) : '-' }}%
            </div>
            <div class="text-caption text-medium-emphasis">
              {{ serverStats ? formatBytes(serverStats.memoryUsedBytes) : '-' }} / {{ serverStats ? formatBytes(serverStats.memoryTotalBytes) : '-' }}
            </div>
          </div>
          <v-progress-linear
            :model-value="serverStats ? (serverStats.memoryUsedBytes / serverStats.memoryTotalBytes) * 100 : 0"
            :color="getUsageColor(serverStats ? (serverStats.memoryUsedBytes / serverStats.memoryTotalBytes) * 100 : 0)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" md="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="warning" class="mr-2">mdi-harddisk</v-icon>
            <span class="text-caption text-medium-emphasis">DISK SPACE</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h5 font-weight-bold" :class="serverStats ? getUsageColorText((serverStats.diskUsedBytes / serverStats.diskTotalBytes) * 100) : ''">
              {{ serverStats ? Math.round((serverStats.diskUsedBytes / serverStats.diskTotalBytes) * 100) : '-' }}%
            </div>
            <div class="text-caption text-medium-emphasis">
              {{ serverStats ? formatBytes(serverStats.diskUsedBytes) : '-' }} / {{ serverStats ? formatBytes(serverStats.diskTotalBytes) : '-' }}
            </div>
          </div>
          <v-progress-linear
            :model-value="serverStats ? (serverStats.diskUsedBytes / serverStats.diskTotalBytes) * 100 : 0"
            :color="getUsageColor(serverStats ? (serverStats.diskUsedBytes / serverStats.diskTotalBytes) * 100 : 0)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" md="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="success" class="mr-2">mdi-account-group</v-icon>
            <span class="text-caption text-medium-emphasis">CLIENTS</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h5 font-weight-bold text-success">
              {{ serverStats?.connectedClients ?? '-' }}
            </div>
            <div class="text-caption text-medium-emphasis">
              Online
            </div>
          </div>
          <div class="d-flex align-center justify-space-between mt-2">
            <div class="text-caption text-medium-emphasis">
              <v-icon size="x-small" class="mr-1">mdi-clock-outline</v-icon>
              Uptime: {{ serverStats ? formatUptime(serverStats.uptimeSeconds) : '-' }}
            </div>
          </div>
        </v-card>
      </v-col>
    </v-row>

    <!-- Error Alert -->
    <v-alert
      v-if="error"
      type="error"
      closable
      variant="tonal"
      class="mb-4 rounded-lg"
      @click:close="error = null"
    >
      {{ error }}
    </v-alert>

    <!-- Content -->
    <v-card class="glass-panel flex-grow-1 d-flex flex-column rounded-xl border-none" variant="flat">
      <!-- Loading State -->
      <div v-if="loading && versions.length === 0" class="d-flex justify-center align-center fill-height">
        <v-progress-circular indeterminate color="primary" size="64" width="6" />
      </div>

      <!-- Empty State -->
      <div v-else-if="!loading && versions.length === 0" class="d-flex flex-column align-center justify-center fill-height">
        <v-icon size="64" color="surface-variant" class="mb-4">mdi-package-variant-closed</v-icon>
        <div class="text-h6 text-medium-emphasis mb-2">No Versions Available</div>
        <div class="text-body-2 text-disabled mb-6">Upload your first client version to get started</div>
        <v-btn color="primary" prepend-icon="mdi-upload" @click="openUploadDialog">
          Upload Version
        </v-btn>
      </div>

      <!-- Versions Table -->
      <v-data-table
        v-else
        :headers="headers"
        :items="versions"
        :items-per-page="-1"
        density="comfortable"
        hover
        fixed-header
        hide-default-footer
        class="versions-table flex-grow-1 bg-transparent"
      >
        <template #item.version="{ item }">
          <v-chip color="primary" variant="tonal" size="small" class="font-weight-bold">
            {{ item.version }}
          </v-chip>
        </template>

        <template #item.fileSize="{ item }">
          <span class="font-mono text-caption">{{ formatBytesShort(item.fileSize) }}</span>
        </template>

        <template #item.uploadedAt="{ item }">
          <span class="text-caption text-medium-emphasis">{{ formatDate(item.uploadedAt) }}</span>
        </template>

        <template #item.releaseNotes="{ item }">
          <span class="text-truncate d-inline-block text-medium-emphasis" style="max-width: 300px;">
            {{ item.releaseNotes || '-' }}
          </span>
        </template>

        <template #item.actions="{ item }">
          <div class="d-flex justify-end">
            <v-btn
              icon="mdi-download"
              variant="text"
              size="small"
              color="primary"
              @click="downloadVersion(item)"
              class="mr-1"
            >
              <v-icon>mdi-download</v-icon>
              <v-tooltip activator="parent" location="top">Download</v-tooltip>
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
      </v-data-table>
    </v-card>

    <!-- Upload Dialog -->
    <v-dialog v-model="uploadDialog" max-width="500" persistent>
      <v-card class="glass-panel rounded-xl border-none">
        <v-card-title class="d-flex align-center px-6 py-4 border-b border-opacity-10">
          <v-icon class="mr-2" color="primary">mdi-upload</v-icon>
          Upload New Version
        </v-card-title>

        <v-card-text class="pa-6">
          <v-alert
            v-if="uploadError"
            type="error"
            variant="tonal"
            density="compact"
            class="mb-4 rounded-lg"
            closable
            @click:close="uploadError = null"
          >
            {{ uploadError }}
          </v-alert>

          <v-file-input
            label="Client Executable"
            accept=".exe"
            prepend-icon=""
            prepend-inner-icon="mdi-file-upload"
            variant="outlined"
            density="comfortable"
            :disabled="uploadLoading"
            @change="onFileSelected"
            class="mb-2"
            rounded="lg"
            bg-color="rgba(255,255,255,0.05)"
          />

          <v-textarea
            v-model="releaseNotes"
            label="Release Notes (optional)"
            placeholder="What's new in this version..."
            variant="outlined"
            density="comfortable"
            rows="3"
            :disabled="uploadLoading"
            rounded="lg"
            bg-color="rgba(255,255,255,0.05)"
          />

          <v-alert type="info" density="compact" variant="tonal" class="mt-4 rounded-lg border-none bg-opacity-10">
            <template #prepend>
              <v-icon color="info" size="small">mdi-information</v-icon>
            </template>
            <div class="text-caption">
              Version is automatically extracted from the executable's assembly metadata.
              Build the client with "dotnet publish" to embed the version.
            </div>
          </v-alert>
        </v-card-text>

        <v-card-actions class="px-6 pb-6 pt-0">
          <v-spacer />
          <v-btn
            variant="text"
            :disabled="uploadLoading"
            @click="uploadDialog = false"
          >
            Cancel
          </v-btn>
          <v-btn
            color="primary"
            variant="elevated"
            :loading="uploadLoading"
            :disabled="!selectedFile"
            @click="handleUpload"
            class="px-6"
          >
            Upload
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Delete Confirmation Dialog -->
    <v-dialog v-model="deleteDialog" max-width="400">
      <v-card class="glass-panel rounded-xl border-none">
        <v-card-title class="d-flex align-center px-6 py-4">
          <v-icon color="error" class="mr-2">mdi-delete-alert</v-icon>
          Delete Version
        </v-card-title>

        <v-card-text class="px-6">
          Are you sure you want to delete version <strong class="text-error">{{ versionToDelete?.version }}</strong>?
          This action cannot be undone.
        </v-card-text>

        <v-card-actions class="px-6 pb-6">
          <v-spacer />
          <v-btn
            variant="text"
            :disabled="deleteLoading"
            @click="deleteDialog = false"
          >
            Cancel
          </v-btn>
          <v-btn
            color="error"
            variant="elevated"
            :loading="deleteLoading"
            @click="handleDelete"
          >
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
.versions-table :deep(.v-table__wrapper) {
  overflow-y: auto;
}

.versions-table :deep(thead tr th) {
  background: transparent !important;
  color: rgba(255, 255, 255, 0.7) !important;
  font-weight: 600;
  text-transform: uppercase;
  font-size: 0.75rem;
  letter-spacing: 0.05em;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1) !important;
}

.versions-table :deep(tbody tr:hover) {
  background: rgba(255, 255, 255, 0.03) !important;
}
</style>
