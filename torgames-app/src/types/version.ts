// Version information from server
export interface VersionInfo {
  version: string
  sha256: string
  fileSize: number
  uploadedAt: string
  releaseNotes: string
  uploadedBy: string
  isTestVersion: boolean
}

// Response from update check endpoint
export interface UpdateCheckResponse {
  updateAvailable: boolean
  latestVersion: VersionInfo | null
}
