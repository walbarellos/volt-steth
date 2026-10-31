#pragma once

#include "core/IPipelineFilter.hpp"
#include "config/AppConfig.hpp"
#include <deque>
#include <unordered_map>

namespace head::filters {

class VoltageRippleDetector : public core::IPipelineFilter {
public:
    explicit VoltageRippleDetector(const config::PowerConfig& config);
    ~VoltageRippleDetector() override = default;

    void process(core::DiagnosticContext& context) override;
    void reset() override;
    std::string getName() const override { return "VoltageRippleDetector"; }

private:
    config::PowerConfig m_config;
    std::unordered_map<std::string, std::deque<double>> m_voltageHistory;
};

} // namespace head::filters
