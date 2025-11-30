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
  releaseNotes: string,
  isTestVersion: boolean = false
): Promise<{ success: boolean; version?: VersionInfo; error?: string }> {
  try {
    const formData = new FormData()
    formData.append('file', file)
    formData.append('releaseNotes', releaseNotes)
    formData.append('isTestVersion', isTestVersion.toString())

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
  networkBytesInPerSec: number
  networkBytesOutPerSec: number
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

// Database API

export interface DatabaseStats {
  totalClients: number
  flaggedClients: number
  blockedClients: number
  clientsSeenToday: number
  clientsSeenThisWeek: number
  totalConnections: number
  connectionsToday: number
  totalCommands: number
  commandsToday: number
}

export interface DatabaseClient {
  clientId: string
  clientType: string
  machineName: string
  username: string
  osVersion: string
  osArchitecture: string
  cpuCount: number
  totalMemoryBytes: number
  macAddress: string
  lastIpAddress: string
  countryCode: string
  isAdmin: boolean
  isUacEnabled: boolean
  clientVersion: string
  label: string | null
  firstSeenAt: string
  lastSeenAt: string
  totalConnections: number
  isFlagged: boolean
  isBlocked: boolean
  isTestMode: boolean
}

export interface ClientConnection {
  id: number
  ipAddress: string
  countryCode: string
  clientVersion: string
  connectedAt: string
  disconnectedAt: string | null
  durationSeconds: number | null
  disconnectReason: string | null
}

export interface DatabaseClientDetail extends DatabaseClient {
  connections: ClientConnection[]
}

export interface CommandLog {
  id: number
  commandId: string
  clientId: string | null
  commandType: string
  commandText: string
  sentAt: string
  wasDelivered: boolean
  resultReceivedAt: string | null
  success: boolean | null
  resultOutput: string | null
  errorMessage: string | null
  isBroadcast: boolean
  initiatorIp: string | null
}

export interface DatabaseInfo {
  filePath: string
  fileSizeBytes: number
  createdAt: string
  modifiedAt: string
}

export async function getDatabaseStats(token: string): Promise<DatabaseStats | null> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/stats`, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get database stats:', response.status)
      return null
    }

    return await response.json()
  } catch (error) {
    console.error('Get database stats failed:', error)
    return null
  }
}

export async function getDatabaseClients(
  token: string,
  options?: { flagged?: boolean; blocked?: boolean; country?: string; limit?: number }
): Promise<DatabaseClient[]> {
  try {
    const params = new URLSearchParams()
    if (options?.flagged !== undefined) params.append('flagged', options.flagged.toString())
    if (options?.blocked !== undefined) params.append('blocked', options.blocked.toString())
    if (options?.country) params.append('country', options.country)
    if (options?.limit) params.append('limit', options.limit.toString())

    const url = `${API_BASE_URL}/database/clients${params.toString() ? '?' + params.toString() : ''}`
    const response = await fetch(url, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get database clients:', response.status)
      return []
    }

    return await response.json()
  } catch (error) {
    console.error('Get database clients failed:', error)
    return []
  }
}

export async function getDatabaseClient(token: string, clientId: string): Promise<DatabaseClientDetail | null> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/clients/${encodeURIComponent(clientId)}`, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get database client:', response.status)
      return null
    }

    return await response.json()
  } catch (error) {
    console.error('Get database client failed:', error)
    return null
  }
}

export async function updateDatabaseClient(
  token: string,
  clientId: string,
  updates: { isFlagged?: boolean; isBlocked?: boolean; isTestMode?: boolean; label?: string }
): Promise<{ success: boolean; error?: string }> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/clients/${encodeURIComponent(clientId)}`, {
      method: 'PATCH',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify(updates),
    })

    if (!response.ok) {
      return { success: false, error: `Server error: ${response.status}` }
    }

    return { success: true }
  } catch (error) {
    console.error('Update database client failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to update client',
    }
  }
}

export async function deleteDatabaseClient(
  token: string,
  clientId: string
): Promise<{ success: boolean; error?: string }> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/clients/${encodeURIComponent(clientId)}`, {
      method: 'DELETE',
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      return { success: false, error: `Server error: ${response.status}` }
    }

    return { success: true }
  } catch (error) {
    console.error('Delete database client failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to delete client',
    }
  }
}

export interface BulkDeleteResult {
  deletedCount: number
  failedIds: string[]
}

export async function bulkDeleteDatabaseClients(
  token: string,
  clientIds: string[]
): Promise<{ success: boolean; result?: BulkDeleteResult; error?: string }> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/clients/bulk-delete`, {
      method: 'POST',
      headers: {
        'Authorization': `Bearer ${token}`,
        'Content-Type': 'application/json',
      },
      body: JSON.stringify({ clientIds }),
    })

    if (!response.ok) {
      return { success: false, error: `Server error: ${response.status}` }
    }

    const result = await response.json()
    return { success: true, result }
  } catch (error) {
    console.error('Bulk delete database clients failed:', error)
    return {
      success: false,
      error: error instanceof Error ? error.message : 'Failed to delete clients',
    }
  }
}

export async function getCommandLogs(
  token: string,
  options?: { clientId?: string; commandType?: string; limit?: number }
): Promise<CommandLog[]> {
  try {
    const params = new URLSearchParams()
    if (options?.clientId) params.append('clientId', options.clientId)
    if (options?.commandType) params.append('commandType', options.commandType)
    if (options?.limit) params.append('limit', options.limit.toString())

    const url = `${API_BASE_URL}/database/commands${params.toString() ? '?' + params.toString() : ''}`
    const response = await fetch(url, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get command logs:', response.status)
      return []
    }

    return await response.json()
  } catch (error) {
    console.error('Get command logs failed:', error)
    return []
  }
}

export async function getDatabaseInfo(token: string): Promise<DatabaseInfo | null> {
  try {
    const response = await fetch(`${API_BASE_URL}/database/info`, {
      headers: {
        'Authorization': `Bearer ${token}`,
      },
    })

    if (!response.ok) {
      console.error('Failed to get database info:', response.status)
      return null
    }

    return await response.json()
  } catch (error) {
    console.error('Get database info failed:', error)
    return null
  }
}

export function getDatabaseBackupUrl(): string {
  return `${API_BASE_URL}/database/backup`
}
