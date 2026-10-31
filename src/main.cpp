#include "config/AppConfig.hpp"
#include "pipeline/DiagnosticPipeline.hpp"
#include "sources/AlsaAudioSource.hpp"
#include "sources/HwmonPowerSource.hpp"
#include "sources/PcieAerSource.hpp"
#include "sources/TopologyDiscoverySource.hpp"
#include "filters/GroundLoopDetector.hpp"
#include "filters/VoltageRippleDetector.hpp"
#include "filters/EmiCorrelationDetector.hpp"
#include "sinks/ConsoleHudSink.hpp"
#include "sinks/JsonStreamSink.hpp"
#include "sinks/SessionRecorderSink.hpp"

#include <csignal>
#include <iostream>
#include <memory>
#include <thread>

static head::pipeline::DiagnosticPipeline* g_pipeline = nullptr;

void signalHandler(int) {
    if (g_pipeline) {
        g_pipeline->stop();
    }
}

int main(int argc, char* argv[]) {
    std::string configPath = "config.default.ini";
    bool forceJson = false;
    std::string recordFilePath = "";
    unsigned int recordDurationSeconds = 0;
    bool showTopology = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--json") {
            forceJson = true;
        } else if (arg == "--topology") {
            showTopology = true;
        } else if (arg == "--record" && i + 1 < argc) {
            recordFilePath = argv[++i];
            recordDurationSeconds = 10; // padrão 10s se não especificado
        } else if (arg == "--duration" && i + 1 < argc) {
            recordDurationSeconds = std::stoul(argv[++i]);
        } else if (arg == "--diagnose") {
            recordFilePath = "diagnostic_session.jsonl";
            recordDurationSeconds = 6; // 6s de auditoria automática
        } else if (arg == "--help" || arg == "-h") {
            std::cout << "VoltSteth: Hardware Electrical & Physical Stethoscope\n"
                      << "Uso: " << argv[0] << " [opções]\n"
                      << "Opções:\n"
                      << "  --diagnose           Executa auditoria automática (6s) e emite veredito estruturado\n"
                      << "  --record <arquivo>   Grava telemetria em alta resolução (JSONL) para auditoria por IA\n"
                      << "  --duration <seg>     Duração da gravação em segundos (padrão: 10s)\n"
                      << "  --json               Exporta eventos em streaming JSON contínuo\n"
                      << "  --config <arquivo>   Caminho para arquivo de configuração INI\n"
                      << "  --help, -h           Exibe esta ajuda\n";
            return 0;
        }
    }

    if (showTopology) {
        auto topo = head::sources::TopologyDiscoverySource::scanSystem();
        head::sources::TopologyDiscoverySource::printTopologyReport(topo);
        return 0;
    }
    head::config::AppConfig config = head::config::AppConfig::loadFromFile(configPath);
    if (forceJson) {
        config.output.enableConsoleHud = false;
        config.output.enableJsonStream = true;
    }

    // Se estiver em modo de gravação ou diagnóstico, desliga a HUD para não poluir a saída
    if (!recordFilePath.empty()) {
        config.output.enableConsoleHud = false;
    }

    // 2. Criação do Pipeline e Injeção de Dependências
    head::pipeline::DiagnosticPipeline pipeline(config);
    g_pipeline = &pipeline;

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // Fontes
    pipeline.setAudioSource(std::make_unique<head::sources::AlsaAudioSource>(config.audio));
    pipeline.setPowerSource(std::make_unique<head::sources::HwmonPowerSource>(config.power));
    pipeline.setBusSource(std::make_unique<head::sources::PcieAerSource>(config.bus));

    // Filtros
    pipeline.addFilter(std::make_unique<head::filters::GroundLoopDetector>(config.audio));
    pipeline.addFilter(std::make_unique<head::filters::VoltageRippleDetector>(config.power));
    pipeline.addFilter(std::make_unique<head::filters::EmiCorrelationDetector>(config.audio));

    // Consumidores (Sinks)
    if (config.output.enableConsoleHud) {
        pipeline.addSink(std::make_unique<head::sinks::ConsoleHudSink>());
    }
    if (config.output.enableJsonStream) {
        pipeline.addSink(std::make_unique<head::sinks::JsonStreamSink>(std::cout));
    }
    if (!recordFilePath.empty()) {
        pipeline.addSink(std::make_unique<head::sinks::SessionRecorderSink>(recordFilePath, recordDurationSeconds));
    }

    // 3. Execução
    if (!pipeline.start()) {
        std::cerr << "[Main] Falha ao iniciar pipeline.\n";
        return 1;
    }

    // Se for gravação com duração finita, encerra automaticamente após a duração
    if (recordDurationSeconds > 0 && !recordFilePath.empty()) {
        std::thread killer([&pipeline, recordDurationSeconds]() {
            std::this_thread::sleep_for(std::chrono::seconds(recordDurationSeconds));
            pipeline.stop();
        });
        killer.detach();
    }

    pipeline.run();

    return 0;
}
