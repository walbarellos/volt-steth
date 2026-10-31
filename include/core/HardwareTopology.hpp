#pragma once

#include <string>
#include <vector>

namespace head::core {

struct AudioCardInfo {
    int cardIndex{0};
    std::string cardId;
    std::string driverName;
    std::string pciAddress;
    bool isAnalog{false};
    bool isDigital{false};
    bool captureMuted{false};
    int captureVolumePercent{0};
};

struct UsbDeviceInfo {
    std::string bus;
    std::string deviceId;
    std::string vendorId;
    std::string productId;
    std::string description;
};

struct PciDeviceInfo {
    std::string pciAddress;
    std::string vendorDevice;
    std::string description;
    std::string driver;
};

struct SystemHardwareTopology {
    std::vector<AudioCardInfo> audioCards;
    std::vector<UsbDeviceInfo> usbDevices;
    std::vector<PciDeviceInfo> pciBuses;
    std::string pciRootTopology;
    std::string preflightStatus{"OK"};
    std::string preflightWarning{""};
};

} // namespace head::core
