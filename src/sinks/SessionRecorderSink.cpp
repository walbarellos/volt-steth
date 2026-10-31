#include "sinks/SessionRecorderSink.hpp"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <numeric>
#include <algorithm>

namespace head::sinks {

SessionRecorderSink::SessionRecorderSink(const std::string& outputFile, unsigned int maxDurationSeconds)
    : m_filePath(outputFile), m_maxDurationSeconds(maxDurationSeconds) {}

SessionRecorderSink::~SessionRecorderSink() {
    finalize();
}

void SessionRecorderSink::initialize() {
    m_file.open(m_filePath, std::ios::out | std::ios::trunc);
    if (!m_file.is_open()) {
        std::cerr << "[SessionRecorder] Erro ao abrir arquivo para gravação: " << m_filePath << "\n";
    }
    m_startTime = std::chrono::steady_clock::now();
    m_entries.clear();
    m_completed = false;
    std::cout << "[SessionRecorder] Iniciando gravação de telemetria (" 
              << m_maxDurationSeconds << "s) em: " << m_filePath << "\n";
}

bool SessionRecorderSink::isComplete() const {
    return m_completed;
}

void SessionRecorderSink::consume(const core::DiagnosticContext& context) {
    if (m_completed) return;

    auto now = std::chrono::steady_clock::now();
    double elapsedSec = std::chrono::duration_cast<std::chrono::duration<double>>(now - m_startTime).count();

    if (elapsedSec >= m_maxDurationSeconds) {
        m_completed = true;
        return;
    }

    SessionRecordEntry entry;
    entry.timestampSeconds = elapsedSec;

    // Métricas de Áudio
    if (!context.currentAudio.samples.empty()) {
        double sumSq = 0.0;
        for (float s : context.currentAudio.samples) sumSq += s * s;
        double rms = std::sqrt(sumSq / static_cast<double>(context.currentAudio.samples.size()));
        entry.audioRmsDbFs = (rms > 1e-6) ? 20.0 * std::log10(rms) : -120.0;
    }

    // Procura métricas dos filtros de anomalia se houver
    entry.activeAnomalyCount = context.activeAnomalies.size();
    for (const auto& anom : context.activeAnomalies) {
        if (anom.numericMetrics.count("hum_ratio")) {
            entry.humRatio = anom.numericMetrics.at("hum_ratio");
        }
        if (anom.numericMetrics.count("mag_60hz")) {
            entry.mag60HzMv = anom.numericMetrics.at("mag_60hz");
        }
        if (anom.numericMetrics.count("mag_120hz")) {
            entry.mag120HzMv = anom.numericMetrics.at("mag_120hz");
        }
        if (anom.numericMetrics.count("noise_floor")) {
            entry.noiseFloorMv = anom.numericMetrics.at("noise_floor");
        }
    }

    // Métricas Elétricas
    for (const auto& p : context.powerReadings) {
        if (p.sensorName == "pkg_power" || p.unit == "W") {
            entry.cpuPowerWatts = p.valueMilliUnits / 1000.0;
        } else if (p.unit == "mV") {
            entry.vcoreMv = p.valueMilliUnits;
        }
    }

    // Barramento
    for (const auto& b : context.busErrors) {
        entry.busErrors += (b.fatalErrors + b.nonFatalErrors + b.correctableErrors);
    }

    m_entries.push_back(entry);

    // Grava linha JSONL
    if (m_file.is_open()) {
        m_file << "{\"t\":" << std::fixed << std::setprecision(3) << entry.timestampSeconds
               << ",\"rms_db\":" << std::setprecision(1) << entry.audioRmsDbFs
               << ",\"hum_ratio\":" << std::setprecision(2) << entry.humRatio
               << ",\"pkg_watts\":" << std::setprecision(2) << entry.cpuPowerWatts
               << ",\"vcore_mv\":" << std::setprecision(1) << entry.vcoreMv
               << ",\"bus_errors\":" << entry.busErrors
               << ",\"anomalies\":" << entry.activeAnomalyCount << "}\n";
    }
}

void SessionRecorderSink::finalize() {
    if (m_file.is_open()) {
        m_file.close();
        printDiagnosticSummary();
    }
}

void SessionRecorderSink::printDiagnosticSummary() const {
    if (m_entries.empty()) {
        std::cout << "[SessionRecorder] Nenhuma amostra foi gravada.\n";
        return;
    }

    double minRms = 0.0, maxRms = -120.0, sumRms = 0.0;
    double maxHumRatio = 1.0;
    double maxPowerWatts = 0.0, sumPowerWatts = 0.0;
    unsigned int totalBusErrors = 0;
    size_t totalAnomalies = 0;

    for (const auto& e : m_entries) {
        if (e.audioRmsDbFs < minRms) minRms = e.audioRmsDbFs;
        if (e.audioRmsDbFs > maxRms) maxRms = e.audioRmsDbFs;
        sumRms += e.audioRmsDbFs;

        if (e.humRatio > maxHumRatio) maxHumRatio = e.humRatio;
        if (e.cpuPowerWatts > maxPowerWatts) maxPowerWatts = e.cpuPowerWatts;
        sumPowerWatts += e.cpuPowerWatts;

        totalBusErrors += e.busErrors;
        totalAnomalies += e.activeAnomalyCount;
    }

    double avgRms = sumRms / static_cast<double>(m_entries.size());
    double avgPower = sumPowerWatts / static_cast<double>(m_entries.size());

    std::cout << "\n================================================================================\n";
    std::cout << "        VOLTSTETH: RELATÓRIO DE AUDITORIA ELETROMECÂNICA E FÍSICA              \n";
    std::cout << "================================================================================\n";
    std::cout << "  • Duração da Sessão   : " << std::fixed << std::setprecision(2) << m_entries.back().timestampSeconds << " segundos (" << m_entries.size() << " frames)\n";
    std::cout << "  • Piso de Ruído Médio : " << std::fixed << std::setprecision(1) << avgRms << " dBFS (Pico: " << maxRms << " dBFS)\n";
    std::cout << "  • Razão Máx. 60Hz/Hum : " << std::fixed << std::setprecision(2) << maxHumRatio << "x do piso de ruído\n";
    std::cout << "  • Potência Média CPU  : " << std::fixed << std::setprecision(2) << avgPower << " W (Pico: " << maxPowerWatts << " W)\n";
    std::cout << "  • Erros Físicos PCIe  : " << totalBusErrors << "\n";
    std::cout << "  • Anomalias Flagradas : " << totalAnomalies << "\n";
    std::cout << "--------------------------------------------------------------------------------\n";
    std::cout << "  [VEREDITO ELETROMECÂNICO PARA ANÁLISE]:\n";

    if (maxHumRatio >= 3.0) {
        std::cout << "  ⚠️  [GROUND LOOP DETECTADO]: Harmônica de rede dominante flagrada na entrada analógica.\n";
    } else {
        std::cout << "  ✅  [GROUND LOOP]: AUSENTE. Malha analógica isolada com sucesso.\n";
    }

    if (totalBusErrors > 0) {
        std::cout << "  ⚠️  [PCIe AER]: ERROS FÍSICOS REGISTRADOS no barramento da placa.\n";
    } else {
        std::cout << "  ✅  [PCIe AER]: INTEGRIDADE DE BARRAMENTO 100% LIMPA.\n";
    }

    std::cout << "================================================================================\n";
    std::cout << "  Arquivo gravado com sucesso: " << m_filePath << "\n";
    std::cout << "================================================================================\n\n";
}

} // namespace head::sinks
