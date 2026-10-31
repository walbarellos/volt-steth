#include "sinks/ConsoleHudSink.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>

namespace head::sinks {

void ConsoleHudSink::initialize() {
    std::cout << "\033[2J\033[H\033[?25l"; // limpa tela, home, esconde cursor
    std::cout.flush();
    m_lastRenderTime = std::chrono::steady_clock::now();
}

void ConsoleHudSink::finalize() {
    std::cout << "\033[?25h\n"; // restaura cursor
    std::cout.flush();
}

void ConsoleHudSink::consume(const core::DiagnosticContext& context) {
    // Volta o cursor para o topo
    std::cout << "\033[H";

    std::cout << "\033[1;36m================================================================================\033[0m\n";
    std::cout << "\033[1;37m   HEAD: Hardware Electrical & Physical Anomaly Detector (Pipe-and-Filter)   \033[0m\n";
    std::cout << "\033[1;36m================================================================================\033[0m\n";

    // 1. Áudio Analógico
    double rms = 0.0;
    if (!context.currentAudio.samples.empty()) {
        double sumSq = 0.0;
        for (float s : context.currentAudio.samples) sumSq += s * s;
        rms = std::sqrt(sumSq / static_cast<double>(context.currentAudio.samples.size()));
    }
    double dbFs = (rms > 1e-5) ? 20.0 * std::log10(rms) : -100.0;

    std::cout << "\033[1;33m[1. TELEMETRIA ANALÓGICA / SINAL]\033[0m\n";
    std::cout << "  • Nível RMS de Entrada: " << std::fixed << std::setprecision(1) << dbFs << " dBFS "
              << " | Taxa: " << context.currentAudio.sampleRate << " Hz "
              << " | Amostras: " << context.currentAudio.samples.size() << "\n";

    // Barra de VU Meter
    int vuBars = static_cast<int>((dbFs + 60.0) / 2.0);
    if (vuBars < 0) vuBars = 0;
    if (vuBars > 30) vuBars = 30;
    std::cout << "  • VU [";
    for (int i = 0; i < 30; ++i) {
        if (i < vuBars) std::cout << "\033[32m#\033[0m";
        else std::cout << " ";
    }
    std::cout << "]\n\n";

    // 2. Sensores Elétricos (hwmon / rapl)
    std::cout << "\033[1;33m[2. TELEMETRIA ELÉTRICA / ALIMENTAÇÃO]\033[0m\n";
    if (context.powerReadings.empty()) {
        std::cout << "  • Sensores hwmon: Aguardando leitura...\n";
    } else {
        int count = 0;
        for (const auto& p : context.powerReadings) {
            if (count++ >= 4) break; // exibe os principais
            double val = p.valueMilliUnits;
            if (p.unit == "W") val /= 1000.0;
            std::cout << "  • " << std::left << std::setw(15) << (p.chipName + ":" + p.sensorName)
                      << ": " << std::fixed << std::setprecision(2) << val << " " << p.unit << "\n";
        }
    }
    std::cout << "\n";

    // 3. Barramento Físico (PCIe / USB)
    std::cout << "\033[1;33m[3. INTEGRIDADE DE BARRAMENTO (PCIe AER / USB)]\033[0m\n";
    if (context.busErrors.empty()) {
        std::cout << "  • Status: \033[32mNenhum erro físico de barramento (TLP/AER limpos)\033[0m\n";
    } else {
        for (const auto& b : context.busErrors) {
            std::cout << "  • \033[31mPCI " << b.pciAddress << " -> Erros fatais: " << b.fatalErrors
                      << " | Não-fatais: " << b.nonFatalErrors 
                      << " | Corrigíveis: " << b.correctableErrors << "\033[0m\n";
        }
    }
    std::cout << "\n";

    // 4. Anomalias Físicas Detectadas pelos Filtros do Pipeline
    std::cout << "\033[1;33m[4. DIAGNÓSTICO DE ANOMALIAS DO PIPELINE]\033[0m\n";
    if (context.activeAnomalies.empty()) {
        std::cout << "  \033[1;32m[STATUS: NORMAL]\033[0m Nenhum loop de terra, ripple ou EMI detectado.\n";
    } else {
        for (const auto& anom : context.activeAnomalies) {
            std::string tag = (anom.severity == core::Severity::CRITICAL) ? "\033[1;41m[CRÍTICO]\033[0m" : "\033[1;43m[AVISO]\033[0m";
            std::cout << "  " << tag << " " << anom.description << "\n";
            for (const auto& [k, v] : anom.numericMetrics) {
                std::cout << "     └─ " << k << ": " << std::fixed << std::setprecision(3) << v << "\n";
            }
        }
    }

    std::cout << "\n\033[1;30m(Pressione Ctrl+C para encerrar o monitoramento)\033[0m\n";
    std::cout.flush();
}

} // namespace head::sinks
