export interface Client {
  id: string
  clientId: string
  version: string
  location: string
  ipAddress: string
  machineName: string
  operatingSystem: string
  account: string
  cpus: number
  type: 'Desktop' | 'Laptop' | 'Server' | 'New Bot'
  firstSeen: string
  lastSeen: string
  status: 'online' | 'offline'
}
