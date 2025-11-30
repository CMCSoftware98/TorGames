<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { getVersions, uploadVersion, deleteVersion, getVersionDownloadUrl } from '@/services/api'
import type { VersionInfo } from '@/types/version'
import ServerStatsPanel from '@/components/ServerStatsPanel.vue'

const authStore = useAuthStore()

// Tab state
const activeTab = ref('stats')

const versions = ref<VersionInfo[]>([])
const loading = ref(false)
const error = ref<string | null>(null)

// Upload dialog
const uploadDialog = ref(false)
const uploadLoading = ref(false)
const selectedFile = ref<File | null>(null)
const releaseNotes = ref('')
const isTestVersion = ref(false)
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
  { title: 'Type', key: 'isTestVersion', sortable: true, width: '100px' },
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

function openUploadDialog(testMode: boolean = false) {
  selectedFile.value = null
  releaseNotes.value = ''
  isTestVersion.value = testMode
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

    const result = await uploadVersion(token, selectedFile.value, releaseNotes.value, isTestVersion.value)

    if (result.success) {
      uploadDialog.value = false
      snackbar.value = {
        show: true,
        message: `${isTestVersion.value ? 'Test version' : 'Version'} ${result.version?.version} uploaded successfully`,
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

onMounted(() => {
  loadVersions()
})
</script>

<template>
  <div class="fill-height d-flex flex-column">
    <!-- Toolbar -->
    <div class="d-flex align-center mb-4">
      <div>
        <div class="text-h5 font-weight-light">Server Management</div>
        <div class="text-caption text-medium-emphasis">Monitor server resources and manage client versions</div>
      </div>
      <v-spacer />
      <v-btn
        v-if="activeTab === 'versions'"
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
        v-if="activeTab === 'versions'"
        prepend-icon="mdi-flask"
        color="info"
        variant="tonal"
        @click="openUploadDialog(true)"
        class="font-weight-bold mr-2"
      >
        Upload Test
      </v-btn>
      <v-btn
        v-if="activeTab === 'versions'"
        prepend-icon="mdi-upload"
        color="primary"
        @click="openUploadDialog(false)"
        class="font-weight-bold"
      >
        Upload Production
      </v-btn>
    </div>

    <!-- Tabs -->
    <v-tabs v-model="activeTab" class="mb-4" color="primary" density="compact">
      <v-tab value="stats" class="text-none">
        <v-icon start size="small">mdi-chart-line</v-icon>
        Server Statistics
      </v-tab>
      <v-tab value="versions" class="text-none">
        <v-icon start size="small">mdi-package-variant</v-icon>
        Version Management
      </v-tab>
    </v-tabs>

    <!-- Server Statistics Tab -->
    <div v-if="activeTab === 'stats'" class="flex-grow-1" style="overflow-y: auto;">
      <ServerStatsPanel />
    </div>

    <!-- Version Management Tab -->
    <div v-else-if="activeTab === 'versions'" class="flex-grow-1 d-flex flex-column">
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

        <template #item.isTestVersion="{ item }">
          <v-chip
            :color="item.isTestVersion ? 'info' : 'success'"
            variant="tonal"
            size="small"
          >
            <v-icon start size="small">{{ item.isTestVersion ? 'mdi-flask' : 'mdi-rocket-launch' }}</v-icon>
            {{ item.isTestVersion ? 'Test' : 'Prod' }}
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
    </div>

    <!-- Upload Dialog -->
    <v-dialog v-model="uploadDialog" max-width="500" persistent>
      <v-card class="glass-panel rounded-xl border-none">
        <v-card-title class="d-flex align-center px-6 py-4 border-b border-opacity-10">
          <v-icon class="mr-2" :color="isTestVersion ? 'info' : 'primary'">{{ isTestVersion ? 'mdi-flask' : 'mdi-upload' }}</v-icon>
          Upload {{ isTestVersion ? 'Test' : 'Production' }} Version
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

          <v-alert v-if="isTestVersion" type="warning" density="compact" variant="tonal" class="mt-4 rounded-lg border-none bg-opacity-10">
            <template #prepend>
              <v-icon color="warning" size="small">mdi-flask</v-icon>
            </template>
            <div class="text-caption">
              <strong>Test Version:</strong> This version will only be deployed to clients marked as "Test Mode"
              in the Saved Data panel. Other clients will continue using the production version.
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
