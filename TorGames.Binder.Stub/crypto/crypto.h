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