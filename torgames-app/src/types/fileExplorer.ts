// File Explorer types for remote file browsing

export interface FileEntry {
  name: string
  fullPath: string
  sizeBytes: number
  type: 'file' | 'directory' | 'drive'
  extension: string
  createdTime: string      // ISO date string
  modifiedTime: string     // ISO date string
  isDirectory: boolean
  isReadOnly: boolean
  isHidden: boolean
  isSystem: boolean
}

export interface DirectoryListing {
  path: string
  entries: FileEntry[]
  success: boolean
  errorMessage: string | null
}

export interface FileOperationResult {
  success: boolean
  errorMessage: string | null
  newPath?: string         // For rename/move operations
}

// Tree node for v-treeview
export interface FileTreeNode extends FileEntry {
  children?: FileTreeNode[]
}
