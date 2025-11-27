const API_BASE_URL = 'http://localhost:5001/api'

export interface LoginResponse {
  success: boolean
  token?: string
  error?: string
  expiresAt?: string
}

export interface ValidateTokenResponse {
  valid: boolean
  expiresAt?: string
}

export async function login(password: string): Promise<LoginResponse> {
  try {
    const response = await fetch(`${API_BASE_URL}/auth/login`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ password }),
    })

    if (!response.ok) {
      return {
        success: false,
        error: `Server error: ${response.status}`,
      }
    }

    return await response.json()
  } catch (error) {
    console.error('Login request failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to connect to server',
    }
  }
}

export async function validateToken(token: string): Promise<ValidateTokenResponse> {
  try {
    const response = await fetch(`${API_BASE_URL}/auth/validate`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      return { valid: false }
    }

    return await response.json()
  } catch (error) {
    console.error('Token validation failed:', error)
    return { valid: false }
  }
}

export function getApiBaseUrl(): string {
  return API_BASE_URL
}
