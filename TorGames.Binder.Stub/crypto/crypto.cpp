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