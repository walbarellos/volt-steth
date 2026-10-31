#include "filters/GroundLoopDetector.hpp"
#include <cmath>
#include <vector>

namespace head::filters {

GroundLoopDetector::GroundLoopDetector(const config::AudioConfig& config)
    : m_config(config) {}

void GroundLoopDetector::reset() {}

// Algoritmo de Goertzel com Janelamento de Hann para supressão de Spectral Leakage
double GroundLoopDetector::computeGoertzel(const std::vector<float>& samples, unsigned int sampleRate, double targetFreqHz) {
    if (samples.empty() || sampleRate == 0) return 0.0;

    int n = static_cast<int>(samples.size());
    double k = std::round((n * targetFreqHz) / static_cast<double>(sampleRate));
    double omega = (2.0 * M_PI * k) / static_cast<double>(n);
    double cosine = std::cos(omega);
    double coeff = 2.0 * cosine;

    double q0 = 0.0;
    double q1 = 0.0;
    double q2 = 0.0;

    // Aplicação da Janela de Hann durante o processamento para evitar alocações extras
    // w[i] = 0.5 * (1.0 - cos(2 * pi * i / (n - 1)))
    double windowSum = 0.0;
    for (int i = 0; i < n; ++i) {
        double window = 0.5 * (1.0 - std::cos((2.0 * M_PI * i) / static_cast<double>(n - 1)));
        double windowedSample = static_cast<double>(samples[i]) * window;
        windowSum += window;

        q0 = coeff * q1 - q2 + windowedSample;
        q2 = q1;
        q1 = q0;
    }

    double magnitudeSquared = q1 * q1 + q2 * q2 - q1 * q2 * coeff;
    if (magnitudeSquared < 0.0) magnitudeSquared = 0.0;

    // Normalização pela soma dos coeficientes da janela
    return (windowSum > 0.0) ? (std::sqrt(magnitudeSquared) * 2.0 / windowSum) : 0.0;
}

void GroundLoopDetector::process(core::DiagnosticContext& context) {
    const auto& samples = context.currentAudio.samples;
    if (samples.empty()) return;

    unsigned int rate = context.currentAudio.sampleRate;
    double f0 = m_config.mainsFrequencyHz;
    double f1 = f0 * 2.0; // 120Hz
    double f2 = f0 * 3.0; // 180Hz

    double magF0 = computeGoertzel(samples, rate, f0);
    double magF1 = computeGoertzel(samples, rate, f1);
    double magF2 = computeGoertzel(samples, rate, f2);

    // Amostra piso de ruído em frequências adjacentes não-harmônicas (850Hz e 1150Hz)
    double magRef1 = computeGoertzel(samples, rate, 850.0);
    double magRef2 = computeGoertzel(samples, rate, 1150.0);
    double noiseFloor = (magRef1 + magRef2) / 2.0;
    if (noiseFloor < 1e-6) noiseFloor = 1e-6;

    double maxHum = std::max({magF0, magF1, magF2});
    double humRatio = maxHum / noiseFloor;

    // Dispara anomalia de Loop de Terra se a harmônica da rede sobressair com significância
    if (humRatio >= m_config.humRatioThreshold && maxHum > 0.002) {
        core::AnomalyReport report;
        report.type = core::AnomalyType::GROUND_LOOP_MAINS_HUM;
        report.sourceModule = getName();
        report.severity = (humRatio > m_config.humRatioThreshold * 2.5) ? core::Severity::CRITICAL : core::Severity::WARNING;
        report.description = "Loop de Terra (Ground Loop): Harmônica da rede (" + 
                             std::to_string(static_cast<int>(f0)) + "Hz/" + 
                             std::to_string(static_cast<int>(f1)) + "Hz) detectada com SNR de " +
                             std::to_string(static_cast<int>(humRatio)) + "x acima do piso de ruído.";
        report.numericMetrics["hum_ratio"] = humRatio;
        report.numericMetrics["mag_60hz"] = magF0 * 1000.0;
        report.numericMetrics["mag_120hz"] = magF1 * 1000.0;
        report.numericMetrics["noise_floor"] = noiseFloor * 1000.0;
        report.timestamp = std::chrono::system_clock::now();

        context.activeAnomalies.push_back(report);
    }
}

} // namespace head::filters
