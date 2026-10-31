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
#include <cmath>
#include <iomanip>

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
    bool quickCheck = false;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--config" && i + 1 < argc) {
            configPath = argv[++i];
        } else if (arg == "--json") {
            forceJson = true;
        } else if (arg == "--topology") {
            showTopology = true;
        } else if (arg == "--quick-check") {
            quickCheck = true;
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

    if (quickCheck) {
        head::config::AppConfig cfg = head::config::AppConfig::loadFromFile(configPath);
        head::sources::AlsaAudioSource audio(cfg.audio);
        head::sources::HwmonPowerSource power(cfg.power);
        head::sources::PcieAerSource bus(cfg.bus);
        
        audio.initialize();
        power.initialize();
        bus.initialize();

        head::core::DiagnosticContext ctx;
        audio.acquire(ctx.currentAudio);
        power.acquire(ctx.powerReadings);
        bus.acquire(ctx.busErrors);

        head::filters::GroundLoopDetector groundFilter(cfg.audio);
        head::filters::VoltageRippleDetector rippleFilter(cfg.power);
        groundFilter.process(ctx);
        rippleFilter.process(ctx);

        double rmsDb = -120.0;
        if (!ctx.currentAudio.samples.empty()) {
            double sumSq = 0.0;
            for (float s : ctx.currentAudio.samples) sumSq += s * s;
            double rms = std::sqrt(sumSq / static_cast<double>(ctx.currentAudio.samples.size()));
            rmsDb = (rms > 1e-6) ? 20.0 * std::log10(rms) : -120.0;
        }

        double humRatio = 1.0;
        bool hasGroundLoop = false;
        bool isCritical = false;
        for (const auto& a : ctx.activeAnomalies) {
            if (a.numericMetrics.count("hum_ratio")) {
                humRatio = a.numericMetrics.at("hum_ratio");
            }
            if (a.type == head::core::AnomalyType::GROUND_LOOP_MAINS_HUM) {
                hasGroundLoop = true;
            }
            if (a.severity == head::core::Severity::CRITICAL) {
                isCritical = true;
            }
        }

        std::string status = "OK";
        if (isCritical) status = "CRITICAL";
        else if (!ctx.activeAnomalies.empty()) status = "WARNING";

        std::cout << "{\"status\":\"" << status << "\""
                  << ",\"ground_loop\":" << (hasGroundLoop ? "true" : "false")
                  << ",\"rms_db\":" << std::fixed << std::setprecision(1) << rmsDb
                  << ",\"hum_ratio\":" << std::setprecision(2) << humRatio
                  << ",\"anomalies\":" << ctx.activeAnomalies.size() << "}\n";

        audio.release();
        power.release();
        bus.release();
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
