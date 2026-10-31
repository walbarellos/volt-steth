#pragma once

#include "core/IReportSink.hpp"
#include <ostream>

namespace head::sinks {

class JsonStreamSink : public core::IReportSink {
public:
    explicit JsonStreamSink(std::ostream& outStream);
    ~JsonStreamSink() override = default;

    void initialize() override {}
    void consume(const core::DiagnosticContext& context) override;
    void finalize() override {}

private:
    std::ostream& m_out;
};

} // namespace head::sinks
