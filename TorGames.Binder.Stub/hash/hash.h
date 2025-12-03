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