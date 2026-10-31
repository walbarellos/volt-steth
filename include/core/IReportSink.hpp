#pragma once

#include "core/Types.hpp"

namespace head::core {

class IReportSink {
public:
    virtual ~IReportSink() = default;
    virtual void consume(const DiagnosticContext& context) = 0;
    virtual void initialize() = 0;
    virtual void finalize() = 0;
};

} // namespace head::core
