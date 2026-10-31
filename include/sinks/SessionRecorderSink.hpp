#pragma once

#include "core/IReportSink.hpp"
#include <string>
#include <fstream>
#include <vector>

namespace head::sinks {

struct SessionRecordEntry {
    double timestampSeconds{0.0};
    double audioRmsDbFs{-100.0};
    double humRatio{1.0};
    double mag60HzMv{0.0};
    double mag120HzMv{0.0};
    double noiseFloorMv{0.0};
    double cpuPowerWatts{0.0};
    double vcoreMv{0.0};
    unsigned int busErrors{0};
    size_t activeAnomalyCount{0};
};

class SessionRecorderSink : public core::IReportSink {
public:
    SessionRecorderSink(const std::string& outputFile, unsigned int maxDurationSeconds);
    ~SessionRecorderSink() override;

    void initialize() override;
    void consume(const core::DiagnosticContext& context) override;
    void finalize() override;

    bool isComplete() const;
    void printDiagnosticSummary() const;

private:
    std::string m_filePath;
    unsigned int m_maxDurationSeconds;
    std::ofstream m_file;
    std::vector<SessionRecordEntry> m_entries;
    std::chrono::steady_clock::time_point m_startTime;
    bool m_completed{false};
};

} // namespace head::sinks
