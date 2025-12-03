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