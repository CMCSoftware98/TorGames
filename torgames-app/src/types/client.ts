// Client DTO from server
export interface ClientDto {
  connectionKey: string
  clientId: string
  clientType: string
  machineName: string
  osVersion: string
  osArchitecture: string
  cpuCount: number
  totalMemoryBytes: number
  username: string
  clientVersion: string
  ipAddress: string
  macAddress: string
  isAdmin: boolean
  countryCode: string
  isUacEnabled: boolean
  connectedAt: string
  lastHeartbeat: string
  isOnline: boolean
  cpuUsagePercent: number
  availableMemoryBytes: number
  uptimeSeconds: number
}

// Command result from client
export interface CommandResultDto {
  connectionKey: string
  commandId: string
  success: boolean
  exitCode: number
  stdout: string
  stderr: string
  errorMessage: string
}

// Request to execute a command
export interface ExecuteCommandRequest {
  connectionKey: string
  commandType: string
  commandText: string
  timeoutSeconds: number
}

// Connection stats
export interface ConnectionStats {
  totalConnected: number
  onlineCount: number
}

// Detailed system information
export interface DetailedSystemInfoDto {
  cpu: CpuInfoDto | null
  memory: MemoryInfoDto | null
  disks: DiskInfoDto[]
  networkAdapters: NetworkAdapterInfoDto[]
  gpus: GpuInfoDto[]
  os: OsInfoDto | null
  hardware: SystemHardwareInfoDto | null
  performance: PerformanceInfoDto | null
}

export interface CpuInfoDto {
  name: string
  manufacturer: string
  cores: number
  logicalProcessors: number
  maxClockSpeedMhz: number
  currentClockSpeedMhz: number
  architecture: string
  l2CacheKb: number
  l3CacheKb: number
}

export interface MemoryInfoDto {
  totalPhysicalBytes: number
  availablePhysicalBytes: number
  totalVirtualBytes: number
  availableVirtualBytes: number
  memoryLoadPercent: number
  slotCount: number
  speedMhz: number
  memoryType: string
}

export interface DiskInfoDto {
  name: string
  driveLetter: string
  volumeLabel: string
  fileSystem: string
  totalBytes: number
  freeBytes: number
  driveType: string
  isSystemDrive: boolean
}

export interface NetworkAdapterInfoDto {
  name: string
  description: string
  macAddress: string
  ipAddress: string
  subnetMask: string
  defaultGateway: string
  dnsServers: string
  speedBps: number
  status: string
  adapterType: string
  isDhcpEnabled: boolean
}

export interface GpuInfoDto {
  name: string
  manufacturer: string
  videoMemoryBytes: number
  driverVersion: string
  currentRefreshRate: number
  videoProcessor: string
  resolution: string
}

export interface OsInfoDto {
  name: string
  version: string
  buildNumber: string
  architecture: string
  installDate: string
  lastBootTime: string
  serialNumber: string
  registeredUser: string
  systemDirectory: string
  timezoneOffsetMinutes: number
  timezoneName: string
}

export interface SystemHardwareInfoDto {
  manufacturer: string
  model: string
  systemType: string
  biosVersion: string
  biosManufacturer: string
  biosReleaseDate: string
  motherboardManufacturer: string
  motherboardProduct: string
  motherboardSerial: string
  uuid: string
}

export interface PerformanceInfoDto {
  cpuUsagePercent: number
  availableMemoryBytes: number
  uptimeSeconds: number
  processCount: number
  threadCount: number
  handleCount: number
  topProcesses: ProcessInfoDto[]
}

export interface ProcessInfoDto {
  pid: number
  name: string
  cpuPercent: number
  memoryBytes: number
}
