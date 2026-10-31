#include "config/AppConfig.hpp"
#include "pipeline/DiagnosticPipeline.hpp"
#include "sources/AlsaAudioSource.hpp"
#include "sources/HwmonPowerSource.hpp"
#include "sources/PcieAerSource.hpp"
#include "filters/GroundLoopDetector.hpp"
#include "filters/VoltageRippleDetector.hpp"
#include "filters/EmiCorrelationDetector.hpp"
#include "sinks/ConsoleHudSink.hpp"
#include "sinks/JsonStreamSink.hpp"

#include <csignal>
#include <iostream>
#include <memory>

static head::pipeline::DiagnosticPipeline* g_pipeline = nullptr;

void signalHandler(int) {
    if (g_pipeline) {
        g_pipeline->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string configPath = "config.default.ini";
    bool forceJson = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--json") {
            forceJson = true;
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "HEAD: Hardware Electrical & Physical Anomaly Detector\n"
                      << "Uso: " << argv[0] << " [opções]\n"
                      << "Opções:\n"
                      << "  --config <arquivo>   Caminho para arquivo de configuração INI\n"
                      << "  --json               Exporta eventos em streaming JSON em stdout\n"
                      << "  --help, -h           Exibe esta ajuda\n";
            return 0;
        }
    }

    // 1. Carregamento de Configuração Dinâmica
    head::config::AppConfig config = head::config::AppConfig::loadFromFile(configPath);
    if (forceJson) {
        config.output.enableConsoleHud = false;
        config.output.enableJsonStream = true;
    }

    // 2. Criação do Pipeline e Injeção de Dependências
    head::pipeline::DiagnosticPipeline pipeline(config);
    g_pipeline = &pipeline;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Injeção de Fontes de Dados (Sources)
    pipeline.setAudioSource(std::make_unique<head::sources::AlsaAudioSource>(config.audio));
    pipeline.setPowerSource(std::make_unique<head::sources::HwmonPowerSource>(config.power));
    pipeline.setBusSource(std::make_unique<head::sources::PcieAerSource>(config.bus));

    // Injeção de Filtros DSP e Analisadores
    pipeline.addFilter(std::make_unique<head::filters::GroundLoopDetector>(config.audio));
    pipeline.addFilter(std::make_unique<head::filters::VoltageRippleDetector>(config.power));
    pipeline.addFilter(std::make_unique<head::filters::EmiCorrelationDetector>(config.audio));

    // Injeção de Consumidores (Sinks)
    if (config.output.enableConsoleHud) {
        pipeline.addSink(std::make_unique<head::sinks::ConsoleHudSink>());
    }
    if (config.output.enableJsonStream) {
        pipeline.addSink(std::make_unique<head::sinks::JsonStreamSink>(std::cout));
    }

    // 3. Execução
    if (!pipeline.start()) {
        std::cerr << "[Main] Falha ao iniciar pipeline de diagnóstico.\n";
        return 1;
    }

    pipeline.run();

    return 0;
}
