<template>
  <div class="server-stats-panel">
    <!-- Stats Overview Cards -->
    <v-row class="mb-4">
      <v-col cols="12" sm="6" lg="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="primary" class="mr-2">mdi-chip</v-icon>
            <span class="text-caption text-medium-emphasis">CPU USAGE</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h4 font-weight-bold" :class="getUsageColorText(currentStats?.cpuUsagePercent ?? 0)">
              {{ currentStats?.cpuUsagePercent?.toFixed(1) ?? '-' }}%
            </div>
          </div>
          <v-progress-linear
            :model-value="currentStats?.cpuUsagePercent ?? 0"
            :color="getUsageColor(currentStats?.cpuUsagePercent ?? 0)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" lg="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="secondary" class="mr-2">mdi-memory</v-icon>
            <span class="text-caption text-medium-emphasis">MEMORY</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h4 font-weight-bold" :class="getUsageColorText(memoryPercent)">
              {{ memoryPercent.toFixed(0) }}%
            </div>
            <div class="text-caption text-medium-emphasis text-right">
              {{ formatBytes(currentStats?.memoryUsedBytes ?? 0) }}<br>
              <span class="text-disabled">/ {{ formatBytes(currentStats?.memoryTotalBytes ?? 0) }}</span>
            </div>
          </div>
          <v-progress-linear
            :model-value="memoryPercent"
            :color="getUsageColor(memoryPercent)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" lg="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="warning" class="mr-2">mdi-harddisk</v-icon>
            <span class="text-caption text-medium-emphasis">DISK SPACE</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h4 font-weight-bold" :class="getUsageColorText(diskPercent)">
              {{ diskPercent.toFixed(0) }}%
            </div>
            <div class="text-caption text-medium-emphasis text-right">
              {{ formatBytes(currentStats?.diskUsedBytes ?? 0) }}<br>
              <span class="text-disabled">/ {{ formatBytes(currentStats?.diskTotalBytes ?? 0) }}</span>
            </div>
          </div>
          <v-progress-linear
            :model-value="diskPercent"
            :color="getUsageColor(diskPercent)"
            height="4"
            rounded
            class="mt-2"
            bg-color="rgba(255,255,255,0.1)"
          />
        </v-card>
      </v-col>

      <v-col cols="12" sm="6" lg="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="info" class="mr-2">mdi-swap-vertical</v-icon>
            <span class="text-caption text-medium-emphasis">NETWORK I/O</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-body-1 font-weight-medium">
              <span class="text-cyan">{{ formatBytesPerSec(currentStats?.networkBytesInPerSec ?? 0) }}</span>
              <v-icon size="x-small" class="mx-1">mdi-arrow-down</v-icon>
            </div>
            <div class="text-body-1 font-weight-medium">
              <v-icon size="x-small" class="mx-1">mdi-arrow-up</v-icon>
              <span class="text-amber">{{ formatBytesPerSec(currentStats?.networkBytesOutPerSec ?? 0) }}</span>
            </div>
          </div>
          <div class="d-flex align-center mt-2 text-caption text-medium-emphasis">
            <v-icon size="x-small" class="mr-1">mdi-clock-outline</v-icon>
            Uptime: {{ formatUptime(currentStats?.uptimeSeconds ?? 0) }}
          </div>
        </v-card>
      </v-col>
    </v-row>

    <!-- Second Row: Clients -->
    <v-row class="mb-4">
      <v-col cols="12" sm="6" lg="3">
        <v-card class="glass-card pa-4" variant="flat">
          <div class="d-flex align-center mb-2">
            <v-icon color="success" class="mr-2">mdi-account-group</v-icon>
            <span class="text-caption text-medium-emphasis">CONNECTED CLIENTS</span>
          </div>
          <div class="d-flex align-center justify-space-between">
            <div class="text-h4 font-weight-bold text-success">
              {{ currentStats?.connectedClients ?? '-' }}
            </div>
            <div class="text-caption text-medium-emphasis text-right">
              Online<br>
              <span class="text-disabled">/ {{ currentStats?.totalClients ?? '-' }} total</span>
            </div>
          </div>
        </v-card>
      </v-col>
    </v-row>

    <!-- Real-time Charts -->
    <v-row>
      <!-- CPU History Chart -->
      <v-col cols="12" lg="6">
        <v-card class="glass-card" variant="flat">
          <v-card-title class="d-flex align-center text-body-1 font-weight-medium">
            <v-icon color="primary" size="small" class="mr-2">mdi-chart-line</v-icon>
            CPU Usage History
            <v-spacer />
            <span class="text-caption text-medium-emphasis">Last 60 seconds</span>
          </v-card-title>
          <v-card-text class="pa-4 pt-0">
            <div class="chart-container">
              <canvas ref="cpuChart" />
            </div>
          </v-card-text>
        </v-card>
      </v-col>

      <!-- Memory History Chart -->
      <v-col cols="12" lg="6">
        <v-card class="glass-card" variant="flat">
          <v-card-title class="d-flex align-center text-body-1 font-weight-medium">
            <v-icon color="secondary" size="small" class="mr-2">mdi-chart-line</v-icon>
            Memory Usage History
            <v-spacer />
            <span class="text-caption text-medium-emphasis">Last 60 seconds</span>
          </v-card-title>
          <v-card-text class="pa-4 pt-0">
            <div class="chart-container">
              <canvas ref="memoryChart" />
            </div>
          </v-card-text>
        </v-card>
      </v-col>

      <!-- Connected Clients History -->
      <v-col cols="12" lg="6">
        <v-card class="glass-card" variant="flat">
          <v-card-title class="d-flex align-center text-body-1 font-weight-medium">
            <v-icon color="success" size="small" class="mr-2">mdi-chart-bar</v-icon>
            Connected Clients History
            <v-spacer />
            <span class="text-caption text-medium-emphasis">Last 60 seconds</span>
          </v-card-title>
          <v-card-text class="pa-4 pt-0">
            <div class="chart-container">
              <canvas ref="clientsChart" />
            </div>
          </v-card-text>
        </v-card>
      </v-col>

      <!-- Network I/O Chart (placeholder for future) -->
      <v-col cols="12" lg="6">
        <v-card class="glass-card" variant="flat">
          <v-card-title class="d-flex align-center text-body-1 font-weight-medium">
            <v-icon color="info" size="small" class="mr-2">mdi-swap-vertical</v-icon>
            Network Activity
            <v-spacer />
            <span class="text-caption text-medium-emphasis">Last 60 seconds</span>
          </v-card-title>
          <v-card-text class="pa-4 pt-0">
            <div class="chart-container">
              <canvas ref="networkChart" />
            </div>
          </v-card-text>
        </v-card>
      </v-col>
    </v-row>

    <!-- Server Info -->
    <v-row class="mt-2">
      <v-col cols="12">
        <v-card class="glass-card" variant="flat">
          <v-card-title class="d-flex align-center text-body-1 font-weight-medium">
            <v-icon color="primary" size="small" class="mr-2">mdi-server</v-icon>
            Server Information
          </v-card-title>
          <v-card-text>
            <v-row>
              <v-col cols="12" sm="6" md="3">
                <div class="text-caption text-medium-emphasis mb-1">PROCESS MEMORY</div>
                <div class="text-body-1 font-weight-medium">{{ formatBytes(currentStats?.memoryUsedBytes ?? 0) }}</div>
              </v-col>
              <v-col cols="12" sm="6" md="3">
                <div class="text-caption text-medium-emphasis mb-1">TOTAL MEMORY</div>
                <div class="text-body-1 font-weight-medium">{{ formatBytes(currentStats?.memoryTotalBytes ?? 0) }}</div>
              </v-col>
              <v-col cols="12" sm="6" md="3">
                <div class="text-caption text-medium-emphasis mb-1">DISK USED</div>
                <div class="text-body-1 font-weight-medium">{{ formatBytes(currentStats?.diskUsedBytes ?? 0) }}</div>
              </v-col>
              <v-col cols="12" sm="6" md="3">
                <div class="text-caption text-medium-emphasis mb-1">DISK TOTAL</div>
                <div class="text-body-1 font-weight-medium">{{ formatBytes(currentStats?.diskTotalBytes ?? 0) }}</div>
              </v-col>
            </v-row>
          </v-card-text>
        </v-card>
      </v-col>
    </v-row>
  </div>
</template>

<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted, nextTick } from 'vue'
import { useAuthStore } from '@/stores/auth'
import { getServerStats, type ServerStats } from '@/services/api'

const authStore = useAuthStore()

// Chart refs
const cpuChart = ref<HTMLCanvasElement | null>(null)
const memoryChart = ref<HTMLCanvasElement | null>(null)
const clientsChart = ref<HTMLCanvasElement | null>(null)
const networkChart = ref<HTMLCanvasElement | null>(null)

// Data history (60 data points for 60 seconds)
const MAX_HISTORY = 60
const cpuHistory = ref<number[]>([])
const memoryHistory = ref<number[]>([])
const clientsHistory = ref<number[]>([])
const networkInHistory = ref<number[]>([])
const networkOutHistory = ref<number[]>([])

// Current stats
const currentStats = ref<ServerStats | null>(null)
let statsInterval: ReturnType<typeof setInterval> | null = null

// Computed
const memoryPercent = computed(() => {
  if (!currentStats.value) return 0
  return (currentStats.value.memoryUsedBytes / currentStats.value.memoryTotalBytes) * 100
})

const diskPercent = computed(() => {
  if (!currentStats.value) return 0
  return (currentStats.value.diskUsedBytes / currentStats.value.diskTotalBytes) * 100
})

// Utility functions
function formatBytes(bytes: number): string {
  if (bytes === 0) return '0 B'
  const k = 1024
  const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i]
}

function formatBytesPerSec(bytes: number): string {
  if (bytes === 0) return '0 B/s'
  const k = 1024
  const sizes = ['B/s', 'KB/s', 'MB/s', 'GB/s']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(1)) + ' ' + (sizes[i] ?? 'B/s')
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

// Chart drawing functions
function drawLineChart(
  canvas: HTMLCanvasElement,
  data: number[],
  color: string,
  maxValue: number = 100,
  label: string = '%',
  fillColor?: string
) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return

  const dpr = window.devicePixelRatio || 1
  const rect = canvas.getBoundingClientRect()

  canvas.width = rect.width * dpr
  canvas.height = rect.height * dpr
  ctx.scale(dpr, dpr)

  const width = rect.width
  const height = rect.height
  const padding = { top: 20, right: 40, bottom: 25, left: 10 }
  const chartWidth = width - padding.left - padding.right
  const chartHeight = height - padding.top - padding.bottom

  // Clear
  ctx.clearRect(0, 0, width, height)

  // Draw grid lines
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)'
  ctx.lineWidth = 1

  // Horizontal grid lines
  for (let i = 0; i <= 4; i++) {
    const y = padding.top + (chartHeight / 4) * i
    ctx.beginPath()
    ctx.moveTo(padding.left, y)
    ctx.lineTo(width - padding.right, y)
    ctx.stroke()

    // Y-axis labels
    const value = maxValue - (maxValue / 4) * i
    ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'right'
    ctx.fillText(`${value.toFixed(0)}${label}`, width - 5, y + 3)
  }

  // Draw data line
  if (data.length > 1) {
    const stepX = chartWidth / (MAX_HISTORY - 1)

    // Fill area under curve
    if (fillColor) {
      ctx.beginPath()
      ctx.moveTo(padding.left, padding.top + chartHeight)

      data.forEach((value, index) => {
        const x = padding.left + index * stepX
        const y = padding.top + chartHeight - (value / maxValue) * chartHeight
        ctx.lineTo(x, y)
      })

      ctx.lineTo(padding.left + (data.length - 1) * stepX, padding.top + chartHeight)
      ctx.closePath()

      const gradient = ctx.createLinearGradient(0, padding.top, 0, padding.top + chartHeight)
      gradient.addColorStop(0, fillColor)
      gradient.addColorStop(1, 'rgba(0, 0, 0, 0)')
      ctx.fillStyle = gradient
      ctx.fill()
    }

    // Draw line
    ctx.beginPath()
    ctx.strokeStyle = color
    ctx.lineWidth = 2
    ctx.lineJoin = 'round'
    ctx.lineCap = 'round'

    data.forEach((value, index) => {
      const x = padding.left + index * stepX
      const y = padding.top + chartHeight - (value / maxValue) * chartHeight

      if (index === 0) {
        ctx.moveTo(x, y)
      } else {
        ctx.lineTo(x, y)
      }
    })

    ctx.stroke()

    // Draw current value dot
    if (data.length > 0) {
      const lastValue = data[data.length - 1] ?? 0
      const lastX = padding.left + (data.length - 1) * stepX
      const lastY = padding.top + chartHeight - (lastValue / maxValue) * chartHeight

      ctx.beginPath()
      ctx.arc(lastX, lastY, 4, 0, Math.PI * 2)
      ctx.fillStyle = color
      ctx.fill()

      ctx.beginPath()
      ctx.arc(lastX, lastY, 6, 0, Math.PI * 2)
      ctx.strokeStyle = color
      ctx.lineWidth = 2
      ctx.stroke()
    }
  }

  // Time labels
  ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
  ctx.font = '10px sans-serif'
  ctx.textAlign = 'center'
  ctx.fillText('-60s', padding.left, height - 5)
  ctx.fillText('-30s', padding.left + chartWidth / 2, height - 5)
  ctx.fillText('now', width - padding.right, height - 5)
}

function drawDualLineChart(
  canvas: HTMLCanvasElement,
  data1: number[],
  data2: number[],
  color1: string,
  color2: string,
  maxValue: number,
  _label: string = ''
) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return

  const dpr = window.devicePixelRatio || 1
  const rect = canvas.getBoundingClientRect()

  canvas.width = rect.width * dpr
  canvas.height = rect.height * dpr
  ctx.scale(dpr, dpr)

  const width = rect.width
  const height = rect.height
  const padding = { top: 20, right: 60, bottom: 25, left: 10 }
  const chartWidth = width - padding.left - padding.right
  const chartHeight = height - padding.top - padding.bottom

  // Clear
  ctx.clearRect(0, 0, width, height)

  // Draw grid lines
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)'
  ctx.lineWidth = 1

  for (let i = 0; i <= 4; i++) {
    const y = padding.top + (chartHeight / 4) * i
    ctx.beginPath()
    ctx.moveTo(padding.left, y)
    ctx.lineTo(width - padding.right, y)
    ctx.stroke()

    const value = maxValue - (maxValue / 4) * i
    ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'right'
    ctx.fillText(formatBytesShort(value), width - 5, y + 3)
  }

  const stepX = chartWidth / (MAX_HISTORY - 1)

  // Draw data1 (in)
  if (data1.length > 1) {
    ctx.beginPath()
    ctx.strokeStyle = color1
    ctx.lineWidth = 2

    data1.forEach((value, index) => {
      const x = padding.left + index * stepX
      const y = padding.top + chartHeight - (value / maxValue) * chartHeight
      if (index === 0) ctx.moveTo(x, y)
      else ctx.lineTo(x, y)
    })
    ctx.stroke()
  }

  // Draw data2 (out)
  if (data2.length > 1) {
    ctx.beginPath()
    ctx.strokeStyle = color2
    ctx.lineWidth = 2

    data2.forEach((value, index) => {
      const x = padding.left + index * stepX
      const y = padding.top + chartHeight - (value / maxValue) * chartHeight
      if (index === 0) ctx.moveTo(x, y)
      else ctx.lineTo(x, y)
    })
    ctx.stroke()
  }

  // Legend
  ctx.fillStyle = color1
  ctx.fillRect(width - padding.right + 10, 10, 12, 3)
  ctx.fillStyle = 'rgba(255, 255, 255, 0.7)'
  ctx.font = '10px sans-serif'
  ctx.textAlign = 'left'
  ctx.fillText('In', width - padding.right + 25, 14)

  ctx.fillStyle = color2
  ctx.fillRect(width - padding.right + 10, 22, 12, 3)
  ctx.fillStyle = 'rgba(255, 255, 255, 0.7)'
  ctx.fillText('Out', width - padding.right + 25, 26)

  // Time labels
  ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
  ctx.textAlign = 'center'
  ctx.fillText('-60s', padding.left, height - 5)
  ctx.fillText('now', width - padding.right, height - 5)
}

function formatBytesShort(bytes: number): string {
  if (bytes === 0) return '0'
  const k = 1024
  const sizes = ['B', 'K', 'M', 'G']
  const i = Math.floor(Math.log(bytes) / Math.log(k))
  return parseFloat((bytes / Math.pow(k, i)).toFixed(0)) + (sizes[i] ?? '')
}

function drawBarChart(
  canvas: HTMLCanvasElement,
  data: number[],
  color: string,
  maxValue: number
) {
  const ctx = canvas.getContext('2d')
  if (!ctx) return

  const dpr = window.devicePixelRatio || 1
  const rect = canvas.getBoundingClientRect()

  canvas.width = rect.width * dpr
  canvas.height = rect.height * dpr
  ctx.scale(dpr, dpr)

  const width = rect.width
  const height = rect.height
  const padding = { top: 20, right: 40, bottom: 25, left: 10 }
  const chartWidth = width - padding.left - padding.right
  const chartHeight = height - padding.top - padding.bottom

  // Clear
  ctx.clearRect(0, 0, width, height)

  // Draw grid lines
  ctx.strokeStyle = 'rgba(255, 255, 255, 0.1)'
  ctx.lineWidth = 1

  for (let i = 0; i <= 4; i++) {
    const y = padding.top + (chartHeight / 4) * i
    ctx.beginPath()
    ctx.moveTo(padding.left, y)
    ctx.lineTo(width - padding.right, y)
    ctx.stroke()

    const value = maxValue - (maxValue / 4) * i
    ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
    ctx.font = '10px sans-serif'
    ctx.textAlign = 'right'
    ctx.fillText(value.toFixed(0), width - 5, y + 3)
  }

  // Draw bars
  if (data.length > 0) {
    const barWidth = Math.max(2, chartWidth / MAX_HISTORY - 1)
    const stepX = chartWidth / MAX_HISTORY

    data.forEach((value, index) => {
      const x = padding.left + index * stepX
      const barHeight = (value / maxValue) * chartHeight
      const y = padding.top + chartHeight - barHeight

      const gradient = ctx.createLinearGradient(x, y, x, y + barHeight)
      gradient.addColorStop(0, color)
      gradient.addColorStop(1, 'rgba(0, 0, 0, 0.3)')

      ctx.fillStyle = gradient
      ctx.fillRect(x, y, barWidth, barHeight)
    })
  }

  // Time labels
  ctx.fillStyle = 'rgba(255, 255, 255, 0.5)'
  ctx.font = '10px sans-serif'
  ctx.textAlign = 'center'
  ctx.fillText('-60s', padding.left, height - 5)
  ctx.fillText('now', width - padding.right, height - 5)
}

function updateCharts() {
  if (cpuChart.value) {
    drawLineChart(cpuChart.value, cpuHistory.value, '#6366f1', 100, '%', 'rgba(99, 102, 241, 0.2)')
  }
  if (memoryChart.value) {
    drawLineChart(memoryChart.value, memoryHistory.value, '#ec4899', 100, '%', 'rgba(236, 72, 153, 0.2)')
  }
  if (clientsChart.value) {
    const maxClients = Math.max(10, ...clientsHistory.value) + 5
    drawBarChart(clientsChart.value, clientsHistory.value, '#22c55e', maxClients)
  }
  if (networkChart.value) {
    const maxNetwork = Math.max(1024 * 1024, ...networkInHistory.value, ...networkOutHistory.value) * 1.2
    drawDualLineChart(networkChart.value, networkInHistory.value, networkOutHistory.value, '#06b6d4', '#f59e0b', maxNetwork)
  }
}

async function loadStats() {
  const token = authStore.sessionToken
  if (!token) return

  try {
    const stats = await getServerStats(token)
    if (stats) {
      currentStats.value = stats

      // Update histories
      cpuHistory.value.push(stats.cpuUsagePercent)
      if (cpuHistory.value.length > MAX_HISTORY) cpuHistory.value.shift()

      const memPercent = (stats.memoryUsedBytes / stats.memoryTotalBytes) * 100
      memoryHistory.value.push(memPercent)
      if (memoryHistory.value.length > MAX_HISTORY) memoryHistory.value.shift()

      clientsHistory.value.push(stats.connectedClients)
      if (clientsHistory.value.length > MAX_HISTORY) clientsHistory.value.shift()

      // Use real network I/O data from server
      networkInHistory.value.push(stats.networkBytesInPerSec)
      networkOutHistory.value.push(stats.networkBytesOutPerSec)
      if (networkInHistory.value.length > MAX_HISTORY) networkInHistory.value.shift()
      if (networkOutHistory.value.length > MAX_HISTORY) networkOutHistory.value.shift()

      // Update charts
      nextTick(() => updateCharts())
    }
  } catch (e) {
    console.error('Failed to load server stats:', e)
  }
}

// Handle window resize
function handleResize() {
  nextTick(() => updateCharts())
}

onMounted(() => {
  // Initialize history arrays
  cpuHistory.value = Array(MAX_HISTORY).fill(0)
  memoryHistory.value = Array(MAX_HISTORY).fill(0)
  clientsHistory.value = Array(MAX_HISTORY).fill(0)
  networkInHistory.value = Array(MAX_HISTORY).fill(0)
  networkOutHistory.value = Array(MAX_HISTORY).fill(0)

  // Initial load
  loadStats()

  // Start polling every second for real-time feel
  statsInterval = setInterval(loadStats, 1000)

  // Handle resize
  window.addEventListener('resize', handleResize)
})

onUnmounted(() => {
  if (statsInterval) {
    clearInterval(statsInterval)
    statsInterval = null
  }
  window.removeEventListener('resize', handleResize)
})
</script>

<style scoped>
.server-stats-panel {
  padding: 0;
}

.chart-container {
  height: 180px;
  position: relative;
}

.chart-container canvas {
  width: 100% !important;
  height: 100% !important;
}
</style>
