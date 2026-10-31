#pragma once

#include "core/IDataSource.hpp"
#include "config/AppConfig.hpp"
#include <string>
#include <vector>

namespace head::sources {

struct MonitoredSensor {
    std::string path;
    std::string chipName;
    std::string label;
    std::string unit;
    double scaleFactor{1.0};
};

class HwmonPowerSource : public core::IPowerSource {
public:
    explicit HwmonPowerSource(const config::PowerConfig& config);
    ~HwmonPowerSource() override = default;

    bool initialize() override;
    bool acquire(std::vector<core::PowerSensorReading>& outData) override;
    void release() override;
    std::string getName() const override { return "HwmonPowerSource"; }

private:
    void discoverSensors();
    void discoverRapl();

    config::PowerConfig m_config;
    std::vector<MonitoredSensor> m_sensors;
    
    // Suporte a RAPL (Running Average Power Limit)
    std::string m_raplEnergyPath;
    unsigned long long m_lastRaplEnergyUj{0};
    std::chrono::steady_clock::time_point m_lastRaplTime;
};

} // namespace head::sources
