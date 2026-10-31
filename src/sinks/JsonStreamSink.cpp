#include "sinks/JsonStreamSink.hpp"
#include <iomanip>

namespace head::sinks {

JsonStreamSink::JsonStreamSink(std::ostream& outStream)
    : m_out(outStream) {}

void JsonStreamSink::consume(const core::DiagnosticContext& context) {
    if (context.activeAnomalies.empty()) return;

    for (const auto& a : context.activeAnomalies) {
        m_out << "{\"timestamp\":" << std::chrono::duration_cast<std::chrono::milliseconds>(
                     a.timestamp.time_since_epoch()).count()
              << ",\"module\":\"" << a.sourceModule << "\""
              << ",\"severity\":\"" << (a.severity == core::Severity::CRITICAL ? "CRITICAL" : "WARNING") << "\""
              << ",\"description\":\"" << a.description << "\""
              << ",\"metrics\":{";
        bool first = true;
        for (const auto& [k, v] : a.numericMetrics) {
            if (!first) m_out << ",";
            m_out << "\"" << k << "\":" << v;
            first = false;
        }
        m_out << "}}\n";
    }
    m_out.flush();
}

} // namespace head::sinks
