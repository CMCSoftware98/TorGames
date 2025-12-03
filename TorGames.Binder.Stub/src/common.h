// ============================================================================
// File: src/core/common.h (Binder) / src/common.h (Stub)
// Purpose: Shared data structures and constants
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <shellapi.h>     // For SHELLEXECUTEINFOA, ShellExecuteExA
#include <shlwapi.h>      // For path utilities

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "shlwapi.lib")

#include <string>
#include <vector>
#include <cstdint>

// Resource type identifiers
#define RT_BINDER_CONFIG    256
#define RT_BINDER_FILE      257
#define RT_BINDER_KEY       258  // NEW: Encrypted key storage

// Configuration version (increment when format changes)
#define CONFIG_VERSION      2

// Encryption constants
#define AES_KEY_SIZE        32   // 256 bits
#define AES_IV_SIZE         12   // 96 bits for GCM
#define AES_TAG_SIZE        16   // 128-bit authentication tag
#define HASH_SIZE           32   // SHA-256 output

// Compression constants
#define COMPRESSION_NONE    0
#define COMPRESSION_LZ4     1

// Encryption constants
#define ENCRYPTION_NONE     0
#define ENCRYPTION_AES256   1

// Per-file configuration
struct BoundFileConfig {
    std::string filename;           // Filename (without path)
    std::string fullPath;           // Full path to source (Binder only)
    int executionOrder;             // Execution sequence number
    bool executeFile;               // Whether to execute this file
    bool waitForExit;               // Wait for process to complete
    bool runHidden;                 // Hide window during execution
    bool runAsAdmin;                // Request admin elevation
    std::string extractPath;        // Custom extraction path (empty = temp)
    std::string commandLineArgs;    // Command line arguments
    int executionDelay;             // Delay before execution (ms)
    bool deleteAfterExecution;      // Delete file after running
    DWORD fileSize;                 // Original file size
    DWORD compressedSize;           // Compressed size (NEW)
    std::string sha256Hash;         // SHA-256 hash of original file (NEW)
};

// Global binder configuration
struct BinderConfig {
    int configVersion;              // Config format version
    bool requireAdmin;              // Require admin for entire stub
    int compressionType;            // COMPRESSION_NONE or COMPRESSION_LZ4 (NEW)
    int encryptionType;             // ENCRYPTION_NONE or ENCRYPTION_AES256 (NEW)
    std::vector<BoundFileConfig> files;
};

// Encrypted data header (prepended to encrypted data)
struct EncryptedDataHeader {
    uint8_t iv[AES_IV_SIZE];        // Initialization vector
    uint8_t tag[AES_TAG_SIZE];      // Authentication tag
    uint32_t originalSize;          // Size before encryption
    uint32_t encryptedSize;         // Size after encryption
};

// Key storage structure (obfuscated in stub)
struct KeyStorage {
    uint8_t obfuscatedKey[AES_KEY_SIZE];  // XOR-obfuscated key
    uint8_t obfuscationMask[AES_KEY_SIZE]; // XOR mask
};