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
    std::vector<uint8_t> fileData = Utils::LoadFileToMemory(filePath.c_str());
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

void Builder::SetConfig(const BinderConfig& config) {
    m_config = config;
}

std::string Builder::GetLastError() const {
    return m_lastError;
}

std::string Builder::GetStubPath() const {
    return Utils::GetExecutableDirectory() + "\\TorGames.Binder.Stub.exe";
}

void Builder::ReportProgress(int percent, const std::string& status) {
    if (m_progressCallback) {
        m_progressCallback(percent, status);
    }
}

bool Builder::CopyStub(const std::string& outputPath) {
    std::string stubPath = GetStubPath();

    if (!CopyFileA(stubPath.c_str(), outputPath.c_str(), FALSE)) {
        m_lastError = "Failed to copy stub executable from: " + stubPath;
        return false;
    }

    return true;
}

bool Builder::UpdateManifest(const std::string& outputPath, bool requireAdmin) {
    if (!requireAdmin) {
        return true;  // Nothing to update if admin not required
    }

    // Update the manifest to require admin elevation
    HANDLE hUpdate = BeginUpdateResourceA(outputPath.c_str(), FALSE);
    if (!hUpdate) {
        m_lastError = "Failed to begin resource update for manifest";
        return false;
    }

    // Admin manifest XML
    const char* adminManifest =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\r\n"
        "<assembly xmlns=\"urn:schemas-microsoft-com:asm.v1\" manifestVersion=\"1.0\">\r\n"
        "  <trustInfo xmlns=\"urn:schemas-microsoft-com:asm.v3\">\r\n"
        "    <security>\r\n"
        "      <requestedPrivileges>\r\n"
        "        <requestedExecutionLevel level=\"requireAdministrator\" uiAccess=\"false\"/>\r\n"
        "      </requestedPrivileges>\r\n"
        "    </security>\r\n"
        "  </trustInfo>\r\n"
        "</assembly>\r\n";

    BOOL result = UpdateResourceA(
        hUpdate,
        RT_MANIFEST,
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPVOID)adminManifest,
        (DWORD)strlen(adminManifest)
    );

    if (!result) {
        EndUpdateResourceA(hUpdate, TRUE);
        m_lastError = "Failed to update manifest resource";
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        m_lastError = "Failed to finalize manifest resource update";
        return false;
    }

    return true;
}

Builder::BuildStats Builder::GetBuildStats() const {
    return m_stats;
}

void Builder::SetProgressCallback(ProgressCallback callback) {
    m_progressCallback = callback;
}