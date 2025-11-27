export interface AuthState {
  isAuthenticated: boolean
  lastLoginTime: number | null
  loginAttempts: number
  lockoutUntil: number | null
}

export interface StoredAuthData {
  sessionToken: string | null // Future: server-provided token
  lastLoginTime: number
}

export interface LoginResult {
  success: boolean
  error?: string
  lockoutSeconds?: number
}

export interface AuthConfig {
  maxAttempts: number
  lockoutDurationSeconds: number
  sessionDurationMs: number
}
