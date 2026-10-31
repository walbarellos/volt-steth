#include "sources/HwmonPowerSource.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace head::sources {

HwmonPowerSource::HwmonPowerSource(const config::PowerConfig& config)
    : m_config(config) {}

bool HwmonPowerSource::initialize() {
    discoverSensors();
    discoverRapl();
    return true;
}

void HwmonPowerSource::discoverRapl() {
    std::string raplPath = "/sys/class/powercap/intel-rapl/intel-rapl:0/energy_uj";
    if (fs::exists(raplPath)) {
        m_raplEnergyPath = raplPath;
        std::ifstream file(m_raplEnergyPath);
        if (file >> m_lastRaplEnergyUj) {
            m_lastRaplTime = std::chrono::steady_clock::now();
        }
    }
}

void HwmonPowerSource::discoverSensors() {
    m_sensors.clear();
    if (!fs::exists(m_config.hwmonBaseDir)) {
        return;
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_config.hwmonBaseDir)) {
            if (!entry.is_directory()) continue;
            
            std::string chipName = "generic";
            std::string nameFile = entry.path().string() + "/name";
            if (fs::exists(nameFile)) {
                std::ifstream nf(nameFile);
                nf >> chipName;
            }

            for (const auto& sensorFile : fs::directory_iterator(entry.path())) {
                std::string filename = sensorFile.path().filename().string();
                
                // Tensões (in*_input em mV)
                if (filename.rfind("in", 0) == 0 && filename.find("_input") != std::string::npos) {
                    m_sensors.push_back({sensorFile.path().string(), chipName, filename, "mV", 1.0});
                }
                // Potência (power*_input em microwatts -> mW)
                else if (filename.rfind("power", 0) == 0 && filename.find("_input") != std::string::npos) {
                    m_sensors.push_back({sensorFile.path().string(), chipName, filename, "mW", 0.001});
                }
                // Corrente (curr*_input em mA)
                else if (filename.rfind("curr", 0) == 0 && filename.find("_input") != std::string::npos) {
                    m_sensors.push_back({sensorFile.path().string(), chipName, filename, "mA", 1.0});
                }
                // Temperatura (temp*_input em milliCelsius -> C)
                else if (filename.rfind("temp", 0) == 0 && filename.find("_input") != std::string::npos) {
                    m_sensors.push_back({sensorFile.path().string(), chipName, filename, "°C", 0.001});
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[HwmonPowerSource] Erro ao varrer hwmon: " << e.what() << "\n";
    }
}

bool HwmonPowerSource::acquire(std::vector<core::PowerSensorReading>& outData) {
    outData.clear();
    auto now = std::chrono::steady_clock::now();

    for (const auto& sensor : m_sensors) {
        std::ifstream file(sensor.path);
        double rawVal = 0.0;
        if (file >> rawVal) {
            core::PowerSensorReading reading;
            reading.sensorName = sensor.label;
            reading.chipName = sensor.chipName;
            reading.valueMilliUnits = rawVal * sensor.scaleFactor;
            reading.unit = sensor.unit;
            reading.timestamp = now;
            outData.push_back(reading);
        }
    }

    // Calcula potência instantânea via Intel RAPL (CPU Package Watts)
    if (!m_raplEnergyPath.empty()) {
        std::ifstream file(m_raplEnergyPath);
        unsigned long long currentEnergyUj = 0;
        if (file >> currentEnergyUj) {
            auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastRaplTime).count();
            if (elapsedUs > 5000) { // pelo menos 5ms
                unsigned long long deltaUj = 0;
                if (currentEnergyUj >= m_lastRaplEnergyUj) {
                    deltaUj = currentEnergyUj - m_lastRaplEnergyUj;
                }
                double watts = (static_cast<double>(deltaUj) / static_cast<double>(elapsedUs)); // uJ / us = Joules / sec = Watts
                
                core::PowerSensorReading raplReading;
                raplReading.chipName = "intel-rapl";
                raplReading.sensorName = "pkg_power";
                raplReading.valueMilliUnits = watts * 1000.0; // mW
                raplReading.unit = "W";
                raplReading.timestamp = now;
                outData.push_back(raplReading);

                m_lastRaplEnergyUj = currentEnergyUj;
                m_lastRaplTime = now;
            }
        }
    }

    return true;
}

void HwmonPowerSource::release() {
    m_sensors.clear();
}

} // namespace head::sources
