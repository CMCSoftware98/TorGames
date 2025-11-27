import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { Store } from '@tauri-apps/plugin-store'
import { login as apiLogin, validateToken as apiValidateToken } from '@/services/api'
import type { StoredAuthData, LoginResult, AuthConfig } from '@/types/auth'

const AUTH_STORE_PATH = 'auth.json'

const CONFIG: AuthConfig = {
  maxAttempts: 5,
  lockoutDurationSeconds: 30,
  sessionDurationMs: 24 * 60 * 60 * 1000 // 24 hours
}

export const useAuthStore = defineStore('auth', () => {
  // State
  const isAuthenticated = ref(false)
  const lastLoginTime = ref<number | null>(null)
  const loginAttempts = ref(0)
  const lockoutUntil = ref<number | null>(null)
  const isInitialized = ref(false)
  const sessionToken = ref<string | null>(null)

  // Store instance (lazy loaded)
  let store: Store | null = null

  const getStore = async (): Promise<Store> => {
    if (!store) {
      store = await Store.load(AUTH_STORE_PATH)
    }
    return store
  }

  // Computed
  const isLockedOut = computed(() => {
    if (!lockoutUntil.value) return false
    return Date.now() < lockoutUntil.value
  })

  const lockoutRemainingSeconds = computed(() => {
    if (!lockoutUntil.value) return 0
    const remaining = Math.ceil((lockoutUntil.value - Date.now()) / 1000)
    return Math.max(0, remaining)
  })

  const attemptsRemaining = computed(() => {
    return Math.max(0, CONFIG.maxAttempts - loginAttempts.value)
  })

  // Actions
  async function initialize(): Promise<void> {
    if (isInitialized.value) return

    try {
      const authStore = await getStore()
      const storedData = await authStore.get<StoredAuthData>('session')

      if (storedData && storedData.sessionToken && storedData.lastLoginTime) {
        // Validate token with server
        const validation = await apiValidateToken(storedData.sessionToken)

        if (validation.valid) {
          // Valid session exists
          isAuthenticated.value = true
          lastLoginTime.value = storedData.lastLoginTime
          sessionToken.value = storedData.sessionToken
        } else {
          // Token expired or invalid, clear it
          await clearSession()
        }
      }
    } catch (error) {
      console.error('Failed to initialize auth store:', error)
      // If server is unreachable, check local session validity
      try {
        const authStore = await getStore()
        const storedData = await authStore.get<StoredAuthData>('session')
        if (storedData && storedData.lastLoginTime) {
          const sessionAge = Date.now() - storedData.lastLoginTime
          if (sessionAge < CONFIG.sessionDurationMs && storedData.sessionToken) {
            // Fallback to local validation if server is down
            isAuthenticated.value = true
            lastLoginTime.value = storedData.lastLoginTime
            sessionToken.value = storedData.sessionToken
          }
        }
      } catch {
        // Ignore fallback errors
      }
    }

    isInitialized.value = true
  }

  async function login(password: string): Promise<LoginResult> {
    // Check lockout
    if (isLockedOut.value) {
      return {
        success: false,
        error: 'Too many failed attempts',
        lockoutSeconds: lockoutRemainingSeconds.value
      }
    }

    // Call server API for authentication
    const response = await apiLogin(password)

    if (response.success && response.token) {
      // Success
      loginAttempts.value = 0
      lockoutUntil.value = null
      isAuthenticated.value = true
      lastLoginTime.value = Date.now()
      sessionToken.value = response.token

      // Persist session
      try {
        const authStore = await getStore()
        await authStore.set('session', {
          sessionToken: response.token,
          lastLoginTime: lastLoginTime.value
        } as StoredAuthData)
        await authStore.save()
      } catch (error) {
        console.error('Failed to persist session:', error)
      }

      return { success: true }
    } else {
      // Failed attempt
      loginAttempts.value++

      if (loginAttempts.value >= CONFIG.maxAttempts) {
        lockoutUntil.value = Date.now() + (CONFIG.lockoutDurationSeconds * 1000)
        return {
          success: false,
          error: 'Account locked due to too many failed attempts',
          lockoutSeconds: CONFIG.lockoutDurationSeconds
        }
      }

      return {
        success: false,
        error: response.error || `Invalid password. ${attemptsRemaining.value} attempts remaining.`
      }
    }
  }

  async function logout(): Promise<void> {
    await clearSession()
    isAuthenticated.value = false
    lastLoginTime.value = null
    sessionToken.value = null
  }

  async function clearSession(): Promise<void> {
    try {
      const authStore = await getStore()
      await authStore.delete('session')
      await authStore.save()
    } catch (error) {
      console.error('Failed to clear session:', error)
    }
  }

  function resetLockout(): void {
    // For admin/testing purposes
    loginAttempts.value = 0
    lockoutUntil.value = null
  }

  function getToken(): string | null {
    return sessionToken.value
  }

  return {
    // State
    isAuthenticated,
    lastLoginTime,
    loginAttempts,
    lockoutUntil,
    isInitialized,
    sessionToken,
    // Computed
    isLockedOut,
    lockoutRemainingSeconds,
    attemptsRemaining,
    // Actions
    initialize,
    login,
    logout,
    resetLockout,
    getToken
  }
})
