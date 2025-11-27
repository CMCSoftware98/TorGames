<script setup lang="ts">
import { ref, computed, onMounted, onUnmounted } from 'vue'
import { useRouter } from 'vue-router'
import { useAuthStore } from '@/stores/auth'

const router = useRouter()
const authStore = useAuthStore()

const password = ref('')
const showPassword = ref(false)
const isLoading = ref(false)
const errorMessage = ref('')
const lockoutTimer = ref<ReturnType<typeof setInterval> | null>(null)
const lockoutCountdown = ref(0)

const isFormDisabled = computed(() => {
  return isLoading.value || authStore.isLockedOut
})

const buttonText = computed(() => {
  if (isLoading.value) return 'Authenticating...'
  if (authStore.isLockedOut) return `Locked (${lockoutCountdown.value}s)`
  return 'Login'
})

async function handleSubmit() {
  if (isFormDisabled.value || !password.value) return

  errorMessage.value = ''
  isLoading.value = true

  try {
    const result = await authStore.login(password.value)

    if (result.success) {
      router.push({ name: 'dashboard' })
    } else {
      errorMessage.value = result.error || 'Authentication failed'
      password.value = ''

      if (result.lockoutSeconds) {
        startLockoutTimer()
      }
    }
  } catch (error) {
    errorMessage.value = 'An unexpected error occurred'
    console.error('Login error:', error)
  } finally {
    isLoading.value = false
  }
}

function startLockoutTimer() {
  lockoutCountdown.value = authStore.lockoutRemainingSeconds

  if (lockoutTimer.value) {
    clearInterval(lockoutTimer.value)
  }

  lockoutTimer.value = setInterval(() => {
    lockoutCountdown.value = authStore.lockoutRemainingSeconds
    if (lockoutCountdown.value <= 0) {
      clearInterval(lockoutTimer.value!)
      lockoutTimer.value = null
      errorMessage.value = ''
    }
  }, 1000)
}

onMounted(() => {
  // Check if already locked out on mount
  if (authStore.isLockedOut) {
    startLockoutTimer()
  }
})

onUnmounted(() => {
  if (lockoutTimer.value) {
    clearInterval(lockoutTimer.value)
  }
})
</script>

<template>
  <v-app>
    <v-main class="d-flex align-center justify-center" style="min-height: 100vh;">
      <v-card
        class="pa-8"
        width="400"
        elevation="8"
      >
        <v-card-title class="text-center text-h4 mb-4">
          <v-icon size="48" color="primary" class="mr-2">mdi-shield-lock</v-icon>
          TorGames
        </v-card-title>

        <v-card-subtitle class="text-center mb-6">
          Enter your master password to continue
        </v-card-subtitle>

        <v-form @submit.prevent="handleSubmit">
          <v-text-field
            v-model="password"
            :type="showPassword ? 'text' : 'password'"
            :append-inner-icon="showPassword ? 'mdi-eye-off' : 'mdi-eye'"
            @click:append-inner="showPassword = !showPassword"
            label="Master Password"
            variant="outlined"
            :disabled="isFormDisabled"
            :error="!!errorMessage"
            autofocus
            class="mb-4"
          />

          <v-alert
            v-if="errorMessage"
            type="error"
            variant="tonal"
            density="compact"
            class="mb-4"
          >
            {{ errorMessage }}
          </v-alert>

          <v-alert
            v-if="!authStore.isLockedOut && authStore.loginAttempts > 0"
            type="warning"
            variant="tonal"
            density="compact"
            class="mb-4"
          >
            {{ authStore.attemptsRemaining }} attempts remaining
          </v-alert>

          <v-btn
            type="submit"
            color="primary"
            size="large"
            block
            :loading="isLoading"
            :disabled="isFormDisabled || !password"
          >
            {{ buttonText }}
          </v-btn>
        </v-form>
      </v-card>
    </v-main>
  </v-app>
</template>
