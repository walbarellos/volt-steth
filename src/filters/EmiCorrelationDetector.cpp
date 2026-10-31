#include "filters/EmiCorrelationDetector.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace head::filters {

EmiCorrelationDetector::EmiCorrelationDetector(const config::AudioConfig& audioCfg)
    : m_audioConfig(audioCfg) {}

void EmiCorrelationDetector::reset() {
    m_audioHighNoiseHistory.clear();
    m_powerDeltaHistory.clear();
    m_lastPowerReadingMw = 0.0;
}

// Filtro passa-altas de primeira ordem para isolar ruídos EMI de alta frequência
double EmiCorrelationDetector::computeRmsHighPass(const std::vector<float>& samples) {
    if (samples.empty()) return 0.0;
    
    double prevX = samples[0];
    double prevY = 0.0;
    double alpha = 0.85; // corte aproximado em frequências altas
    double sumSquares = 0.0;

    for (float sample : samples) {
        double currentX = static_cast<double>(sample);
        double currentY = alpha * (prevY + currentX - prevX);
        sumSquares += currentY * currentY;
        prevX = currentX;
        prevY = currentY;
    }

    return std::sqrt(sumSquares / static_cast<double>(samples.size()));
}

void EmiCorrelationDetector::process(core::DiagnosticContext& context) {
    double currentNoise = computeRmsHighPass(context.currentAudio.samples);

    double currentPowerMw = 0.0;
    bool foundPower = false;
    for (const auto& p : context.powerReadings) {
        if (p.sensorName == "pkg_power" || p.unit == "W" || p.unit == "mW") {
            currentPowerMw = p.valueMilliUnits;
            foundPower = true;
            break;
        }
    }

    if (!foundPower) return;

    double powerDelta = std::abs(currentPowerMw - m_lastPowerReadingMw);
    m_lastPowerReadingMw = currentPowerMw;

    m_audioHighNoiseHistory.push_back(currentNoise);
    m_powerDeltaHistory.push_back(powerDelta);

    if (m_audioHighNoiseHistory.size() > 30) {
        m_audioHighNoiseHistory.pop_front();
        m_powerDeltaHistory.pop_front();
    }

    if (m_audioHighNoiseHistory.size() < 15) return;

    // Calcula coeficiente de correlação de Pearson entre Ruído e Delta de Potência
    size_t n = m_audioHighNoiseHistory.size();
    double meanNoise = std::accumulate(m_audioHighNoiseHistory.begin(), m_audioHighNoiseHistory.end(), 0.0) / static_cast<double>(n);
    double meanPower = std::accumulate(m_powerDeltaHistory.begin(), m_powerDeltaHistory.end(), 0.0) / static_cast<double>(n);

    double cov = 0.0;
    double varNoise = 0.0;
    double varPower = 0.0;

    for (size_t i = 0; i < n; ++i) {
        double diffN = m_audioHighNoiseHistory[i] - meanNoise;
        double diffP = m_powerDeltaHistory[i] - meanPower;
        cov += diffN * diffP;
        varNoise += diffN * diffN;
        varPower += diffP * diffP;
    }

    if (varNoise > 1e-9 && varPower > 1e-4) {
        double correlation = cov / std::sqrt(varNoise * varPower);

        if (correlation > 0.65 && meanNoise > 0.005) {
            core::AnomalyReport report;
            report.type = core::AnomalyType::EMI_CROSS_COUPLING;
            report.sourceModule = getName();
            report.severity = (correlation > 0.85) ? core::Severity::CRITICAL : core::Severity::WARNING;
            report.description = "Acoplamento Eletromagnético (EMI / Coil Whine): Ruído correlacionado à comutação de energia da CPU/GPU.";
            report.numericMetrics["pearson_r"] = correlation;
            report.numericMetrics["noise_rms"] = meanNoise * 1000.0;
            report.numericMetrics["mean_power_delta_w"] = meanPower / 1000.0;
            report.timestamp = std::chrono::system_clock::now();

            context.activeAnomalies.push_back(report);
        }
    }
}

} // namespace head::filters
