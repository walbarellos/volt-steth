#pragma once

#include "core/Types.hpp"
#include <string>

namespace head::core {

class IPipelineFilter {
public:
    virtual ~IPipelineFilter() = default;
    virtual void process(DiagnosticContext& context) = 0;
    virtual void reset() = 0;
    virtual std::string getName() const = 0;
};

} // namespace head::core
