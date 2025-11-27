<template>
  <v-dialog v-model="dialog" max-width="900" scrollable>
    <v-card>
      <v-card-title class="d-flex align-center">
        <v-icon class="mr-2">mdi-monitor</v-icon>
        Client Manager - {{ client?.machineName }}
        <v-spacer />
        <v-btn icon="mdi-refresh" variant="text" :loading="loading" @click="refresh" />
        <v-btn icon="mdi-close" variant="text" @click="dialog = false" />
      </v-card-title>

      <v-card-text v-if="loading && !systemInfo" class="text-center py-8">
        <v-progress-circular indeterminate color="primary" size="64" />
        <div class="mt-4">Fetching system information...</div>
      </v-card-text>

      <v-card-text v-else-if="error" class="text-center py-8">
        <v-icon color="error" size="64">mdi-alert-circle</v-icon>
        <div class="mt-4 text-error">{{ error }}</div>
        <v-btn color="primary" class="mt-4" @click="refresh">Retry</v-btn>
      </v-card-text>

      <v-card-text v-else-if="systemInfo" class="pa-0">
        <v-tabs v-model="tab" bg-color="grey-darken-4">
          <v-tab value="overview">Overview</v-tab>
          <v-tab value="cpu">CPU</v-tab>
          <v-tab value="memory">Memory</v-tab>
          <v-tab value="storage">Storage</v-tab>
          <v-tab value="network">Network</v-tab>
          <v-tab value="gpu">GPU</v-tab>
          <v-tab value="processes">Processes</v-tab>
        </v-tabs>

        <v-tabs-window v-model="tab" class="pa-4">
          <!-- Overview Tab -->
          <v-tabs-window-item value="overview">
            <v-alert v-if="responseTimeMs !== null" type="info" variant="tonal" density="compact" class="mb-4">
              <v-icon class="mr-2">mdi-timer-outline</v-icon>
              Response Time: <strong>{{ responseTimeMs }}ms</strong>
            </v-alert>
            <v-row>
              <v-col cols="12" md="6">
                <v-card variant="outlined" class="mb-4">
                  <v-card-title class="text-subtitle-1">
                    <v-icon class="mr-2">mdi-desktop-tower</v-icon>
                    System
                  </v-card-title>
                  <v-card-text>
                    <div class="info-row"><span>Manufacturer</span><span>{{ systemInfo.hardware?.manufacturer || '-' }}</span></div>
                    <div class="info-row"><span>Model</span><span>{{ systemInfo.hardware?.model || '-' }}</span></div>
                    <div class="info-row"><span>System Type</span><span>{{ systemInfo.hardware?.systemType || '-' }}</span></div>
                    <div class="info-row"><span>UUID</span><span>{{ systemInfo.hardware?.uuid || '-' }}</span></div>
                  </v-card-text>
                </v-card>

                <v-card variant="outlined">
                  <v-card-title class="text-subtitle-1">
                    <v-icon class="mr-2">mdi-microsoft-windows</v-icon>
                    Operating System
                  </v-card-title>
                  <v-card-text>
                    <div class="info-row"><span>Name</span><span>{{ systemInfo.os?.name || '-' }}</span></div>
                    <div class="info-row"><span>Version</span><span>{{ systemInfo.os?.version || '-' }}</span></div>
                    <div class="info-row"><span>Build</span><span>{{ systemInfo.os?.buildNumber || '-' }}</span></div>
                    <div class="info-row"><span>Architecture</span><span>{{ systemInfo.os?.architecture || '-' }}</span></div>
                    <div class="info-row"><span>Install Date</span><span>{{ systemInfo.os?.installDate || '-' }}</span></div>
                    <div class="info-row"><span>Last Boot</span><span>{{ systemInfo.os?.lastBootTime || '-' }}</span></div>
                    <div class="info-row"><span>User</span><span>{{ systemInfo.os?.registeredUser || '-' }}</span></div>
                    <div class="info-row"><span>Timezone</span><span>{{ systemInfo.os?.timezoneName || '-' }}</span></div>
                  </v-card-text>
                </v-card>
              </v-col>

              <v-col cols="12" md="6">
                <v-card variant="outlined" class="mb-4">
                  <v-card-title class="text-subtitle-1">
                    <v-icon class="mr-2">mdi-speedometer</v-icon>
                    Performance
                  </v-card-title>
                  <v-card-text>
                    <div class="mb-3">
                      <div class="d-flex justify-space-between mb-1">
                        <span>CPU Usage</span>
                        <span>{{ systemInfo.performance?.cpuUsagePercent?.toFixed(1) }}%</span>
                      </div>
                      <v-progress-linear
                        :model-value="systemInfo.performance?.cpuUsagePercent || 0"
                        :color="getUsageColor(systemInfo.performance?.cpuUsagePercent || 0)"
                        height="8"
                        rounded
                      />
                    </div>
                    <div class="mb-3">
                      <div class="d-flex justify-space-between mb-1">
                        <span>Memory Usage</span>
                        <span>{{ systemInfo.memory?.memoryLoadPercent }}%</span>
                      </div>
                      <v-progress-linear
                        :model-value="systemInfo.memory?.memoryLoadPercent || 0"
                        :color="getUsageColor(systemInfo.memory?.memoryLoadPercent || 0)"
                        height="8"
                        rounded
                      />
                    </div>
                    <div class="info-row"><span>Processes</span><span>{{ systemInfo.performance?.processCount || '-' }}</span></div>
                    <div class="info-row"><span>Threads</span><span>{{ systemInfo.performance?.threadCount || '-' }}</span></div>
                    <div class="info-row"><span>Handles</span><span>{{ systemInfo.performance?.handleCount || '-' }}</span></div>
                    <div class="info-row"><span>Uptime</span><span>{{ formatUptime(systemInfo.performance?.uptimeSeconds || 0) }}</span></div>
                  </v-card-text>
                </v-card>

                <v-card variant="outlined" class="mb-4">
                  <v-card-title class="text-subtitle-1">
                    <v-icon class="mr-2">mdi-chip</v-icon>
                    BIOS
                  </v-card-title>
                  <v-card-text>
                    <div class="info-row"><span>Manufacturer</span><span>{{ systemInfo.hardware?.biosManufacturer || '-' }}</span></div>
                    <div class="info-row"><span>Version</span><span>{{ systemInfo.hardware?.biosVersion || '-' }}</span></div>
                    <div class="info-row"><span>Release Date</span><span>{{ systemInfo.hardware?.biosReleaseDate || '-' }}</span></div>
                  </v-card-text>
                </v-card>

                <v-card variant="outlined">
                  <v-card-title class="text-subtitle-1">
                    <v-icon class="mr-2">mdi-lightning-bolt</v-icon>
                    Quick Actions
                  </v-card-title>
                  <v-card-text class="d-flex flex-column ga-2">
                    <v-btn
                      color="warning"
                      variant="tonal"
                      prepend-icon="mdi-shield-off"
                      :loading="disablingUac"
                      block
                      @click="disableUac"
                    >
                      Disable UAC
                    </v-btn>
                    <v-btn
                      color="error"
                      variant="tonal"
                      prepend-icon="mdi-power"
                      :loading="powerActionLoading === 'shutdown'"
                      block
                      @click="sendPowerCommand('shutdown', 'Shut Down')"
                    >
                      Shut Down
                    </v-btn>
                    <v-btn
                      color="warning"
                      variant="tonal"
                      prepend-icon="mdi-restart"
                      :loading="powerActionLoading === 'restart'"
                      block
                      @click="sendPowerCommand('restart', 'Restart')"
                    >
                      Restart
                    </v-btn>
                    <v-btn
                      color="info"
                      variant="tonal"
                      prepend-icon="mdi-logout"
                      :loading="powerActionLoading === 'signout'"
                      block
                      @click="sendPowerCommand('signout', 'Sign Out')"
                    >
                      Sign Out
                    </v-btn>
                  </v-card-text>
                </v-card>
              </v-col>
            </v-row>
          </v-tabs-window-item>

          <!-- CPU Tab -->
          <v-tabs-window-item value="cpu">
            <v-card variant="outlined">
              <v-card-title class="text-subtitle-1">
                <v-icon class="mr-2">mdi-chip</v-icon>
                Processor
              </v-card-title>
              <v-card-text>
                <div class="info-row"><span>Name</span><span>{{ systemInfo.cpu?.name || '-' }}</span></div>
                <div class="info-row"><span>Manufacturer</span><span>{{ systemInfo.cpu?.manufacturer || '-' }}</span></div>
                <div class="info-row"><span>Architecture</span><span>{{ systemInfo.cpu?.architecture || '-' }}</span></div>
                <div class="info-row"><span>Cores</span><span>{{ systemInfo.cpu?.cores || '-' }}</span></div>
                <div class="info-row"><span>Logical Processors</span><span>{{ systemInfo.cpu?.logicalProcessors || '-' }}</span></div>
                <div class="info-row"><span>Max Clock Speed</span><span>{{ systemInfo.cpu?.maxClockSpeedMhz ? `${systemInfo.cpu.maxClockSpeedMhz} MHz` : '-' }}</span></div>
                <div class="info-row"><span>Current Clock Speed</span><span>{{ systemInfo.cpu?.currentClockSpeedMhz ? `${systemInfo.cpu.currentClockSpeedMhz} MHz` : '-' }}</span></div>
                <div class="info-row"><span>L2 Cache</span><span>{{ systemInfo.cpu?.l2CacheKb ? `${systemInfo.cpu.l2CacheKb} KB` : '-' }}</span></div>
                <div class="info-row"><span>L3 Cache</span><span>{{ systemInfo.cpu?.l3CacheKb ? `${(systemInfo.cpu.l3CacheKb / 1024).toFixed(1)} MB` : '-' }}</span></div>
              </v-card-text>
            </v-card>
          </v-tabs-window-item>

          <!-- Memory Tab -->
          <v-tabs-window-item value="memory">
            <v-card variant="outlined" class="mb-4">
              <v-card-title class="text-subtitle-1">
                <v-icon class="mr-2">mdi-memory</v-icon>
                Physical Memory
              </v-card-title>
              <v-card-text>
                <div class="mb-4">
                  <div class="d-flex justify-space-between mb-1">
                    <span>Usage: {{ formatBytes(usedMemory) }} / {{ formatBytes(systemInfo.memory?.totalPhysicalBytes || 0) }}</span>
                    <span>{{ systemInfo.memory?.memoryLoadPercent }}%</span>
                  </div>
                  <v-progress-linear
                    :model-value="systemInfo.memory?.memoryLoadPercent || 0"
                    :color="getUsageColor(systemInfo.memory?.memoryLoadPercent || 0)"
                    height="20"
                    rounded
                  />
                </div>
                <div class="info-row"><span>Total Physical</span><span>{{ formatBytes(systemInfo.memory?.totalPhysicalBytes || 0) }}</span></div>
                <div class="info-row"><span>Available Physical</span><span>{{ formatBytes(systemInfo.memory?.availablePhysicalBytes || 0) }}</span></div>
                <div class="info-row"><span>Total Virtual</span><span>{{ formatBytes(systemInfo.memory?.totalVirtualBytes || 0) }}</span></div>
                <div class="info-row"><span>Available Virtual</span><span>{{ formatBytes(systemInfo.memory?.availableVirtualBytes || 0) }}</span></div>
                <div class="info-row"><span>Memory Type</span><span>{{ systemInfo.memory?.memoryType || '-' }}</span></div>
                <div class="info-row"><span>Speed</span><span>{{ systemInfo.memory?.speedMhz ? `${systemInfo.memory.speedMhz} MHz` : '-' }}</span></div>
                <div class="info-row"><span>Slots Used</span><span>{{ systemInfo.memory?.slotCount || '-' }}</span></div>
              </v-card-text>
            </v-card>
          </v-tabs-window-item>

          <!-- Storage Tab -->
          <v-tabs-window-item value="storage">
            <div class="scrollable-tab">
            <v-card v-for="disk in systemInfo.disks" :key="disk.driveLetter" variant="outlined" class="mb-4">
              <v-card-title class="text-subtitle-1">
                <v-icon class="mr-2">{{ disk.driveType === 'Fixed' ? 'mdi-harddisk' : 'mdi-usb-flash-drive' }}</v-icon>
                {{ disk.driveLetter }} {{ disk.volumeLabel ? `(${disk.volumeLabel})` : '' }}
                <v-chip v-if="disk.isSystemDrive" size="x-small" color="primary" class="ml-2">System</v-chip>
              </v-card-title>
              <v-card-text>
                <div class="mb-4">
                  <div class="d-flex justify-space-between mb-1">
                    <span>Used: {{ formatBytes(disk.totalBytes - disk.freeBytes) }} / {{ formatBytes(disk.totalBytes) }}</span>
                    <span>{{ getDiskUsagePercent(disk) }}%</span>
                  </div>
                  <v-progress-linear
                    :model-value="getDiskUsagePercent(disk)"
                    :color="getUsageColor(getDiskUsagePercent(disk))"
                    height="12"
                    rounded
                  />
                </div>
                <div class="info-row"><span>Free Space</span><span>{{ formatBytes(disk.freeBytes) }}</span></div>
                <div class="info-row"><span>File System</span><span>{{ disk.fileSystem || '-' }}</span></div>
                <div class="info-row"><span>Drive Type</span><span>{{ disk.driveType || '-' }}</span></div>
              </v-card-text>
            </v-card>
            </div>
          </v-tabs-window-item>

          <!-- Network Tab -->
          <v-tabs-window-item value="network">
            <div class="scrollable-tab">
            <v-card v-for="adapter in systemInfo.networkAdapters" :key="adapter.macAddress" variant="outlined" class="mb-4">
              <v-card-title class="text-subtitle-1">
                <v-icon class="mr-2">{{ adapter.adapterType.includes('Wireless') ? 'mdi-wifi' : 'mdi-ethernet' }}</v-icon>
                {{ adapter.name }}
                <v-chip :color="adapter.status === 'Up' ? 'success' : 'grey'" size="x-small" class="ml-2">
                  {{ adapter.status }}
                </v-chip>
              </v-card-title>
              <v-card-text>
                <div class="info-row"><span>Description</span><span>{{ adapter.description || '-' }}</span></div>
                <div class="info-row"><span>Type</span><span>{{ adapter.adapterType || '-' }}</span></div>
                <div class="info-row"><span>MAC Address</span><span>{{ formatMacAddress(adapter.macAddress) }}</span></div>
                <div class="info-row"><span>IP Address</span><span>{{ adapter.ipAddress || '-' }}</span></div>
                <div class="info-row"><span>Subnet Mask</span><span>{{ adapter.subnetMask || '-' }}</span></div>
                <div class="info-row"><span>Gateway</span><span>{{ adapter.defaultGateway || '-' }}</span></div>
                <div class="info-row"><span>DNS Servers</span><span>{{ adapter.dnsServers || '-' }}</span></div>
                <div class="info-row"><span>Speed</span><span>{{ formatNetworkSpeed(adapter.speedBps) }}</span></div>
                <div class="info-row"><span>DHCP</span><span>{{ adapter.isDhcpEnabled ? 'Enabled' : 'Disabled' }}</span></div>
              </v-card-text>
            </v-card>
            </div>
          </v-tabs-window-item>

          <!-- GPU Tab -->
          <v-tabs-window-item value="gpu">
            <v-card v-for="(gpu, index) in systemInfo.gpus" :key="index" variant="outlined" class="mb-4">
              <v-card-title class="text-subtitle-1">
                <v-icon class="mr-2">mdi-expansion-card</v-icon>
                {{ gpu.name }}
              </v-card-title>
              <v-card-text>
                <div class="info-row"><span>Manufacturer</span><span>{{ gpu.manufacturer || '-' }}</span></div>
                <div class="info-row"><span>Video Processor</span><span>{{ gpu.videoProcessor || '-' }}</span></div>
                <div class="info-row"><span>Video Memory</span><span>{{ formatBytes(gpu.videoMemoryBytes) }}</span></div>
                <div class="info-row"><span>Resolution</span><span>{{ gpu.resolution || '-' }}</span></div>
                <div class="info-row"><span>Refresh Rate</span><span>{{ gpu.currentRefreshRate ? `${gpu.currentRefreshRate} Hz` : '-' }}</span></div>
                <div class="info-row"><span>Driver Version</span><span>{{ gpu.driverVersion || '-' }}</span></div>
              </v-card-text>
            </v-card>
            <v-alert v-if="systemInfo.gpus.length === 0" type="info" variant="tonal">
              No GPU information available
            </v-alert>
          </v-tabs-window-item>

          <!-- Processes Tab -->
          <v-tabs-window-item value="processes">
            <v-card variant="outlined">
              <v-card-title class="text-subtitle-1 d-flex align-center">
                <v-icon class="mr-2">mdi-application</v-icon>
                Processes ({{ filteredProcesses.length }} / {{ systemInfo.performance?.topProcesses?.length || 0 }})
                <v-spacer />
                <v-text-field
                  v-model="processSearch"
                  density="compact"
                  variant="outlined"
                  placeholder="Search by PID or Name..."
                  prepend-inner-icon="mdi-magnify"
                  hide-details
                  clearable
                  style="max-width: 250px;"
                  class="ml-4"
                />
              </v-card-title>
              <v-card-text class="pa-0">
                <div class="processes-table-container">
                  <v-table density="compact" hover>
                    <thead>
                      <tr>
                        <th>PID</th>
                        <th>Name</th>
                        <th class="text-right">Memory</th>
                      </tr>
                    </thead>
                    <tbody>
                      <tr
                        v-for="proc in filteredProcesses"
                        :key="proc.pid"
                        class="process-row"
                        @contextmenu.prevent="showProcessContextMenu($event, proc)"
                      >
                        <td>{{ proc.pid }}</td>
                        <td>{{ proc.name }}</td>
                        <td class="text-right">{{ formatBytes(proc.memoryBytes) }}</td>
                      </tr>
                      <tr v-if="filteredProcesses.length === 0">
                        <td colspan="3" class="text-center text-grey py-4">
                          No processes found matching "{{ processSearch }}"
                        </td>
                      </tr>
                    </tbody>
                  </v-table>
                </div>
              </v-card-text>
            </v-card>
          </v-tabs-window-item>
        </v-tabs-window>
      </v-card-text>
    </v-card>
  </v-dialog>

  <!-- Process Context Menu -->
  <v-menu
    v-model="processContextMenu.show"
    :style="{ position: 'fixed', left: processContextMenu.x + 'px', top: processContextMenu.y + 'px' }"
    location-strategy="connected"
    scroll-strategy="close"
  >
    <v-list density="compact" class="py-0">
      <v-list-item
        prepend-icon="mdi-snowflake"
        title="Freeze Process"
        disabled
        @click="freezeProcess"
      />
      <v-list-item
        prepend-icon="mdi-skull"
        title="Kill Process"
        :disabled="killingProcess"
        @click="killProcess"
      >
        <template #append>
          <v-progress-circular v-if="killingProcess" indeterminate size="16" width="2" />
        </template>
      </v-list-item>
    </v-list>
  </v-menu>

  <!-- Kill Process Confirmation Snackbar -->
  <v-snackbar v-model="killSnackbar.show" :color="killSnackbar.color" :timeout="3000">
    {{ killSnackbar.message }}
  </v-snackbar>
</template>

<script setup lang="ts">
import { ref, computed, watch, reactive } from 'vue'
import { signalRService } from '@/services/signalr'
import type { ClientDto, DetailedSystemInfoDto, DiskInfoDto, ProcessInfoDto } from '@/types/client'

const props = defineProps<{
  modelValue: boolean
  client: ClientDto | null
}>()

const emit = defineEmits<{
  'update:modelValue': [value: boolean]
}>()

const dialog = computed({
  get: () => props.modelValue,
  set: (value) => emit('update:modelValue', value)
})

const tab = ref('overview')
const loading = ref(false)
const error = ref<string | null>(null)
const systemInfo = ref<DetailedSystemInfoDto | null>(null)
const responseTimeMs = ref<number | null>(null)

// Process tab state
const processSearch = ref('')
const killingProcess = ref(false)
const disablingUac = ref(false)
const powerActionLoading = ref<string | null>(null)
const selectedProcess = ref<ProcessInfoDto | null>(null)
const processContextMenu = reactive({
  show: false,
  x: 0,
  y: 0
})
const killSnackbar = reactive({
  show: false,
  message: '',
  color: 'success'
})

const filteredProcesses = computed(() => {
  const processes = systemInfo.value?.performance?.topProcesses || []
  if (!processSearch.value) return processes

  const search = processSearch.value.toLowerCase()
  return processes.filter(proc =>
    proc.pid.toString().includes(search) ||
    proc.name.toLowerCase().includes(search)
  )
})

const usedMemory = computed(() => {
  if (!systemInfo.value?.memory) return 0
  return systemInfo.value.memory.totalPhysicalBytes - systemInfo.value.memory.availablePhysicalBytes
})

watch(() => props.modelValue, async (newVal) => {
  if (newVal && props.client) {
    await fetchSystemInfo()
  }
})

async function fetchSystemInfo() {
  if (!props.client) return

  loading.value = true
  error.value = null
  responseTimeMs.value = null

  try {
    const result = await signalRService.requestSystemInfo(props.client.connectionKey)
    if (result.data) {
      systemInfo.value = result.data
      responseTimeMs.value = result.responseTimeMs
    } else {
      error.value = 'Failed to retrieve system information. The client may be offline.'
    }
  } catch (e) {
    error.value = e instanceof Error ? e.message : 'An error occurred'
  } finally {
    loading.value = false
  }
}

async function refresh() {
  await fetchSystemInfo()
}

function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return `${parseFloat((bytes / Math.pow(k, i)).toFixed(2))} ${sizes[i]}`
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

function formatMacAddress(mac: string): string {
  if (!mac || mac.length !== 12) return mac || '-'
  return mac.match(/.{2}/g)?.join(':') || mac
}

function formatNetworkSpeed(bps: number): string {
  if (bps <= 0) return '-'
  if (bps >= 1000000000) return `${(bps / 1000000000).toFixed(1)} Gbps`
  if (bps >= 1000000) return `${(bps / 1000000).toFixed(0)} Mbps`
  return `${(bps / 1000).toFixed(0)} Kbps`
}

function getDiskUsagePercent(disk: DiskInfoDto): number {
  if (disk.totalBytes === 0) return 0
  return Math.round(((disk.totalBytes - disk.freeBytes) / disk.totalBytes) * 100)
}

function getUsageColor(percent: number): string {
  if (percent >= 90) return 'error'
  if (percent >= 70) return 'warning'
  return 'success'
}

// Quick actions
async function disableUac() {
  if (!props.client) return

  disablingUac.value = true

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'disableuac',
      commandText: '',
      timeoutSeconds: 10
    }

    const success = await signalRService.executeCommand(command)

    if (success) {
      killSnackbar.message = `Disable UAC command sent to ${props.client.machineName}`
      killSnackbar.color = 'success'
    } else {
      killSnackbar.message = `Failed to send Disable UAC command`
      killSnackbar.color = 'error'
    }
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : 'Failed to disable UAC'
    killSnackbar.color = 'error'
  } finally {
    disablingUac.value = false
    killSnackbar.show = true
  }
}

async function sendPowerCommand(commandType: string, actionName: string) {
  if (!props.client) return

  powerActionLoading.value = commandType

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: commandType,
      commandText: '',
      timeoutSeconds: 10
    }

    const success = await signalRService.executeCommand(command)

    if (success) {
      killSnackbar.message = `${actionName} command sent to ${props.client.machineName}`
      killSnackbar.color = 'success'
    } else {
      killSnackbar.message = `Failed to send ${actionName} command`
      killSnackbar.color = 'error'
    }
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : `Failed to ${actionName.toLowerCase()}`
    killSnackbar.color = 'error'
  } finally {
    powerActionLoading.value = null
    killSnackbar.show = true
  }
}

// Process context menu functions
function showProcessContextMenu(event: MouseEvent, process: ProcessInfoDto) {
  selectedProcess.value = process
  processContextMenu.x = event.clientX
  processContextMenu.y = event.clientY
  processContextMenu.show = true
}

function freezeProcess() {
  // Not implemented yet
  processContextMenu.show = false
}

async function killProcess() {
  if (!selectedProcess.value || !props.client) return

  killingProcess.value = true
  processContextMenu.show = false

  try {
    // Use /F to force, /T to kill process tree, /IM to target by image name
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'cmd',
      commandText: `taskkill /IM "${selectedProcess.value.name}.exe" /F /T`,
      timeoutSeconds: 10
    }

    const success = await signalRService.executeCommand(command)

    if (success) {
      killSnackbar.message = `Kill command sent for process ${selectedProcess.value.name} (PID: ${selectedProcess.value.pid})`
      killSnackbar.color = 'success'
    } else {
      killSnackbar.message = `Failed to send kill command for process ${selectedProcess.value.name}`
      killSnackbar.color = 'error'
    }
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : 'Failed to kill process'
    killSnackbar.color = 'error'
  } finally {
    killingProcess.value = false
    killSnackbar.show = true
    selectedProcess.value = null

    // Refresh system info after a short delay to see updated process list
    setTimeout(() => refresh(), 1000)
  }
}
</script>

<style scoped>
.v-tabs-window {
  min-height: 400px;
  max-height: 60vh;
  overflow-y: auto;
}

.scrollable-tab {
  max-height: 50vh;
  overflow-y: auto;
  padding-right: 8px;
}

.processes-table-container {
  max-height: 45vh;
  overflow-y: auto;
}

.process-row {
  cursor: context-menu;
}

.process-row:hover {
  background-color: rgba(255, 255, 255, 0.05);
}

.info-row {
  display: flex;
  justify-content: space-between;
  padding: 6px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.1);
}

.info-row span:first-child {
  color: rgba(255, 255, 255, 0.7);
}

.info-row span:last-child {
  text-align: right;
  max-width: 60%;
  word-break: break-all;
}
</style>
