#pragma once

#include "core/IPipelineFilter.hpp"
#include "config/AppConfig.hpp"
#include <deque>

namespace head::filters {

class EmiCorrelationDetector : public core::IPipelineFilter {
public:
    explicit EmiCorrelationDetector(const config::AudioConfig& audioCfg);
    ~EmiCorrelationDetector() override = default;

    void process(core::DiagnosticContext& context) override;
    void reset() override;
    std::string getName() const override { return "EmiCorrelationDetector"; }

private:
    double computeRmsHighPass(const std::vector<float>& samples);

    config::AudioConfig m_audioConfig;
    std::deque<double> m_audioHighNoiseHistory;
    std::deque<double> m_powerDeltaHistory;
    double m_lastPowerReadingMw{0.0};
};

} // namespace head::filters
