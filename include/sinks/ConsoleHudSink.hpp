#pragma once

#include "core/IReportSink.hpp"
#include <chrono>

namespace head::sinks {

class ConsoleHudSink : public core::IReportSink {
public:
    ConsoleHudSink() = default;
    ~ConsoleHudSink() override = default;

    void initialize() override;
    void consume(const core::DiagnosticContext& context) override;
    void finalize() override;

private:
    std::chrono::steady_clock::time_point m_lastRenderTime;
};

} // namespace head::sinks
