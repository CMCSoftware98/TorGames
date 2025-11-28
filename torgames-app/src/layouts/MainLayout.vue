<script setup lang="ts">
import { ref, computed } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'
import { useSettingsStore } from '@/stores/settings'
import { useClientsStore } from '@/stores/clients'

const router = useRouter()
const authStore = useAuthStore()
const settingsStore = useSettingsStore()
const clientsStore = useClientsStore()

const drawer = ref(true)
const rail = ref(false)

const isConnected = computed(() => clientsStore.isConnected)
const isConnecting = computed(() => clientsStore.isConnecting)
const isReconnecting = computed(() => clientsStore.isReconnecting)

const menuItems = [
  { title: 'Machine List', icon: 'mdi-monitor-dashboard', value: 'clients', route: 'dashboard' },
  { title: 'Command & Control', icon: 'mdi-console-line', value: 'command', route: 'command' },
  { title: 'Station Config', icon: 'mdi-cog-box', value: 'config', route: 'config' },
  { title: 'Saved Data', icon: 'mdi-database-outline', value: 'data', route: 'data' },
  { title: 'Server Manager', icon: 'mdi-server-network', value: 'server', route: 'server' },
]



defineProps<{
  activeTab: string
}>()

const emit = defineEmits<{
  (e: 'update:activeTab', value: string): void
}>()

function handleNavigation(itemValue: string) {
  emit('update:activeTab', itemValue)
}

async function handleLogout() {
  await clientsStore.disconnect()
  await authStore.logout()
  router.push({ name: 'login' })
}

function toggleNotifications() {
  settingsStore.setNotificationsEnabled(!settingsStore.notificationsEnabled)
}
</script>

<template>
  <v-layout class="rounded rounded-md fill-height">
    <!-- Glass Sidebar -->
    <v-navigation-drawer
      v-model="drawer"
      :rail="rail"
      permanent
      class="glass-panel border-none"
      width="280"
      rail-width="80"
    >
      <div class="d-flex flex-column fill-height">
        <!-- Logo Area -->
        <div class="pa-6 d-flex align-center">
          <v-avatar color="primary" size="40" class="mr-3 elevation-4">
            <v-icon icon="mdi-shield-lock" color="white"></v-icon>
          </v-avatar>
          <div v-if="!rail" class="text-h6 font-weight-bold text-white">
            TorGames
            <div class="text-caption text-medium-emphasis">Control Panel</div>
          </div>
        </div>

        <v-divider class="border-opacity-25 mx-4"></v-divider>

        <!-- Navigation Items -->
        <v-list class="pa-3 bg-transparent">
          <v-list-item
            v-for="item in menuItems"
            :key="item.value"
            :value="item.value"
            :active="activeTab === item.value"
            @click="handleNavigation(item.value)"
            rounded="lg"
            class="mb-2 nav-item"
            :class="{ 'active-nav-item': activeTab === item.value }"
            color="primary"
          >
            <template v-slot:prepend>
              <v-icon :icon="item.icon" :class="activeTab === item.value ? 'text-primary' : 'text-medium-emphasis'"></v-icon>
            </template>
            <v-list-item-title :class="activeTab === item.value ? 'font-weight-bold text-white' : 'text-medium-emphasis'">
              {{ item.title }}
            </v-list-item-title>
          </v-list-item>
        </v-list>

        <v-spacer></v-spacer>

        <!-- Bottom Actions -->
        <div class="pa-3">
          <v-list class="bg-transparent">
             <v-list-item
              @click="rail = !rail"
              rounded="lg"
              class="mb-2 nav-item"
            >
              <template v-slot:prepend>
                <v-icon :icon="rail ? 'mdi-chevron-right' : 'mdi-chevron-left'"></v-icon>
              </template>
              <v-list-item-title>Collapse</v-list-item-title>
            </v-list-item>

            <v-list-item
              @click="handleLogout"
              rounded="lg"
              class="nav-item text-error"
            >
              <template v-slot:prepend>
                <v-icon icon="mdi-logout" color="error"></v-icon>
              </template>
              <v-list-item-title class="text-error">Logout</v-list-item-title>
            </v-list-item>
          </v-list>
        </div>
      </div>
    </v-navigation-drawer>

    <!-- Main Content Area -->
    <v-main class="fill-height bg-transparent">
      <div class="d-flex flex-column fill-height">
        <!-- Top Bar -->
        <div class="px-6 py-4 d-flex align-center justify-space-between">
          <div>
            <h1 class="text-h4 font-weight-bold text-white mb-1">
              {{ menuItems.find(i => i.value === activeTab)?.title }}
            </h1>
            <div class="d-flex align-center">
              <v-chip
                :color="isConnected ? 'success' : (isConnecting || isReconnecting) ? 'warning' : 'error'"
                size="small"
                variant="flat"
                class="font-weight-medium"
              >
                <v-icon start size="small" class="mr-1">
                  {{ isConnected ? 'mdi-wifi' : (isConnecting || isReconnecting) ? 'mdi-wifi-strength-1' : 'mdi-wifi-off' }}
                </v-icon>
                {{ isConnected ? 'System Online' : isReconnecting ? 'Reconnecting...' : isConnecting ? 'Connecting...' : 'Offline' }}
              </v-chip>
              <span v-if="isConnected" class="ml-3 text-caption text-medium-emphasis">
                {{ clientsStore.onlineCount }} Clients Connected
              </span>
            </div>
          </div>

          <div class="d-flex align-center gap-2">
             <v-btn
              :icon="settingsStore.notificationsEnabled ? 'mdi-bell-ring' : 'mdi-bell-off'"
              variant="text"
              :color="settingsStore.notificationsEnabled ? 'primary' : 'medium-emphasis'"
              class="glass-card mr-2"
              @click="toggleNotifications"
            >
            </v-btn>
            <v-avatar color="surface-variant" size="40" class="glass-card cursor-pointer">
              <span class="text-h6">A</span>
            </v-avatar>
          </div>
        </div>

        <!-- Content Slot -->
        <div class="flex-grow-1 px-6 pb-6" style="overflow: hidden;">
          <div class="fill-height glass-panel rounded-xl pa-6" style="overflow: hidden;">
            <slot></slot>
          </div>
        </div>
      </div>
    </v-main>
  </v-layout>
</template>

<style scoped>
.nav-item {
  transition: all 0.2s ease;
}

.nav-item:hover {
  background: rgba(255, 255, 255, 0.05);
}

.active-nav-item {
  background: rgba(99, 102, 241, 0.15) !important;
  border: 1px solid rgba(99, 102, 241, 0.3);
}

:deep(.v-navigation-drawer__content) {
  overflow-y: hidden;
}
</style>
