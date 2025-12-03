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

    // Get stub executable path
    std::string GetStubPath() const;

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
    bool EmbedFileData(const std::string& outputPath, const std::vector<uint8_t>& data, int resourceId);
    bool UpdateIcon(const std::string& outputPath, const std::string& iconPath);
    bool UpdateManifest(const std::string& outputPath, bool requireAdmin);

    // NEW: Processing pipeline
    std::vector<uint8_t> ProcessFile(const std::string& filePath, std::string& outHash);
    std::vector<uint8_t> CompressData(const std::vector<uint8_t>& data);
    std::vector<uint8_t> EncryptData(const std::vector<uint8_t>& data);
    std::string CalculateHash(const std::vector<uint8_t>& data);

    // Helper methods
    void ReportProgress(int percent, const std::string& status);
    static std::string EscapeJson(const std::string& str);
};