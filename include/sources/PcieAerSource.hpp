#pragma once

#include "core/IDataSource.hpp"
#include "config/AppConfig.hpp"
#include <string>
#include <vector>

namespace head::sources {

class PcieAerSource : public core::IBusSource {
public:
    explicit PcieAerSource(const config::BusConfig& config);
    ~PcieAerSource() override = default;

    bool initialize() override;
    bool acquire(std::vector<core::BusErrorReading>& outData) override;
    void release() override;
    std::string getName() const override { return "PcieAerSource"; }

private:
    config::BusConfig m_config;
    std::vector<std::string> m_monitoredDevices;
};

} // namespace head::sources
