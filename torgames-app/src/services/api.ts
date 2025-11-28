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

// Server stats API
export interface ServerStats {
  cpuUsagePercent: number
  memoryUsedBytes: number
  memoryTotalBytes: number
  diskUsedBytes: number
  diskTotalBytes: number
  connectedClients: number
  totalClients: number
  uptimeSeconds: number
  timestamp: string
}

export async function getServerStats(token: string): Promise<ServerStats | null> {
  try {
    const response = await fetch(`${API_BASE_URL}/serverstats`, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get server stats:', response.status)
      return null
    }

    return await response.json()
  } catch (error) {
    console.error('Get server stats failed:', error)
    return null
  }
}

// IP Geolocation API
export interface GeoLocation {
  country: string
  countryCode: string
  city: string
  region: string
}

export async function getIpGeolocation(ip: string): Promise<GeoLocation | null> {
  try {
    // Use ip-api.com (free, no API key needed, 45 req/min limit)
    const response = await fetch(`http://ip-api.com/json/${ip}?fields=status,country,countryCode,regionName,city`)

    if (!response.ok) {
      return null
    }

    const data = await response.json()

    if (data.status !== 'success') {
      return null
    }

    return {
      country: data.country,
      countryCode: data.countryCode,
      city: data.city,
      region: data.regionName
    }
  } catch (error) {
    console.error('Geolocation lookup failed:', error)
    return null
  }
}
