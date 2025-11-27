import { defineStore } from 'pinia'
import { ref } from 'vue'
import { Store } from '@tauri-apps/plugin-store'
import {
  isPermissionGranted,
  requestPermission,
  sendNotification
} from '@tauri-apps/plugin-notification'

const SETTINGS_STORE_PATH = 'settings.json'

interface AppSettings {
  notificationsEnabled: boolean
}

export const useSettingsStore = defineStore('settings', () => {
  // State
  const notificationsEnabled = ref(true)
  const notificationPermissionGranted = ref(false)
  const isInitialized = ref(false)

  // Store instance (lazy loaded)
  let store: Store | null = null

  const getStore = async (): Promise<Store> => {
    if (!store) {
      store = await Store.load(SETTINGS_STORE_PATH)
    }
    return store
  }

  // Actions
  async function initialize(): Promise<void> {
    if (isInitialized.value) return

    try {
      // Load saved settings
      const settingsStore = await getStore()
      const saved = await settingsStore.get<AppSettings>('app')

      if (saved) {
        notificationsEnabled.value = saved.notificationsEnabled ?? true
      }

      // Check notification permission
      notificationPermissionGranted.value = await isPermissionGranted()
    } catch (error) {
      console.error('Failed to initialize settings:', error)
    }

    isInitialized.value = true
  }

  async function setNotificationsEnabled(enabled: boolean): Promise<void> {
    // If enabling and permission not granted, request it
    if (enabled && !notificationPermissionGranted.value) {
      const permission = await requestPermission()
      notificationPermissionGranted.value = permission === 'granted'

      if (!notificationPermissionGranted.value) {
        console.warn('Notification permission denied')
        return
      }
    }

    notificationsEnabled.value = enabled

    // Save to store
    try {
      const settingsStore = await getStore()
      await settingsStore.set('app', {
        notificationsEnabled: enabled
      } as AppSettings)
      await settingsStore.save()
    } catch (error) {
      console.error('Failed to save settings:', error)
    }
  }

  async function showNotification(title: string, body: string): Promise<void> {
    if (!notificationsEnabled.value) {
      return
    }

    if (!notificationPermissionGranted.value) {
      const permission = await requestPermission()
      notificationPermissionGranted.value = permission === 'granted'

      if (!notificationPermissionGranted.value) {
        return
      }
    }

    try {
      sendNotification({
        title,
        body
      })
    } catch (error) {
      console.error('Failed to send notification:', error)
    }
  }

  return {
    // State
    notificationsEnabled,
    notificationPermissionGranted,
    isInitialized,
    // Actions
    initialize,
    setNotificationsEnabled,
    showNotification
  }
})
