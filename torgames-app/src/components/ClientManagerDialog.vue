<template>
  <v-dialog v-model="dialog" :max-width="tab === 'logs' ? 1200 : 900" scrollable transition="dialog-bottom-transition">
    <v-card class="glass-panel rounded-xl border-none">
      <!-- Header -->
      <v-card-title class="d-flex align-center px-6 py-4 border-b border-opacity-10">
        <v-avatar color="primary" size="32" class="mr-3">
          <v-icon size="18" color="white">mdi-monitor</v-icon>
        </v-avatar>
        <span class="text-h6 font-weight-bold">Client Manager</span>
        <v-chip class="ml-3 font-weight-medium" size="small" variant="tonal" color="primary">
          {{ client?.machineName }}
        </v-chip>
        <v-spacer />
        <v-btn icon="mdi-refresh" variant="text" :loading="loading" @click="refresh" class="mr-2" />
        <v-btn icon="mdi-close" variant="text" @click="dialog = false" />
      </v-card-title>

      <!-- Loading State -->
      <v-card-text v-if="loading && !systemInfo" class="text-center py-12">
        <v-progress-circular indeterminate color="primary" size="64" width="6" />
        <div class="mt-6 text-h6 font-weight-light text-medium-emphasis">Fetching system information...</div>
      </v-card-text>

      <!-- Error State -->
      <v-card-text v-else-if="error" class="text-center py-12">
        <v-icon color="error" size="64" class="mb-4">mdi-alert-circle</v-icon>
        <div class="text-h6 text-error mb-2">Connection Failed</div>
        <div class="text-body-1 text-medium-emphasis mb-6">{{ error }}</div>
        <v-btn color="primary" prepend-icon="mdi-refresh" @click="refresh" class="px-6">Retry Connection</v-btn>
      </v-card-text>

      <!-- Content -->
      <v-card-text v-else-if="systemInfo" class="pa-0" style="height: 70vh; max-height: 700px; overflow: hidden;">
        <div class="d-flex" style="height: 100%;">
          <!-- Vertical Tabs -->
          <v-tabs
            v-model="tab"
            direction="vertical"
            color="primary"
            class="border-r border-opacity-10 py-4 flex-shrink-0"
            style="min-width: 160px;"
          >
            <v-tab value="overview" prepend-icon="mdi-view-dashboard-outline" class="justify-start px-6">Overview</v-tab>
            <v-tab value="cpu" prepend-icon="mdi-chip" class="justify-start px-6">CPU</v-tab>
            <v-tab value="memory" prepend-icon="mdi-memory" class="justify-start px-6">Memory</v-tab>
            <v-tab value="storage" prepend-icon="mdi-harddisk" class="justify-start px-6">Storage</v-tab>
            <v-tab value="network" prepend-icon="mdi-lan" class="justify-start px-6">Network</v-tab>
            <v-tab value="gpu" prepend-icon="mdi-expansion-card" class="justify-start px-6">GPU</v-tab>
            <v-tab value="processes" prepend-icon="mdi-application" class="justify-start px-6">Processes</v-tab>
            <v-tab value="logs" prepend-icon="mdi-text-box-outline" class="justify-start px-6">Logs</v-tab>
          </v-tabs>

          <!-- Tab Content -->
          <v-window v-model="tab" class="flex-grow-1" style="overflow-y: auto; overflow-x: hidden; height: 100%;">
            <!-- Overview Tab -->
            <v-window-item value="overview" class="pa-6">
              <v-alert v-if="responseTimeMs !== null" type="info" variant="tonal" density="compact" class="mb-6 rounded-lg border-none bg-opacity-10">
                <template #prepend>
                  <v-icon color="info">mdi-timer-outline</v-icon>
                </template>
                Response Time: <strong class="text-info">{{ responseTimeMs }}ms</strong>
              </v-alert>

              <v-row>
                <v-col cols="12" md="6">
                  <v-card class="glass-card mb-4 pa-4" variant="flat">
                    <div class="text-subtitle-2 text-medium-emphasis mb-4 d-flex align-center">
                      <v-icon size="small" class="mr-2">mdi-desktop-tower</v-icon> SYSTEM
                    </div>
                    <div class="info-row"><span>Manufacturer</span><span>{{ systemInfo.hardware?.manufacturer || '-' }}</span></div>
                    <div class="info-row"><span>Model</span><span>{{ systemInfo.hardware?.model || '-' }}</span></div>
                    <div class="info-row"><span>System Type</span><span>{{ systemInfo.hardware?.systemType || '-' }}</span></div>
                    <div class="info-row"><span>UUID</span><span class="text-caption font-mono">{{ systemInfo.hardware?.uuid || '-' }}</span></div>
                  </v-card>

                  <v-card class="glass-card pa-4" variant="flat">
                    <div class="text-subtitle-2 text-medium-emphasis mb-4 d-flex align-center">
                      <v-icon size="small" class="mr-2">mdi-microsoft-windows</v-icon> OPERATING SYSTEM
                    </div>
                    <div class="info-row"><span>Name</span><span>{{ systemInfo.os?.name || '-' }}</span></div>
                    <div class="info-row"><span>Version</span><span>{{ systemInfo.os?.version || '-' }}</span></div>
                    <div class="info-row"><span>Build</span><span>{{ systemInfo.os?.buildNumber || '-' }}</span></div>
                    <div class="info-row"><span>Architecture</span><span>{{ systemInfo.os?.architecture || '-' }}</span></div>
                    <div class="info-row"><span>User</span><span>{{ systemInfo.os?.registeredUser || '-' }}</span></div>
                  </v-card>
                </v-col>

                <v-col cols="12" md="6">
                  <v-card class="glass-card mb-4 pa-4" variant="flat">
                    <div class="text-subtitle-2 text-medium-emphasis mb-4 d-flex align-center">
                      <v-icon size="small" class="mr-2">mdi-speedometer</v-icon> PERFORMANCE
                    </div>
                    
                    <div class="mb-4">
                      <div class="d-flex justify-space-between mb-1 text-caption">
                        <span>CPU Usage</span>
                        <span :class="getUsageColorText(systemInfo.performance?.cpuUsagePercent || 0)">
                          {{ systemInfo.performance?.cpuUsagePercent?.toFixed(1) }}%
                        </span>
                      </div>
                      <v-progress-linear
                        :model-value="systemInfo.performance?.cpuUsagePercent || 0"
                        :color="getUsageColor(systemInfo.performance?.cpuUsagePercent || 0)"
                        height="6"
                        rounded
                        bg-color="rgba(255,255,255,0.1)"
                      />
                    </div>

                    <div class="mb-4">
                      <div class="d-flex justify-space-between mb-1 text-caption">
                        <span>Memory Usage</span>
                        <span :class="getUsageColorText(systemInfo.memory?.memoryLoadPercent || 0)">
                          {{ systemInfo.memory?.memoryLoadPercent }}%
                        </span>
                      </div>
                      <v-progress-linear
                        :model-value="systemInfo.memory?.memoryLoadPercent || 0"
                        :color="getUsageColor(systemInfo.memory?.memoryLoadPercent || 0)"
                        height="6"
                        rounded
                        bg-color="rgba(255,255,255,0.1)"
                      />
                    </div>

                    <div class="d-flex justify-space-between mt-4">
                      <div class="text-center">
                        <div class="text-h6 font-weight-bold">{{ systemInfo.performance?.processCount || '-' }}</div>
                        <div class="text-caption text-medium-emphasis">Processes</div>
                      </div>
                      <div class="text-center">
                        <div class="text-h6 font-weight-bold">{{ systemInfo.performance?.threadCount || '-' }}</div>
                        <div class="text-caption text-medium-emphasis">Threads</div>
                      </div>
                      <div class="text-center">
                        <div class="text-h6 font-weight-bold">{{ formatUptime(systemInfo.performance?.uptimeSeconds || 0) }}</div>
                        <div class="text-caption text-medium-emphasis">Uptime</div>
                      </div>
                    </div>
                  </v-card>

                  <v-card class="glass-card pa-4" variant="flat">
                    <div class="text-subtitle-2 text-medium-emphasis mb-4 d-flex align-center">
                      <v-icon size="small" class="mr-2">mdi-lightning-bolt</v-icon> QUICK ACTIONS
                    </div>
                    <div class="d-grid gap-2" style="display: grid; grid-template-columns: 1fr 1fr; gap: 8px;">
                      <v-btn
                        v-if="!client?.isAdmin"
                        color="warning"
                        variant="tonal"
                        prepend-icon="mdi-shield-account"
                        :loading="requestingElevation"
                        @click="requestElevation"
                        class="flex-grow-1"
                      >
                        Request Admin
                      </v-btn>
                      <v-btn
                        color="warning"
                        variant="tonal"
                        prepend-icon="mdi-shield-off"
                        :loading="disablingUac"
                        @click="disableUac"
                        class="flex-grow-1"
                      >
                        Disable UAC
                      </v-btn>
                      <v-btn
                        color="info"
                        variant="tonal"
                        prepend-icon="mdi-logout"
                        :loading="powerActionLoading === 'signout'"
                        @click="sendPowerCommand('signout', 'Sign Out')"
                        class="flex-grow-1"
                      >
                        Sign Out
                      </v-btn>
                      <v-btn
                        color="warning"
                        variant="tonal"
                        prepend-icon="mdi-restart"
                        :loading="powerActionLoading === 'restart'"
                        @click="sendPowerCommand('restart', 'Restart')"
                        class="flex-grow-1"
                      >
                        Restart
                      </v-btn>
                      <v-btn
                        color="error"
                        variant="tonal"
                        prepend-icon="mdi-power"
                        :loading="powerActionLoading === 'shutdown'"
                        @click="sendPowerCommand('shutdown', 'Shut Down')"
                        class="flex-grow-1"
                      >
                        Shut Down
                      </v-btn>
                    </div>
                  </v-card>
                </v-col>
              </v-row>
            </v-window-item>

            <!-- CPU Tab -->
            <v-window-item value="cpu" class="pa-6">
              <v-card class="glass-card pa-6" variant="flat">
                <div class="d-flex align-center mb-6">
                  <v-avatar color="primary" variant="tonal" class="mr-4">
                    <v-icon color="primary">mdi-chip</v-icon>
                  </v-avatar>
                  <div>
                    <div class="text-h6">{{ systemInfo.cpu?.name || 'Unknown Processor' }}</div>
                    <div class="text-caption text-medium-emphasis">{{ systemInfo.cpu?.manufacturer }}</div>
                  </div>
                </div>
                
                <v-divider class="mb-6 border-opacity-10"></v-divider>

                <div class="d-grid-3 gap-6" style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 24px;">
                  <div>
                    <div class="text-caption text-medium-emphasis mb-1">Architecture</div>
                    <div class="text-body-1">{{ systemInfo.cpu?.architecture || '-' }}</div>
                  </div>
                  <div>
                    <div class="text-caption text-medium-emphasis mb-1">Cores / Threads</div>
                    <div class="text-body-1">{{ systemInfo.cpu?.cores || '-' }} / {{ systemInfo.cpu?.logicalProcessors || '-' }}</div>
                  </div>
                  <div>
                    <div class="text-caption text-medium-emphasis mb-1">Max Speed</div>
                    <div class="text-body-1">{{ systemInfo.cpu?.maxClockSpeedMhz ? `${systemInfo.cpu.maxClockSpeedMhz} MHz` : '-' }}</div>
                  </div>
                  <div>
                    <div class="text-caption text-medium-emphasis mb-1">L2 Cache</div>
                    <div class="text-body-1">{{ systemInfo.cpu?.l2CacheKb ? `${systemInfo.cpu.l2CacheKb} KB` : '-' }}</div>
                  </div>
                  <div>
                    <div class="text-caption text-medium-emphasis mb-1">L3 Cache</div>
                    <div class="text-body-1">{{ systemInfo.cpu?.l3CacheKb ? `${(systemInfo.cpu.l3CacheKb / 1024).toFixed(1)} MB` : '-' }}</div>
                  </div>
                </div>
              </v-card>
            </v-window-item>

            <!-- Memory Tab -->
            <v-window-item value="memory" class="pa-6">
              <v-card class="glass-card pa-6 mb-6" variant="flat">
                <div class="d-flex align-center justify-space-between mb-6">
                  <div class="d-flex align-center">
                    <v-avatar color="secondary" variant="tonal" class="mr-4">
                      <v-icon color="secondary">mdi-memory</v-icon>
                    </v-avatar>
                    <div>
                      <div class="text-h6">Physical Memory</div>
                      <div class="text-caption text-medium-emphasis">{{ formatBytes(systemInfo.memory?.totalPhysicalBytes || 0) }} Total</div>
                    </div>
                  </div>
                  <div class="text-h4 font-weight-bold" :class="getUsageColorText(systemInfo.memory?.memoryLoadPercent || 0)">
                    {{ systemInfo.memory?.memoryLoadPercent }}%
                  </div>
                </div>

                <v-progress-linear
                  :model-value="systemInfo.memory?.memoryLoadPercent || 0"
                  :color="getUsageColor(systemInfo.memory?.memoryLoadPercent || 0)"
                  height="12"
                  rounded
                  bg-color="rgba(255,255,255,0.1)"
                  class="mb-2"
                />
                <div class="d-flex justify-space-between text-caption text-medium-emphasis">
                  <span>Used: {{ formatBytes(usedMemory) }}</span>
                  <span>Available: {{ formatBytes(systemInfo.memory?.availablePhysicalBytes || 0) }}</span>
                </div>
              </v-card>

              <v-card class="glass-card pa-6" variant="flat">
                <div class="text-subtitle-2 text-medium-emphasis mb-4">DETAILS</div>
                <div class="d-grid-2 gap-4" style="display: grid; grid-template-columns: 1fr 1fr; gap: 16px;">
                  <div class="info-row"><span>Total Virtual</span><span>{{ formatBytes(systemInfo.memory?.totalVirtualBytes || 0) }}</span></div>
                  <div class="info-row"><span>Available Virtual</span><span>{{ formatBytes(systemInfo.memory?.availableVirtualBytes || 0) }}</span></div>
                  <div class="info-row"><span>Memory Type</span><span>{{ systemInfo.memory?.memoryType || '-' }}</span></div>
                  <div class="info-row"><span>Speed</span><span>{{ systemInfo.memory?.speedMhz ? `${systemInfo.memory.speedMhz} MHz` : '-' }}</span></div>
                  <div class="info-row"><span>Slots Used</span><span>{{ systemInfo.memory?.slotCount || '-' }}</span></div>
                </div>
              </v-card>
            </v-window-item>

            <!-- Storage Tab -->
            <v-window-item value="storage" class="pa-6">
              <div class="d-flex flex-column gap-4">
                <v-card v-for="disk in systemInfo.disks" :key="disk.driveLetter" class="glass-card pa-4" variant="flat">
                  <div class="d-flex align-center mb-4">
                    <v-icon size="large" class="mr-3" color="primary">
                      {{ disk.driveType === 'Fixed' ? 'mdi-harddisk' : 'mdi-usb-flash-drive' }}
                    </v-icon>
                    <div>
                      <div class="text-subtitle-1 font-weight-bold">
                        {{ disk.driveLetter }} <span class="text-medium-emphasis font-weight-regular">{{ disk.volumeLabel ? `(${disk.volumeLabel})` : '' }}</span>
                      </div>
                      <div class="text-caption text-medium-emphasis">{{ disk.fileSystem }} • {{ disk.driveType }}</div>
                    </div>
                    <v-spacer />
                    <v-chip v-if="disk.isSystemDrive" size="small" color="primary" variant="tonal">System</v-chip>
                  </div>

                  <div class="mb-2">
                    <div class="d-flex justify-space-between mb-1 text-caption">
                      <span>{{ formatBytes(disk.totalBytes - disk.freeBytes) }} used</span>
                      <span>{{ formatBytes(disk.freeBytes) }} free</span>
                    </div>
                    <v-progress-linear
                      :model-value="getDiskUsagePercent(disk)"
                      :color="getUsageColor(getDiskUsagePercent(disk))"
                      height="8"
                      rounded
                      bg-color="rgba(255,255,255,0.1)"
                    />
                  </div>
                </v-card>
              </div>
            </v-window-item>

            <!-- Network Tab -->
            <v-window-item value="network" class="pa-6">
              <div class="d-flex flex-column gap-4">
                <v-card v-for="adapter in systemInfo.networkAdapters" :key="adapter.macAddress" class="glass-card pa-4" variant="flat">
                  <div class="d-flex align-center justify-space-between mb-4">
                    <div class="d-flex align-center">
                      <v-icon class="mr-3" :color="adapter.status === 'Up' ? 'success' : 'grey'">
                        {{ adapter.adapterType.includes('Wireless') ? 'mdi-wifi' : 'mdi-ethernet' }}
                      </v-icon>
                      <div class="text-subtitle-1 font-weight-medium text-truncate" style="max-width: 300px;">
                        {{ adapter.name }}
                      </div>
                    </div>
                    <v-chip :color="adapter.status === 'Up' ? 'success' : 'grey'" size="small" variant="tonal">
                      {{ adapter.status }}
                    </v-chip>
                  </div>

                  <div class="info-row"><span>Description</span><span class="text-truncate" style="max-width: 60%;">{{ adapter.description || '-' }}</span></div>
                  <div class="info-row"><span>MAC Address</span><span class="font-mono">{{ formatMacAddress(adapter.macAddress) }}</span></div>
                  <div class="info-row"><span>IP Address</span><span class="font-mono">{{ adapter.ipAddress || '-' }}</span></div>
                  <div class="info-row"><span>Gateway</span><span class="font-mono">{{ adapter.defaultGateway || '-' }}</span></div>
                  <div class="info-row"><span>Speed</span><span>{{ formatNetworkSpeed(adapter.speedBps) }}</span></div>
                </v-card>
              </div>
            </v-window-item>

            <!-- GPU Tab -->
            <v-window-item value="gpu" class="pa-6">
              <div v-if="systemInfo.gpus.length === 0" class="text-center py-8 text-medium-emphasis">
                <v-icon size="48" class="mb-2">mdi-expansion-card-variant</v-icon>
                <div>No GPU information available</div>
              </div>
              
              <div v-else class="d-flex flex-column gap-4">
                <v-card v-for="(gpu, index) in systemInfo.gpus" :key="index" class="glass-card pa-4" variant="flat">
                  <div class="d-flex align-center mb-4">
                    <v-avatar color="purple" variant="tonal" class="mr-3">
                      <v-icon color="purple">mdi-expansion-card</v-icon>
                    </v-avatar>
                    <div class="text-h6">{{ gpu.name }}</div>
                  </div>
                  
                  <div class="info-row"><span>Manufacturer</span><span>{{ gpu.manufacturer || '-' }}</span></div>
                  <div class="info-row"><span>Video Memory</span><span>{{ formatBytes(gpu.videoMemoryBytes) }}</span></div>
                  <div class="info-row"><span>Resolution</span><span>{{ gpu.resolution || '-' }}</span></div>
                  <div class="info-row"><span>Refresh Rate</span><span>{{ gpu.currentRefreshRate ? `${gpu.currentRefreshRate} Hz` : '-' }}</span></div>
                  <div class="info-row"><span>Driver Version</span><span>{{ gpu.driverVersion || '-' }}</span></div>
                </v-card>
              </div>
            </v-window-item>

            <!-- Processes Tab -->
            <v-window-item value="processes" class="pa-6 fill-height d-flex flex-column">
              <template v-if="tab === 'processes'">
                <div class="d-flex align-center mb-4">
                  <v-text-field
                    v-model="processSearch"
                    density="compact"
                    variant="outlined"
                    placeholder="Search processes..."
                    prepend-inner-icon="mdi-magnify"
                    hide-details
                    class="glass-card rounded-lg flex-grow-1"
                    bg-color="transparent"
                  />
                  <div class="ml-4 text-caption text-medium-emphasis">
                    {{ filteredProcesses.length }} processes
                  </div>
                </div>

                <v-card class="glass-card flex-grow-1 d-flex flex-column" variant="flat" style="overflow: hidden;">
                  <v-table density="compact" hover class="bg-transparent flex-grow-1" fixed-header>
                    <thead>
                      <tr>
                        <th class="text-left bg-transparent">PID</th>
                        <th class="text-left bg-transparent">Name</th>
                        <th class="text-right bg-transparent">Memory</th>
                      </tr>
                    </thead>
                    <tbody>
                      <tr
                        v-for="proc in filteredProcesses"
                        :key="proc.pid"
                        class="process-row"
                        :class="{ 'frozen-process': isProcessFrozen(proc.pid) }"
                        @contextmenu.prevent="showProcessContextMenu($event, proc)"
                      >
                        <td class="text-medium-emphasis font-mono">{{ proc.pid }}</td>
                        <td class="font-weight-medium">
                          <v-icon v-if="isProcessFrozen(proc.pid)" size="small" color="info" class="mr-1">mdi-snowflake</v-icon>
                          {{ proc.name }}
                        </td>
                        <td class="text-right font-mono">{{ formatBytes(proc.memoryBytes) }}</td>
                      </tr>
                      <tr v-if="filteredProcesses.length === 0">
                        <td colspan="3" class="text-center text-medium-emphasis py-8">
                          No processes found matching "{{ processSearch }}"
                        </td>
                      </tr>
                    </tbody>
                  </v-table>
                </v-card>
              </template>
            </v-window-item>

            <!-- Logs Tab -->
            <v-window-item value="logs" class="pa-6 fill-height d-flex flex-column">
              <template v-if="tab === 'logs'">
                <div class="d-flex align-center mb-4">
                  <v-btn
                    color="primary"
                    variant="tonal"
                    prepend-icon="mdi-refresh"
                    :loading="loadingLogs"
                    @click="fetchLogs"
                    class="mr-3"
                  >
                    Refresh Logs
                  </v-btn>
                  <v-select
                    v-model="logLineCount"
                    :items="[100, 200, 300, 500]"
                    density="compact"
                    variant="outlined"
                    label="Lines"
                    hide-details
                    style="max-width: 120px;"
                    class="mr-3"
                  />
                  <v-spacer />
                  <v-text-field
                    v-model="logSearch"
                    density="compact"
                    variant="outlined"
                    placeholder="Filter logs..."
                    prepend-inner-icon="mdi-filter"
                    hide-details
                    clearable
                    style="max-width: 250px;"
                    bg-color="transparent"
                  />
                </div>

                <v-card class="glass-card flex-grow-1 d-flex flex-column" variant="flat" style="overflow: hidden;">
                  <!-- Loading State -->
                  <div v-if="loadingLogs" class="d-flex align-center justify-center fill-height">
                    <v-progress-circular indeterminate color="primary" size="48" />
                  </div>

                  <!-- Error State -->
                  <div v-else-if="logsError" class="d-flex flex-column align-center justify-center fill-height text-error">
                    <v-icon size="48" class="mb-2">mdi-alert-circle</v-icon>
                    <div>{{ logsError }}</div>
                    <v-btn color="primary" variant="tonal" class="mt-4" @click="fetchLogs">Retry</v-btn>
                  </div>

                  <!-- No Logs -->
                  <div v-else-if="!clientLogs" class="d-flex flex-column align-center justify-center fill-height text-medium-emphasis">
                    <v-icon size="48" class="mb-2">mdi-text-box-outline</v-icon>
                    <div>Click "Refresh Logs" to fetch client logs</div>
                  </div>

                  <!-- Logs Content -->
                  <div
                    v-else
                    ref="logsContainer"
                    class="logs-content pa-4"
                    style="overflow-y: auto; overflow-x: auto; flex: 1 1 auto; max-height: 60vh;"
                  >
                    <pre style="margin: 0; font-family: 'Consolas', 'Monaco', monospace; font-size: 12px; line-height: 1.5; white-space: pre-wrap; word-break: break-word;"><code v-html="highlightedLogs"></code></pre>
                  </div>
                </v-card>
              </template>
            </v-window-item>
          </v-window>
        </div>
      </v-card-text>
    </v-card>
  </v-dialog>

  <!-- Process Context Menu -->
  <v-menu
    v-model="processContextMenu.show"
    :style="{ position: 'fixed', left: processContextMenu.x + 'px', top: processContextMenu.y + 'px' }"
    location-strategy="connected"
  >
    <v-list density="compact" class="glass-panel py-2" width="220">
      <v-list-item
        v-if="selectedProcess && !isProcessFrozen(selectedProcess.pid)"
        prepend-icon="mdi-snowflake"
        title="Freeze Process"
        :disabled="freezingProcess"
        @click="freezeProcess"
        class="text-info"
      >
        <template #append>
          <v-progress-circular v-if="freezingProcess" indeterminate size="16" width="2" color="info" />
        </template>
      </v-list-item>
      <v-list-item
        v-else-if="selectedProcess && isProcessFrozen(selectedProcess.pid)"
        prepend-icon="mdi-play"
        title="Resume Process"
        :disabled="freezingProcess"
        @click="resumeProcess"
        class="text-success"
      >
        <template #append>
          <v-progress-circular v-if="freezingProcess" indeterminate size="16" width="2" color="success" />
        </template>
      </v-list-item>
      <v-list-item
        prepend-icon="mdi-skull"
        title="Kill Process"
        :disabled="killingProcess"
        @click="killProcess"
        class="text-error"
      >
        <template #append>
          <v-progress-circular v-if="killingProcess" indeterminate size="16" width="2" color="error" />
        </template>
      </v-list-item>
    </v-list>
  </v-menu>

  <!-- Snackbar -->
  <v-snackbar v-model="killSnackbar.show" :color="killSnackbar.color" :timeout="3000" location="bottom right">
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
const freezingProcess = ref(false)
const frozenProcesses = ref<Set<number>>(new Set())
const disablingUac = ref(false)
const requestingElevation = ref(false)
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

// Logs tab state
const loadingLogs = ref(false)
const logsError = ref<string | null>(null)
const clientLogs = ref<string | null>(null)
const logLineCount = ref(200)
const logSearch = ref('')
const logsContainer = ref<HTMLElement | null>(null)

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

// Computed property for filtered and highlighted logs
const highlightedLogs = computed(() => {
  if (!clientLogs.value) return ''

  let logs = clientLogs.value

  // Filter by search term if provided
  if (logSearch.value) {
    const searchLower = logSearch.value.toLowerCase()
    logs = logs
      .split('\n')
      .filter(line => line.toLowerCase().includes(searchLower))
      .join('\n')
  }

  // Escape HTML and apply syntax highlighting
  logs = escapeHtml(logs)

  // Highlight log levels with colors
  logs = logs
    .replace(/\[info\]/g, '<span style="color: #4caf50;">[info]</span>')
    .replace(/\[warn\]/g, '<span style="color: #ff9800;">[warn]</span>')
    .replace(/\[fail\]/g, '<span style="color: #f44336;">[fail]</span>')
    .replace(/\[crit\]/g, '<span style="color: #d32f2f; font-weight: bold;">[crit]</span>')
    .replace(/\[dbug\]/g, '<span style="color: #9e9e9e;">[dbug]</span>')
    .replace(/\[trce\]/g, '<span style="color: #757575;">[trce]</span>')

  // Highlight timestamps
  logs = logs.replace(/(\d{2}:\d{2}:\d{2})/g, '<span style="color: #64b5f6;">$1</span>')

  return logs
})

function escapeHtml(text: string): string {
  const div = document.createElement('div')
  div.textContent = text
  return div.innerHTML
}

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

async function fetchLogs() {
  if (!props.client) return

  loadingLogs.value = true
  logsError.value = null

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'getlogs',
      commandText: logLineCount.value.toString(),
      timeoutSeconds: 15
    }

    const result = await signalRService.executeCommandWithResult(command)

    if (result && result.success) {
      clientLogs.value = result.stdout || '[No logs available]'
      // Scroll to bottom after logs load
      setTimeout(() => {
        if (logsContainer.value) {
          logsContainer.value.scrollTop = logsContainer.value.scrollHeight
        }
      }, 100)
    } else {
      logsError.value = result?.errorMessage || 'Failed to fetch logs'
    }
  } catch (e) {
    logsError.value = e instanceof Error ? e.message : 'Failed to fetch logs'
  } finally {
    loadingLogs.value = false
  }
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

function getUsageColorText(percent: number): string {
  if (percent >= 90) return 'text-error'
  if (percent >= 70) return 'text-warning'
  return 'text-success'
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

async function requestElevation() {
  if (!props.client) return

  requestingElevation.value = true

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'elevate',
      commandText: '',
      timeoutSeconds: 300  // Long timeout - prompts keep appearing until accepted
    }

    // Fire and forget - the command will keep prompting until accepted
    await signalRService.executeCommand(command)

    killSnackbar.message = `Elevation request sent to ${props.client.machineName}. UAC prompts will appear until accepted.`
    killSnackbar.color = 'info'
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : 'Failed to request elevation'
    killSnackbar.color = 'error'
  } finally {
    requestingElevation.value = false
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

function isProcessFrozen(pid: number): boolean {
  return frozenProcesses.value.has(pid)
}

async function freezeProcess() {
  if (!selectedProcess.value || !props.client) return

  freezingProcess.value = true
  processContextMenu.show = false

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'freezeprocess',
      commandText: selectedProcess.value.pid.toString(),
      timeoutSeconds: 10
    }

    const success = await signalRService.executeCommand(command)

    if (success) {
      frozenProcesses.value.add(selectedProcess.value.pid)
      killSnackbar.message = `Process ${selectedProcess.value.name} (PID: ${selectedProcess.value.pid}) frozen`
      killSnackbar.color = 'info'
    } else {
      killSnackbar.message = `Failed to freeze process ${selectedProcess.value.name}`
      killSnackbar.color = 'error'
    }
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : 'Failed to freeze process'
    killSnackbar.color = 'error'
  } finally {
    freezingProcess.value = false
    killSnackbar.show = true
    selectedProcess.value = null
  }
}

async function resumeProcess() {
  if (!selectedProcess.value || !props.client) return

  freezingProcess.value = true
  processContextMenu.show = false

  try {
    const command = {
      connectionKey: props.client.connectionKey,
      commandType: 'resumeprocess',
      commandText: selectedProcess.value.pid.toString(),
      timeoutSeconds: 10
    }

    const success = await signalRService.executeCommand(command)

    if (success) {
      frozenProcesses.value.delete(selectedProcess.value.pid)
      killSnackbar.message = `Process ${selectedProcess.value.name} (PID: ${selectedProcess.value.pid}) resumed`
      killSnackbar.color = 'success'
    } else {
      killSnackbar.message = `Failed to resume process ${selectedProcess.value.name}`
      killSnackbar.color = 'error'
    }
  } catch (e) {
    killSnackbar.message = e instanceof Error ? e.message : 'Failed to resume process'
    killSnackbar.color = 'error'
  } finally {
    freezingProcess.value = false
    killSnackbar.show = true
    selectedProcess.value = null
  }
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
.info-row {
  display: flex;
  justify-content: space-between;
  padding: 8px 0;
  border-bottom: 1px solid rgba(255, 255, 255, 0.05);
}

.info-row:last-child {
  border-bottom: none;
}

.info-row span:first-child {
  color: rgba(255, 255, 255, 0.5);
  font-size: 0.875rem;
}

.info-row span:last-child {
  text-align: right;
  max-width: 60%;
  word-break: break-all;
  font-weight: 500;
}

.process-row {
  cursor: context-menu;
  transition: background-color 0.2s;
}

.process-row:hover {
  background-color: rgba(255, 255, 255, 0.05) !important;
}

.frozen-process {
  background-color: rgba(33, 150, 243, 0.1) !important;
}

.frozen-process:hover {
  background-color: rgba(33, 150, 243, 0.15) !important;
}

/* Custom Scrollbar for tabs */
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

/* Logs styling */
.logs-content {
  background: rgba(0, 0, 0, 0.3);
  color: #e0e0e0;
  white-space: pre-wrap;
  word-break: break-all;
}

.logs-content code {
  font-family: 'Consolas', 'Monaco', 'Courier New', monospace;
}
</style>
