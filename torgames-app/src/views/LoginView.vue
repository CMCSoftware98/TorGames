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
    <v-main class="d-flex align-center justify-center login-background">
      <!-- Decorative Elements -->
      <div class="glow-orb orb-1"></div>
      <div class="glow-orb orb-2"></div>

      <v-card
        class="glass-panel pa-8 rounded-xl"
        width="420"
        elevation="0"
      >
        <div class="text-center mb-8">
          <v-avatar color="primary" size="64" class="mb-4 elevation-6">
            <v-icon size="32" color="white">mdi-shield-lock</v-icon>
          </v-avatar>
          <h1 class="text-h4 font-weight-bold text-white mb-2">TorGames</h1>
          <div class="text-subtitle-1 text-medium-emphasis">Secure Control Panel</div>
        </div>

        <v-form @submit.prevent="handleSubmit">
          <v-text-field
            v-model="password"
            :type="showPassword ? 'text' : 'password'"
            :append-inner-icon="showPassword ? 'mdi-eye-off' : 'mdi-eye'"
            @click:append-inner="showPassword = !showPassword"
            label="Master Password"
            variant="outlined"
            bg-color="rgba(255,255,255,0.05)"
            :disabled="isFormDisabled"
            :error="!!errorMessage"
            autofocus
            class="mb-2"
            rounded="lg"
          >
            <template #prepend-inner>
              <v-icon color="primary" class="mr-2">mdi-key</v-icon>
            </template>
          </v-text-field>

          <v-expand-transition>
            <div v-if="errorMessage">
              <v-alert
                type="error"
                variant="tonal"
                density="compact"
                class="mb-4 rounded-lg"
                icon="mdi-alert-circle"
              >
                {{ errorMessage }}
              </v-alert>
            </div>
          </v-expand-transition>

          <v-expand-transition>
            <div v-if="!authStore.isLockedOut && authStore.loginAttempts > 0">
              <v-alert
                type="warning"
                variant="tonal"
                density="compact"
                class="mb-4 rounded-lg"
                icon="mdi-shield-alert"
              >
                {{ authStore.attemptsRemaining }} attempts remaining
              </v-alert>
            </div>
          </v-expand-transition>

          <v-btn
            type="submit"
            color="primary"
            size="large"
            block
            :loading="isLoading"
            :disabled="isFormDisabled || !password"
            class="mt-4 rounded-lg font-weight-bold"
            elevation="4"
            height="48"
          >
            {{ buttonText }}
          </v-btn>
        </v-form>

        <div class="text-center mt-6 text-caption text-disabled">
          Protected by TorGames Security System
        </div>
      </v-card>
    </v-main>
  </v-app>
</template>

<style scoped>
.login-background {
  background: radial-gradient(circle at top right, #1e1b4b, #0f172a);
  position: relative;
  overflow: hidden;
}

.glow-orb {
  position: absolute;
  border-radius: 50%;
  filter: blur(80px);
  opacity: 0.4;
  z-index: 0;
}

.orb-1 {
  width: 300px;
  height: 300px;
  background: #6366f1;
  top: 10%;
  left: 20%;
  animation: float 10s infinite ease-in-out;
}

.orb-2 {
  width: 250px;
  height: 250px;
  background: #ec4899;
  bottom: 15%;
  right: 20%;
  animation: float 12s infinite ease-in-out reverse;
}

@keyframes float {
  0% { transform: translate(0, 0); }
  50% { transform: translate(30px, -30px); }
  100% { transform: translate(0, 0); }
}

.v-text-field :deep(.v-field__outline__start) {
  border-radius: 8px 0 0 8px !important;
}

.v-text-field :deep(.v-field__outline__end) {
  border-radius: 0 8px 8px 0 !important;
}
</style>
