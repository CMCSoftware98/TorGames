<script setup lang="ts">
import { ref, onMounted } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { getVersions, uploadVersion, deleteVersion, getVersionDownloadUrl } from '@/services/api'
import type { VersionInfo } from '@/types/version'

const authStore = useAuthStore()

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
  { title: 'Actions', key: 'actions', sortable: false, width: '150px' },
]

function formatBytes(bytes: number): string {
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
    const token = authStore.token
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
  if (input.files && input.files.length > 0) {
    selectedFile.value = input.files[0]
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
    const token = authStore.token
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
    const token = authStore.token
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
  <div class="version-management fill-height d-flex flex-column">
    <!-- Toolbar -->
    <v-toolbar density="compact" color="transparent" class="px-2">
      <v-toolbar-title class="text-subtitle-1">
        <v-icon class="mr-2">mdi-package-variant</v-icon>
        Client Versions
      </v-toolbar-title>
      <v-spacer />
      <v-btn
        prepend-icon="mdi-refresh"
        variant="outlined"
        size="small"
        :loading="loading"
        class="mr-2"
        @click="loadVersions"
      >
        Refresh
      </v-btn>
      <v-btn
        prepend-icon="mdi-upload"
        color="primary"
        size="small"
        @click="openUploadDialog"
      >
        Upload Version
      </v-btn>
    </v-toolbar>

    <v-divider />

    <!-- Error Alert -->
    <v-alert
      v-if="error"
      type="error"
      closable
      density="compact"
      class="mx-4 mt-2"
      @click:close="error = null"
    >
      {{ error }}
    </v-alert>

    <!-- Loading State -->
    <div v-if="loading && versions.length === 0" class="d-flex justify-center align-center pa-8">
      <v-progress-circular indeterminate color="primary" />
    </div>

    <!-- Empty State -->
    <div v-else-if="!loading && versions.length === 0" class="d-flex flex-column align-center justify-center pa-8 flex-grow-1">
      <v-icon size="64" color="grey" class="mb-4">mdi-package-variant-closed</v-icon>
      <div class="text-h6 text-grey mb-2">No Versions Available</div>
      <div class="text-body-2 text-grey mb-4">Upload your first client version to get started</div>
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
      density="compact"
      hover
      fixed-header
      hide-default-footer
      class="versions-table flex-grow-1"
    >
      <template #item.fileSize="{ item }">
        {{ formatBytes(item.fileSize) }}
      </template>

      <template #item.uploadedAt="{ item }">
        {{ formatDate(item.uploadedAt) }}
      </template>

      <template #item.releaseNotes="{ item }">
        <span class="text-truncate" style="max-width: 300px; display: inline-block;">
          {{ item.releaseNotes || '-' }}
        </span>
      </template>

      <template #item.actions="{ item }">
        <v-btn
          icon="mdi-download"
          variant="text"
          size="small"
          color="primary"
          @click="downloadVersion(item)"
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
      </template>
    </v-data-table>

    <!-- Upload Dialog -->
    <v-dialog v-model="uploadDialog" max-width="500" persistent>
      <v-card>
        <v-card-title class="d-flex align-center">
          <v-icon class="mr-2">mdi-upload</v-icon>
          Upload New Version
        </v-card-title>

        <v-card-text>
          <v-alert
            v-if="uploadError"
            type="error"
            density="compact"
            class="mb-4"
            closable
            @click:close="uploadError = null"
          >
            {{ uploadError }}
          </v-alert>

          <v-file-input
            label="Client Executable"
            accept=".exe"
            prepend-icon="mdi-file-upload"
            variant="outlined"
            density="compact"
            :disabled="uploadLoading"
            @change="onFileSelected"
          />

          <v-textarea
            v-model="releaseNotes"
            label="Release Notes (optional)"
            placeholder="What's new in this version..."
            variant="outlined"
            density="compact"
            rows="3"
            :disabled="uploadLoading"
          />

          <v-alert type="info" density="compact" variant="tonal" class="mt-2">
            <template #text>
              The version number will be automatically extracted from the executable's metadata.
              Make sure the file was built with a valid version (e.g., 2025.11.28.1).
            </template>
          </v-alert>
        </v-card-text>

        <v-card-actions>
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
          >
            Upload
          </v-btn>
        </v-card-actions>
      </v-card>
    </v-dialog>

    <!-- Delete Confirmation Dialog -->
    <v-dialog v-model="deleteDialog" max-width="400">
      <v-card>
        <v-card-title class="d-flex align-center">
          <v-icon color="error" class="mr-2">mdi-delete-alert</v-icon>
          Delete Version
        </v-card-title>

        <v-card-text>
          Are you sure you want to delete version <strong>{{ versionToDelete?.version }}</strong>?
          This action cannot be undone.
        </v-card-text>

        <v-card-actions>
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
    <v-snackbar v-model="snackbar.show" :color="snackbar.color" :timeout="3000">
      {{ snackbar.message }}
    </v-snackbar>
  </div>
</template>

<style scoped>
.versions-table {
  display: flex !important;
  flex-direction: column !important;
  overflow: hidden !important;
}

.versions-table :deep(.v-table__wrapper) {
  flex: 1 1 0 !important;
  min-height: 0 !important;
  overflow-y: auto !important;
}
</style>
