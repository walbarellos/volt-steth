#pragma once

#include <vector>
#include <string>
#include <chrono>
#include <unordered_map>

namespace head::core {

enum class Severity {
    OK,
    WARNING,
    CRITICAL
};

enum class AnomalyType {
    NONE,
    GROUND_LOOP_MAINS_HUM,
    VOLTAGE_RIPPLE_INSTABILITY,
    EMI_CROSS_COUPLING,
    PCIE_AER_BUS_ERROR
};

struct AudioSampleFrame {
    std::vector<float> samples;
    unsigned int sampleRate{48000};
    std::chrono::steady_clock::time_point timestamp;
};

struct PowerSensorReading {
    std::string sensorName;
    std::string chipName;
    double valueMilliUnits{0.0}; // mV, mW or mA
    std::string unit;
    std::chrono::steady_clock::time_point timestamp;
};

struct BusErrorReading {
    std::string pciAddress;
    std::string deviceName;
    unsigned int correctableErrors{0};
    unsigned int fatalErrors{0};
    unsigned int nonFatalErrors{0};
    std::chrono::steady_clock::time_point timestamp;
};

struct AnomalyReport {
    AnomalyType type{AnomalyType::NONE};
    Severity severity{Severity::OK};
    std::string sourceModule;
    std::string description;
    std::unordered_map<std::string, double> numericMetrics;
    std::chrono::system_clock::time_point timestamp;
};

struct DiagnosticContext {
    AudioSampleFrame currentAudio;
    std::vector<PowerSensorReading> powerReadings;
    std::vector<BusErrorReading> busErrors;
    std::vector<AnomalyReport> activeAnomalies;
    std::chrono::steady_clock::time_point cycleTimestamp;
};

} // namespace head::core
