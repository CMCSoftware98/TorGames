// TorGames.Binder - Builder engine
#pragma once

#include "../core/common.h"

class Builder {
public:
    Builder();
    ~Builder();

    // Set the configuration to build
    void SetConfig(const BinderConfig& config);

    // Build the final executable
    bool Build(const std::string& outputPath, std::string& errorMsg);

    // Get the path to the stub executable
    std::string GetStubPath() const;

private:
    BinderConfig m_config;

    // Copy stub to output location
    bool CopyStub(const std::string& outputPath, std::string& errorMsg);

    // Embed configuration as resource
    bool EmbedConfig(const std::string& exePath, std::string& errorMsg);

    // Embed a single file as resource
    bool EmbedFile(const std::string& exePath, int resourceId, const std::string& filePath, std::string& errorMsg);

    // Update the executable icon
    bool UpdateIcon(const std::string& exePath, const std::string& iconPath, std::string& errorMsg);

    // Update the manifest for admin requirement
    bool UpdateManifest(const std::string& exePath, bool requireAdmin, std::string& errorMsg);

    // Serialize config to JSON
    std::string SerializeConfig() const;
};
