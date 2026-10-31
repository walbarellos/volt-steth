#pragma once

#include <string>
#include <vector>
#include <unordered_map>

namespace head::config {

struct AudioConfig {
    std::string device{"default"};
    unsigned int sampleRate{48000};
    unsigned int bufferSize{4096};
    unsigned int channels{1};
    double mainsFrequencyHz{60.0};
    double humRatioThreshold{3.0};
    double emiBandStartHz{2000.0};
    double emiBandEndHz{12000.0};
};

struct PowerConfig {
    std::string hwmonBaseDir{"/sys/class/hwmon"};
    double maxRippleStdDevMv{35.0};
    unsigned int sampleHistorySize{50};
};

struct BusConfig {
    std::string pciBaseDir{"/sys/bus/pci/devices"};
    bool checkAerErrors{true};
};

struct OutputConfig {
    bool enableConsoleHud{true};
    bool enableJsonStream{false};
    unsigned int hudRefreshRateMs{150};
};

struct AppConfig {
    AudioConfig audio;
    PowerConfig power;
    BusConfig bus;
    OutputConfig output;

    static AppConfig loadFromFile(const std::string& filePath);
    static AppConfig createDefault();
};

} // namespace head::config
