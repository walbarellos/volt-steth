#pragma once

#include "core/IDataSource.hpp"
#include "core/IPipelineFilter.hpp"
#include "core/IReportSink.hpp"
#include "config/AppConfig.hpp"
#include <memory>
#include <vector>
#include <atomic>

namespace head::pipeline {

class DiagnosticPipeline {
public:
    explicit DiagnosticPipeline(const config::AppConfig& config);
    ~DiagnosticPipeline();

    void addFilter(std::unique_ptr<core::IPipelineFilter> filter);
    void addSink(std::unique_ptr<core::IReportSink> sink);
    
    void setAudioSource(std::unique_ptr<core::IAudioSource> source);
    void setPowerSource(std::unique_ptr<core::IPowerSource> source);
    void setBusSource(std::unique_ptr<core::IBusSource> source);

    bool start();
    void run();
    void stop();

private:
    config::AppConfig m_config;
    std::atomic<bool> m_running{false};

    std::unique_ptr<core::IAudioSource> m_audioSource;
    std::unique_ptr<core::IPowerSource> m_powerSource;
    std::unique_ptr<core::IBusSource> m_busSource;

    std::vector<std::unique_ptr<core::IPipelineFilter>> m_filters;
    std::vector<std::unique_ptr<core::IReportSink>> m_sinks;

    core::DiagnosticContext m_context;
};

} // namespace head::pipeline
