#include "filters/VoltageRippleDetector.hpp"
#include <cmath>
#include <numeric>
#include <algorithm>

namespace head::filters {

VoltageRippleDetector::VoltageRippleDetector(const config::PowerConfig& config)
    : m_config(config) {}

void VoltageRippleDetector::reset() {
    m_voltageHistory.clear();
}

void VoltageRippleDetector::process(core::DiagnosticContext& context) {
    for (const auto& reading : context.powerReadings) {
        if (reading.unit != "mV") continue;

        auto& history = m_voltageHistory[reading.sensorName];
        history.push_back(reading.valueMilliUnits);
        if (history.size() > m_config.sampleHistorySize) {
            history.pop_front();
        }

        if (history.size() < 10) continue;

        double sum = std::accumulate(history.begin(), history.end(), 0.0);
        double mean = sum / static_cast<double>(history.size());

        double sqDiffSum = 0.0;
        double minVal = history.front();
        double maxVal = history.front();

        for (double v : history) {
            sqDiffSum += (v - mean) * (v - mean);
            minVal = std::min(minVal, v);
            maxVal = std::max(maxVal, v);
        }

        double variance = sqDiffSum / static_cast<double>(history.size());
        double stdDev = std::sqrt(variance);
        double peakToPeakRipple = maxVal - minVal;

        if (stdDev >= m_config.maxRippleStdDevMv) {
            core::AnomalyReport report;
            report.type = core::AnomalyType::VOLTAGE_RIPPLE_INSTABILITY;
            report.sourceModule = getName();
            report.severity = (stdDev > m_config.maxRippleStdDevMv * 2.0) ? core::Severity::CRITICAL : core::Severity::WARNING;
            report.description = "Instabilidade/Ripple de Tensão em " + reading.sensorName + 
                                 " (" + reading.chipName + "): Vpp=" + std::to_string(static_cast<int>(peakToPeakRipple)) + "mV";
            report.numericMetrics["stddev_mv"] = stdDev;
            report.numericMetrics["vpp_ripple_mv"] = peakToPeakRipple;
            report.numericMetrics["vmean_mv"] = mean;
            report.timestamp = std::chrono::system_clock::now();

            context.activeAnomalies.push_back(report);
        }
    }
}

} // namespace head::filters
