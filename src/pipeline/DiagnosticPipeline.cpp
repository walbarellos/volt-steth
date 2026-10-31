#include "pipeline/DiagnosticPipeline.hpp"
#include <thread>
#include <iostream>

namespace head::pipeline {

DiagnosticPipeline::DiagnosticPipeline(const config::AppConfig& config)
    : m_config(config) {}

DiagnosticPipeline::~DiagnosticPipeline() {
    stop();
}

void DiagnosticPipeline::addFilter(std::unique_ptr<core::IPipelineFilter> filter) {
    if (filter) m_filters.push_back(std::move(filter));
}

void DiagnosticPipeline::addSink(std::unique_ptr<core::IReportSink> sink) {
    if (sink) m_sinks.push_back(std::move(sink));
}

void DiagnosticPipeline::setAudioSource(std::unique_ptr<core::IAudioSource> source) {
    m_audioSource = std::move(source);
}

void DiagnosticPipeline::setPowerSource(std::unique_ptr<core::IPowerSource> source) {
    m_powerSource = std::move(source);
}

void DiagnosticPipeline::setBusSource(std::unique_ptr<core::IBusSource> source) {
    m_busSource = std::move(source);
}

bool DiagnosticPipeline::start() {
    if (m_audioSource && !m_audioSource->initialize()) {
        std::cerr << "[Pipeline] Falha ao iniciar audio source.\n";
    }
    if (m_powerSource && !m_powerSource->initialize()) {
        std::cerr << "[Pipeline] Falha ao iniciar power source.\n";
    }
    if (m_busSource && !m_busSource->initialize()) {
        std::cerr << "[Pipeline] Falha ao iniciar bus source.\n";
    }

    for (auto& sink : m_sinks) {
        sink->initialize();
    }

    m_running = true;
    return true;
}

void DiagnosticPipeline::stop() {
    if (m_running) {
        m_running = false;
        for (auto& sink : m_sinks) {
            sink->finalize();
        }
        if (m_audioSource) m_audioSource->release();
        if (m_powerSource) m_powerSource->release();
        if (m_busSource) m_busSource->release();
    }
}

void DiagnosticPipeline::run() {
    while (m_running) {
        auto cycleStart = std::chrono::steady_clock::now();
        m_context.activeAnomalies.clear();
        m_context.cycleTimestamp = cycleStart;

        // 1. Fase de Aquisição (Sources)
        if (m_audioSource) {
            m_audioSource->acquire(m_context.currentAudio);
        }
        if (m_powerSource) {
            m_powerSource->acquire(m_context.powerReadings);
        }
        if (m_busSource) {
            m_busSource->acquire(m_context.busErrors);
        }

        // 2. Fase de Processamento (Filters)
        for (auto& filter : m_filters) {
            filter->process(m_context);
        }

        // 3. Fase de Emissão (Sinks)
        for (auto& sink : m_sinks) {
            sink->consume(m_context);
        }

        // Controle de ciclo de execução
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - cycleStart).count();
        int sleepTime = static_cast<int>(m_config.output.hudRefreshRateMs) - static_cast<int>(elapsed);
        if (sleepTime > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
        }
    }
}

} // namespace head::pipeline
