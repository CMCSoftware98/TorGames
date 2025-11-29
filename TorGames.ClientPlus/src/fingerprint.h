// TorGames.ClientPlus - Hardware fingerprinting
#pragma once

#include "common.h"

namespace Fingerprint {
    // Get unique hardware fingerprint
    std::string GetHardwareId();

    // Individual components
    std::string GetCpuId();
    std::string GetBiosSerial();
    std::string GetMotherboardSerial();
    std::string GetMacAddress();
}
