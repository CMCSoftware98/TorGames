const API_BASE_URL = 'http://144.91.111.101:5001/api'

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

// Version management API functions
import type { VersionInfo } from '@/types/version'

export async function getVersions(token: string): Promise<VersionInfo[]> {
  try {
    const response = await fetch(`${API_BASE_URL}/update/versions`, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get versions:', response.status)
      return []
    }

    return await response.json()
  } catch (error) {
    console.error('Get versions failed:', error)
    return []
  }
}

export async function uploadVersion(
  token: string,
  file: File,
  releaseNotes: string
): Promise<{ success: boolean; version?: VersionInfo; error?: string }> {
  try {
    const formData = new FormData()
    formData.append('file', file)
    formData.append('releaseNotes', releaseNotes)

    const response = await fetch(`${API_BASE_URL}/update/upload`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
      },
      body: formData,
    })

    if (!response.ok) {
      const errorText = await response.text()
      return { success: false, error: errorText || `Server error: ${response.status}` }
    }

    const versionInfo = await response.json()
    return { success: true, version: versionInfo }
  } catch (error) {
    console.error('Upload version failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to upload version',
    }
  }
}

export async function deleteVersion(
  token: string,
  version: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const response = await fetch(`${API_BASE_URL}/update/${version}`, {
      method: 'DELETE',
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      const errorText = await response.text()
      return { success: false, error: errorText || `Server error: ${response.status}` }
    }

    return { success: true }
  } catch (error) {
    console.error('Delete version failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to delete version',
    }
  }
}

export function getVersionDownloadUrl(version: string): string {
  return `${API_BASE_URL}/update/download/${version}`
}
