#include "config/AppConfig.hpp"
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>

namespace head::config {

static std::string trim(const std::string& str) {
    size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return "";
    size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, (last - first + 1));
}

AppConfig AppConfig::createDefault() {
    return AppConfig{};
}

AppConfig AppConfig::loadFromFile(const std::string& filePath) {
    AppConfig config = createDefault();
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cerr << "[Config] Arquivo " << filePath << " não encontrado, utilizando parâmetros padrão dinâmicos.\n";
        return config;
    }

    std::string line;
    std::string currentSection = "";

    while (std::getline(file, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }

        if (line.front() == '[' && line.back() == ']') {
            currentSection = line.substr(1, line.size() - 2);
            continue;
        }

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        std::string key = trim(line.substr(0, eqPos));
        std::string val = trim(line.substr(eqPos + 1));

        if (currentSection == "audio") {
            if (key == "device") config.audio.device = val;
            else if (key == "sample_rate") config.audio.sampleRate = std::stoul(val);
            else if (key == "buffer_size") config.audio.bufferSize = std::stoul(val);
            else if (key == "mains_frequency_hz") config.audio.mainsFrequencyHz = std::stod(val);
            else if (key == "hum_ratio_threshold") config.audio.humRatioThreshold = std::stod(val);
            else if (key == "emi_band_start_hz") config.audio.emiBandStartHz = std::stod(val);
            else if (key == "emi_band_end_hz") config.audio.emiBandEndHz = std::stod(val);
        } else if (currentSection == "power") {
            if (key == "hwmon_base_dir") config.power.hwmonBaseDir = val;
            else if (key == "max_ripple_stddev_mv") config.power.maxRippleStdDevMv = std::stod(val);
            else if (key == "sample_history_size") config.power.sampleHistorySize = std::stoul(val);
        } else if (currentSection == "bus") {
            if (key == "pci_base_dir") config.bus.pciBaseDir = val;
            else if (key == "check_aer_errors") config.bus.checkAerErrors = (val == "true" || val == "1");
        } else if (currentSection == "output") {
            if (key == "enable_console_hud") config.output.enableConsoleHud = (val == "true" || val == "1");
            else if (key == "enable_json_stream") config.output.enableJsonStream = (val == "true" || val == "1");
            else if (key == "hud_refresh_rate_ms") config.output.hudRefreshRateMs = std::stoul(val);
        }
    }

    return config;
}

} // namespace head::config
