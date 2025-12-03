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