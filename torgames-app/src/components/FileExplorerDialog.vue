<template>
  <v-dialog v-model="dialog" max-width="1200" scrollable transition="dialog-bottom-transition">
    <v-card class="glass-panel rounded-xl border-none" style="height: 80vh;">
      <!-- Header -->
      <v-card-title class="d-flex align-center px-6 py-4 border-b border-opacity-10">
        <v-avatar color="primary" size="32" class="mr-3">
          <v-icon size="18" color="white">mdi-folder-open</v-icon>
        </v-avatar>
        <span class="text-h6 font-weight-bold">File Explorer</span>
        <v-chip class="ml-3 font-weight-medium" size="small" variant="tonal" color="primary">
          {{ client?.machineName }}
        </v-chip>
        <v-spacer />
        <v-btn icon="mdi-close" variant="text" @click="dialog = false" />
      </v-card-title>

      <!-- Toolbar -->
      <div class="d-flex align-center px-4 py-2 border-b border-opacity-10">
        <v-btn icon="mdi-arrow-left" variant="text" size="small" :disabled="!canGoBack" @click="goBack" />
        <v-btn icon="mdi-arrow-right" variant="text" size="small" :disabled="!canGoForward" @click="goForward" />
        <v-btn icon="mdi-arrow-up" variant="text" size="small" :disabled="!canGoUp" @click="goUp" />
        <v-btn icon="mdi-home" variant="text" size="small" @click="goHome" />
        <v-divider vertical class="mx-2" style="height: 24px;" />

        <!-- Breadcrumb / Path -->
        <div class="flex-grow-1 d-flex align-center">
          <v-breadcrumbs :items="breadcrumbs" class="pa-0">
            <template #item="{ item }">
              <v-breadcrumbs-item
                :title="item.title"
                :disabled="item.disabled"
                @click="navigateTo(item.path)"
                class="cursor-pointer"
              />
            </template>
          </v-breadcrumbs>
        </div>

        <v-btn icon="mdi-refresh" variant="text" size="small" :loading="loading" @click="refresh" />
      </div>

      <!-- Main Content -->
      <v-card-text class="pa-0 d-flex" style="height: calc(100% - 140px);">
        <!-- Tree Panel -->
        <div class="tree-panel border-r border-opacity-10" style="width: 250px; overflow-y: auto;">
          <v-treeview
            v-if="treeItems.length > 0"
            :items="treeItems"
            item-value="fullPath"
            item-title="name"
            :load-children="loadTreeChildren"
            activatable
            density="compact"
            open-on-click
            class="bg-transparent"
            @update:activated="onTreeSelect"
          >
            <template #prepend="{ item, isOpen }">
              <v-icon size="small" :color="item.type === 'drive' ? 'primary' : undefined">
                {{ getIcon(item, isOpen) }}
              </v-icon>
            </template>
          </v-treeview>
          <div v-else-if="loading" class="pa-4 text-center">
            <v-progress-circular indeterminate size="24" color="primary" />
          </div>
        </div>

        <!-- List Panel -->
        <div class="list-panel flex-grow-1 d-flex flex-column" style="overflow: hidden;">
          <!-- Loading State -->
          <div v-if="loading && currentItems.length === 0" class="d-flex align-center justify-center fill-height">
            <v-progress-circular indeterminate size="48" color="primary" />
          </div>

          <!-- Error State -->
          <div v-else-if="error" class="d-flex flex-column align-center justify-center fill-height text-error">
            <v-icon size="48" class="mb-2">mdi-alert-circle</v-icon>
            <div class="text-body-1 mb-2">{{ error }}</div>
            <v-btn color="primary" variant="tonal" @click="refresh">Retry</v-btn>
          </div>

          <!-- File List -->
          <v-table v-else hover density="compact" class="bg-transparent flex-grow-1" fixed-header style="overflow-y: auto;">
            <thead>
              <tr>
                <th class="text-left bg-transparent" style="width: 50%;">Name</th>
                <th class="text-right bg-transparent" style="width: 20%;">Size</th>
                <th class="text-left bg-transparent" style="width: 30%;">Modified</th>
              </tr>
            </thead>
            <tbody>
              <tr
                v-for="item in currentItems"
                :key="item.fullPath"
                class="file-row"
                :class="{ 'selected-row': selectedItem?.fullPath === item.fullPath }"
                @click="selectItem(item)"
                @dblclick="onItemDoubleClick(item)"
                @contextmenu.prevent="showContextMenu($event, item)"
              >
                <td class="d-flex align-center">
                  <v-icon size="small" class="mr-2" :color="item.isDirectory ? 'warning' : undefined">
                    {{ getIcon(item) }}
                  </v-icon>
                  <span :class="{ 'text-medium-emphasis': item.isHidden }">{{ item.name }}</span>
                </td>
                <td class="text-right font-mono text-caption">
                  {{ item.isDirectory ? '' : formatSize(item.sizeBytes) }}
                </td>
                <td class="text-caption text-medium-emphasis">
                  {{ formatDate(item.modifiedTime) }}
                </td>
              </tr>
              <tr v-if="currentItems.length === 0 && !loading">
                <td colspan="3" class="text-center text-medium-emphasis py-8">
                  <v-icon size="48" class="mb-2">mdi-folder-open-outline</v-icon>
                  <div>This folder is empty</div>
                </td>
              </tr>
            </tbody>
          </v-table>
        </div>
      </v-card-text>

      <!-- Status Bar -->
      <div class="px-4 py-2 border-t border-opacity-10 text-caption text-medium-emphasis d-flex align-center">
        <span>{{ currentItems.length }} items</span>
        <v-spacer />
        <span v-if="totalSize > 0">{{ formatSize(totalSize) }}</span>
      </div>
    </v-card>
  </v-dialog>

  <!-- Context Menu -->
  <v-menu
    v-model="contextMenu.show"
    :style="{ position: 'fixed', left: contextMenu.x + 'px', top: contextMenu.y + 'px' }"
    location-strategy="connected"
  >
    <v-list density="compact" class="glass-panel py-2" width="200">
      <v-list-item
        v-if="contextMenu.target?.isDirectory"
        prepend-icon="mdi-folder-open"
        title="Open"
        @click="onItemDoubleClick(contextMenu.target!)"
      />
      <v-divider v-if="contextMenu.target?.isDirectory" class="my-1" />

      <v-list-item prepend-icon="mdi-folder-plus" title="New Folder" @click="createFolder" />
      <v-list-item prepend-icon="mdi-file-plus" title="New File" @click="createFile" />
      <v-divider class="my-1" />

      <v-list-item
        v-if="contextMenu.target"
        prepend-icon="mdi-content-copy"
        title="Copy"
        @click="copyItem"
      />
      <v-list-item
        v-if="contextMenu.target"
        prepend-icon="mdi-content-cut"
        title="Cut"
        @click="cutItem"
      />
      <v-list-item
        prepend-icon="mdi-content-paste"
        title="Paste"
        :disabled="!clipboard"
        @click="pasteItem"
      />
      <v-divider class="my-1" />

      <v-list-item
        v-if="contextMenu.target"
        prepend-icon="mdi-rename"
        title="Rename"
        @click="renameItem"
      />
      <v-list-item
        v-if="contextMenu.target"
        prepend-icon="mdi-delete"
        title="Delete"
        class="text-error"
        @click="deleteItem"
      />
    </v-list>
  </v-menu>

  <!-- Rename Dialog -->
  <v-dialog v-model="renameDialog.show" max-width="400">
    <v-card class="glass-panel rounded-xl">
      <v-card-title class="text-h6">Rename</v-card-title>
      <v-card-text>
        <v-text-field
          v-model="renameDialog.newName"
          label="New name"
          variant="outlined"
          density="compact"
          autofocus
          @keyup.enter="confirmRename"
        />
      </v-card-text>
      <v-card-actions>
        <v-spacer />
        <v-btn variant="text" @click="renameDialog.show = false">Cancel</v-btn>
        <v-btn color="primary" variant="tonal" :loading="renameDialog.loading" @click="confirmRename">Rename</v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>

  <!-- New Folder Dialog -->
  <v-dialog v-model="newFolderDialog.show" max-width="400">
    <v-card class="glass-panel rounded-xl">
      <v-card-title class="text-h6">New Folder</v-card-title>
      <v-card-text>
        <v-text-field
          v-model="newFolderDialog.name"
          label="Folder name"
          variant="outlined"
          density="compact"
          autofocus
          @keyup.enter="confirmNewFolder"
        />
      </v-card-text>
      <v-card-actions>
        <v-spacer />
        <v-btn variant="text" @click="newFolderDialog.show = false">Cancel</v-btn>
        <v-btn color="primary" variant="tonal" :loading="newFolderDialog.loading" @click="confirmNewFolder">Create</v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>

  <!-- New File Dialog -->
  <v-dialog v-model="newFileDialog.show" max-width="400">
    <v-card class="glass-panel rounded-xl">
      <v-card-title class="text-h6">New File</v-card-title>
      <v-card-text>
        <v-text-field
          v-model="newFileDialog.name"
          label="File name"
          variant="outlined"
          density="compact"
          autofocus
          @keyup.enter="confirmNewFile"
        />
      </v-card-text>
      <v-card-actions>
        <v-spacer />
        <v-btn variant="text" @click="newFileDialog.show = false">Cancel</v-btn>
        <v-btn color="primary" variant="tonal" :loading="newFileDialog.loading" @click="confirmNewFile">Create</v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>

  <!-- Delete Confirmation Dialog -->
  <v-dialog v-model="deleteDialog.show" max-width="400">
    <v-card class="glass-panel rounded-xl">
      <v-card-title class="text-h6">Delete</v-card-title>
      <v-card-text>
        Are you sure you want to delete <strong>{{ deleteDialog.itemName }}</strong>?
        <div v-if="deleteDialog.isDirectory" class="text-caption text-warning mt-2">
          This will delete all contents inside the folder.
        </div>
      </v-card-text>
      <v-card-actions>
        <v-spacer />
        <v-btn variant="text" @click="deleteDialog.show = false">Cancel</v-btn>
        <v-btn color="error" variant="tonal" :loading="deleteDialog.loading" @click="confirmDelete">Delete</v-btn>
      </v-card-actions>
    </v-card>
  </v-dialog>

  <!-- Snackbar -->
  <v-snackbar v-model="snackbar.show" :color="snackbar.color" :timeout="3000" location="bottom right">
    {{ snackbar.message }}
  </v-snackbar>
</template>

<script setup lang="ts">
import { ref, computed, watch, reactive } from 'vue'
import { signalRService } from '@/services/signalr'
import type { ClientDto } from '@/types/client'
import type { FileEntry, DirectoryListing } from '@/types/fileExplorer'

interface TreeNode extends FileEntry {
  children?: TreeNode[]
}

interface BreadcrumbItem {
  title: string
  path: string
  disabled: boolean
}

const props = defineProps<{
  modelValue: boolean
  client: ClientDto | null
  connectionKey?: string
}>()

const emit = defineEmits<{
  'update:modelValue': [value: boolean]
}>()

const dialog = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value)
})

// State
const loading = ref(false)
const error = ref<string | null>(null)
const currentPath = ref('')
const currentItems = ref<FileEntry[]>([])
const treeItems = ref<TreeNode[]>([])
const selectedItem = ref<FileEntry | null>(null)
const history = ref<string[]>([])
const historyIndex = ref(-1)
const clipboard = ref<{ item: FileEntry; action: 'copy' | 'cut' } | null>(null)

// Context menu state
const contextMenu = reactive({
  show: false,
  x: 0,
  y: 0,
  target: null as FileEntry | null
})

// Dialog states
const renameDialog = reactive({
  show: false,
  loading: false,
  item: null as FileEntry | null,
  newName: ''
})

const newFolderDialog = reactive({
  show: false,
  loading: false,
  name: ''
})

const newFileDialog = reactive({
  show: false,
  loading: false,
  name: ''
})

const deleteDialog = reactive({
  show: false,
  loading: false,
  item: null as FileEntry | null,
  itemName: '',
  isDirectory: false
})

const snackbar = reactive({
  show: false,
  message: '',
  color: 'success'
})

// Computed
const canGoBack = computed(() => historyIndex.value > 0)
const canGoForward = computed(() => historyIndex.value < history.value.length - 1)
const canGoUp = computed(() => currentPath.value !== '')

const breadcrumbs = computed((): BreadcrumbItem[] => {
  if (!currentPath.value) {
    return [{ title: 'This PC', path: '', disabled: true }]
  }

  const items: BreadcrumbItem[] = [{ title: 'This PC', path: '', disabled: false }]
  const parts = currentPath.value.split('\\').filter(Boolean)
  let path = ''

  parts.forEach((part, index) => {
    path += (index === 0 ? '' : '\\') + part
    if (index === 0) path += '\\'
    items.push({
      title: part,
      path: path,
      disabled: index === parts.length - 1
    })
  })

  return items
})

const totalSize = computed(() => {
  return currentItems.value
    .filter(item => !item.isDirectory)
    .reduce((sum, item) => sum + item.sizeBytes, 0)
})

// Icon mapping
const iconMap: Record<string, string> = {
  '.txt': 'mdi-file-document',
  '.pdf': 'mdi-file-pdf-box',
  '.doc': 'mdi-file-word',
  '.docx': 'mdi-file-word',
  '.xls': 'mdi-file-excel',
  '.xlsx': 'mdi-file-excel',
  '.ppt': 'mdi-file-powerpoint',
  '.pptx': 'mdi-file-powerpoint',
  '.js': 'mdi-language-javascript',
  '.ts': 'mdi-language-typescript',
  '.py': 'mdi-language-python',
  '.cs': 'mdi-language-csharp',
  '.json': 'mdi-code-json',
  '.xml': 'mdi-file-xml-box',
  '.html': 'mdi-language-html5',
  '.css': 'mdi-language-css3',
  '.jpg': 'mdi-file-image',
  '.jpeg': 'mdi-file-image',
  '.png': 'mdi-file-image',
  '.gif': 'mdi-file-image',
  '.svg': 'mdi-file-image',
  '.mp3': 'mdi-file-music',
  '.wav': 'mdi-file-music',
  '.mp4': 'mdi-file-video',
  '.avi': 'mdi-file-video',
  '.mkv': 'mdi-file-video',
  '.zip': 'mdi-folder-zip',
  '.rar': 'mdi-folder-zip',
  '.7z': 'mdi-folder-zip',
  '.exe': 'mdi-application',
  '.dll': 'mdi-cog'
}

function getIcon(item: FileEntry, isOpen = false): string {
  if (item.type === 'drive') return 'mdi-harddisk'
  if (item.isDirectory) return isOpen ? 'mdi-folder-open' : 'mdi-folder'
  return iconMap[item.extension.toLowerCase()] || 'mdi-file'
}

// Formatting
function formatSize(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(1))} ${sizes[i]}`
}

function formatDate(dateStr: string): string {
  if (!dateStr) return ''
  const date = new Date(dateStr)
  return date.toLocaleDateString() + ' ' + date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
}

// Navigation
function addToHistory(path: string) {
  // Remove any forward history
  if (historyIndex.value < history.value.length - 1) {
    history.value = history.value.slice(0, historyIndex.value + 1)
  }
  history.value.push(path)
  historyIndex.value = history.value.length - 1
}

async function navigateTo(path: string) {
  loading.value = true
  error.value = null
  selectedItem.value = null

  try {
    const commandType = path === '' ? 'listdrives' : 'listdir'
    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType,
      commandText: path,
      timeoutSeconds: 30
    })

    if (result?.success && result.stdout) {
      const listing = JSON.parse(result.stdout) as DirectoryListing
      if (listing.success) {
        currentItems.value = listing.entries
        currentPath.value = path
        addToHistory(path)
      } else {
        error.value = listing.errorMessage || 'Failed to list directory'
      }
    } else {
      error.value = result?.errorMessage || 'Failed to retrieve directory listing'
    }
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'An error occurred'
  } finally {
    loading.value = false
  }
}

async function loadTreeChildren(item: TreeNode): Promise<void> {
  try {
    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType: 'listdir',
      commandText: item.fullPath,
      timeoutSeconds: 30
    })

    if (result?.success && result.stdout) {
      const listing = JSON.parse(result.stdout) as DirectoryListing
      if (listing.success) {
        // Only add directories as children for the tree
        item.children = listing.entries
          .filter(e => e.isDirectory)
          .map(e => ({ ...e, children: [] }))
      }
    }
  } catch (e) {
    console.error('Failed to load tree children:', e)
  }
}

function goBack() {
  if (canGoBack.value) {
    historyIndex.value--
    const path = history.value[historyIndex.value] ?? ''
    loadPath(path)
  }
}

function goForward() {
  if (canGoForward.value) {
    historyIndex.value++
    const path = history.value[historyIndex.value] ?? ''
    loadPath(path)
  }
}

function goUp() {
  if (!canGoUp.value) return
  const parts = currentPath.value.split('\\').filter(Boolean)
  if (parts.length === 1) {
    navigateTo('')
  } else {
    parts.pop()
    navigateTo(parts.join('\\') + '\\')
  }
}

function goHome() {
  navigateTo('')
}

async function loadPath(path: string) {
  loading.value = true
  error.value = null

  try {
    const commandType = path === '' ? 'listdrives' : 'listdir'
    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType,
      commandText: path,
      timeoutSeconds: 30
    })

    if (result?.success && result.stdout) {
      const listing = JSON.parse(result.stdout) as DirectoryListing
      if (listing.success) {
        currentItems.value = listing.entries
        currentPath.value = path
      } else {
        error.value = listing.errorMessage || 'Failed to list directory'
      }
    } else {
      error.value = result?.errorMessage || 'Failed to retrieve directory listing'
    }
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'An error occurred'
  } finally {
    loading.value = false
  }
}

function refresh() {
  loadPath(currentPath.value)
}

// Item interaction
function selectItem(item: FileEntry) {
  selectedItem.value = item
}

function onItemDoubleClick(item: FileEntry) {
  if (item.isDirectory || item.type === 'drive') {
    navigateTo(item.fullPath)
  }
}

function onTreeSelect(paths: string[]) {
  if (paths.length > 0 && paths[0]) {
    navigateTo(paths[0])
  }
}

function showContextMenu(event: MouseEvent, item: FileEntry | null) {
  selectedItem.value = item
  contextMenu.target = item
  contextMenu.x = event.clientX
  contextMenu.y = event.clientY
  contextMenu.show = true
}

// File operations
function createFolder() {
  contextMenu.show = false
  newFolderDialog.name = 'New Folder'
  newFolderDialog.show = true
}

async function confirmNewFolder() {
  if (!newFolderDialog.name.trim()) return

  newFolderDialog.loading = true

  try {
    const newPath = currentPath.value
      ? `${currentPath.value}\\${newFolderDialog.name}`
      : `${newFolderDialog.name}`

    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType: 'createfolder',
      commandText: newPath,
      timeoutSeconds: 15
    })

    if (result?.success) {
      showSnackbar('Folder created successfully', 'success')
      refresh()
    } else {
      showSnackbar(result?.errorMessage || 'Failed to create folder', 'error')
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to create folder', 'error')
  } finally {
    newFolderDialog.loading = false
    newFolderDialog.show = false
  }
}

function createFile() {
  contextMenu.show = false
  newFileDialog.name = 'New File.txt'
  newFileDialog.show = true
}

async function confirmNewFile() {
  if (!newFileDialog.name.trim()) return

  newFileDialog.loading = true

  try {
    const newPath = currentPath.value
      ? `${currentPath.value}\\${newFileDialog.name}`
      : `${newFileDialog.name}`

    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType: 'createfile',
      commandText: newPath,
      timeoutSeconds: 15
    })

    if (result?.success) {
      showSnackbar('File created successfully', 'success')
      refresh()
    } else {
      showSnackbar(result?.errorMessage || 'Failed to create file', 'error')
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to create file', 'error')
  } finally {
    newFileDialog.loading = false
    newFileDialog.show = false
  }
}

function copyItem() {
  if (!contextMenu.target) return
  clipboard.value = { item: contextMenu.target, action: 'copy' }
  contextMenu.show = false
  showSnackbar(`"${contextMenu.target.name}" copied to clipboard`, 'info')
}

function cutItem() {
  if (!contextMenu.target) return
  clipboard.value = { item: contextMenu.target, action: 'cut' }
  contextMenu.show = false
  showSnackbar(`"${contextMenu.target.name}" cut to clipboard`, 'info')
}

async function pasteItem() {
  if (!clipboard.value) return
  contextMenu.show = false

  const { item, action } = clipboard.value
  const destPath = currentPath.value
    ? `${currentPath.value}\\${item.name}`
    : item.name

  loading.value = true

  try {
    const commandType = action === 'copy' ? 'copypath' : 'movepath'
    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType,
      commandText: `${item.fullPath}|${destPath}`,
      timeoutSeconds: 60
    })

    if (result?.success) {
      showSnackbar(`"${item.name}" ${action === 'copy' ? 'copied' : 'moved'} successfully`, 'success')
      if (action === 'cut') {
        clipboard.value = null
      }
      refresh()
    } else {
      showSnackbar(result?.errorMessage || `Failed to ${action} item`, 'error')
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : `Failed to ${action} item`, 'error')
  } finally {
    loading.value = false
  }
}

function renameItem() {
  if (!contextMenu.target) return
  contextMenu.show = false
  renameDialog.item = contextMenu.target
  renameDialog.newName = contextMenu.target.name
  renameDialog.show = true
}

async function confirmRename() {
  if (!renameDialog.item || !renameDialog.newName.trim()) return

  renameDialog.loading = true

  try {
    const dir = renameDialog.item.fullPath.substring(0, renameDialog.item.fullPath.lastIndexOf('\\'))
    const newPath = dir ? `${dir}\\${renameDialog.newName}` : renameDialog.newName

    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType: 'renamepath',
      commandText: `${renameDialog.item.fullPath}|${newPath}`,
      timeoutSeconds: 15
    })

    if (result?.success) {
      showSnackbar('Item renamed successfully', 'success')
      refresh()
    } else {
      showSnackbar(result?.errorMessage || 'Failed to rename item', 'error')
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to rename item', 'error')
  } finally {
    renameDialog.loading = false
    renameDialog.show = false
  }
}

function deleteItem() {
  if (!contextMenu.target) return
  contextMenu.show = false
  deleteDialog.item = contextMenu.target
  deleteDialog.itemName = contextMenu.target.name
  deleteDialog.isDirectory = contextMenu.target.isDirectory
  deleteDialog.show = true
}

async function confirmDelete() {
  if (!deleteDialog.item) return

  deleteDialog.loading = true

  try {
    const result = await signalRService.executeCommandWithResult({
      connectionKey: props.connectionKey || props.client?.connectionKey || '',
      commandType: 'deletepath',
      commandText: deleteDialog.item.fullPath,
      timeoutSeconds: 30
    })

    if (result?.success) {
      showSnackbar('Item deleted successfully', 'success')
      refresh()
    } else {
      showSnackbar(result?.errorMessage || 'Failed to delete item', 'error')
    }
  } catch (e) {
    showSnackbar(e instanceof Error ? e.message : 'Failed to delete item', 'error')
  } finally {
    deleteDialog.loading = false
    deleteDialog.show = false
  }
}

function showSnackbar(message: string, color: string) {
  snackbar.message = message
  snackbar.color = color
  snackbar.show = true
}

// Initialize when dialog opens
watch(() => props.modelValue, async (newVal) => {
  if (newVal && props.client) {
    // Reset state
    currentPath.value = ''
    currentItems.value = []
    history.value = []
    historyIndex.value = -1
    selectedItem.value = null
    clipboard.value = null
    error.value = null

    // Load drives first
    await navigateTo('')

    // Build initial tree from drives
    treeItems.value = currentItems.value
      .filter(item => item.type === 'drive')
      .map(item => ({ ...item, children: [] }))
  }
})
</script>

<style scoped>
.file-row {
  cursor: pointer;
  transition: background-color 0.2s;
}

.file-row:hover {
  background-color: rgba(255, 255, 255, 0.05) !important;
}

.selected-row {
  background-color: rgba(var(--v-theme-primary), 0.15) !important;
}

.tree-panel {
  background: rgba(0, 0, 0, 0.1);
}

.cursor-pointer {
  cursor: pointer;
}

/* Custom Scrollbar */
::-webkit-scrollbar {
  width: 6px;
  height: 6px;
}

::-webkit-scrollbar-track {
  background: transparent;
}

::-webkit-scrollbar-thumb {
  background: rgba(255, 255, 255, 0.1);
  border-radius: 3px;
}

::-webkit-scrollbar-thumb:hover {
  background: rgba(255, 255, 255, 0.2);
}
</style>
