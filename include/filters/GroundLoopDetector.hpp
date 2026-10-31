#pragma once

#include "core/IPipelineFilter.hpp"
#include "config/AppConfig.hpp"

namespace head::filters {

class GroundLoopDetector : public core::IPipelineFilter {
public:
    explicit GroundLoopDetector(const config::AudioConfig& config);
    ~GroundLoopDetector() override = default;

    void process(core::DiagnosticContext& context) override;
    void reset() override;
    std::string getName() const override { return "GroundLoopDetector"; }

private:
    double computeGoertzel(const std::vector<float>& samples, unsigned int sampleRate, double targetFreqHz);

    config::AudioConfig m_config;
};

} // namespace head::filters
