// TorGames.Binder.Stub - Resource extraction
#pragma once

#include "common.h"

namespace Extractor {
    // Load and parse the configuration from resources
    bool LoadConfig(BinderConfig& config);

    // Extract a file resource to disk
    bool ExtractFile(int resourceId, const char* outputPath);

    // Get the temp extraction folder path
    std::string GetTempExtractionFolder();

    // Create the extraction folder
    bool CreateExtractionFolder(const std::string& path);

    // Clean up extraction folder
    void CleanupExtractionFolder(const std::string& path);
}
