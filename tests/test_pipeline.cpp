#include "core/Types.hpp"
#include "config/AppConfig.hpp"
#include "filters/GroundLoopDetector.hpp"
#include "filters/VoltageRippleDetector.hpp"

#include <iostream>
#include <cassert>
#include <cmath>

void test_clean_audio_no_ground_loop() {
    std::cout << "[TEST] Executando test_clean_audio_no_ground_loop... ";
    head::config::AudioConfig cfg;
    cfg.sampleRate = 48000;
    cfg.bufferSize = 4096;
    cfg.mainsFrequencyHz = 60.0;
    cfg.humRatioThreshold = 3.0;

    head::filters::GroundLoopDetector detector(cfg);
    head::core::DiagnosticContext ctx;
    ctx.currentAudio.sampleRate = cfg.sampleRate;
    ctx.currentAudio.samples.resize(cfg.bufferSize);

    // Gera tom puro em 1000Hz (sem componente de 60Hz/120Hz)
    for (size_t i = 0; i < cfg.bufferSize; ++i) {
        ctx.currentAudio.samples[i] = 0.5f * std::sin(2.0 * M_PI * 1000.0 * i / cfg.sampleRate);
    }

    detector.process(ctx);
    assert(ctx.activeAnomalies.empty() && "Falha: Detectou falso positivo em sinal limpo!");
    std::cout << "PASSOU!\n";
}

void test_synthetic_ground_loop_detection() {
    std::cout << "[TEST] Executando test_synthetic_ground_loop_detection... ";
    head::config::AudioConfig cfg;
    cfg.sampleRate = 48000;
    cfg.bufferSize = 4096;
    cfg.mainsFrequencyHz = 60.0;
    cfg.humRatioThreshold = 3.0;

    head::filters::GroundLoopDetector detector(cfg);
    head::core::DiagnosticContext ctx;
    ctx.currentAudio.sampleRate = cfg.sampleRate;
    ctx.currentAudio.samples.resize(cfg.bufferSize);

    // Injeta harmônicas de ground loop: 60Hz e 120Hz com ruído leve
    for (size_t i = 0; i < cfg.bufferSize; ++i) {
        double hum60 = 0.08 * std::sin(2.0 * M_PI * 60.0 * i / cfg.sampleRate);
        double hum120 = 0.04 * std::sin(2.0 * M_PI * 120.0 * i / cfg.sampleRate);
        double noise = 0.001 * (static_cast<double>(rand()) / RAND_MAX - 0.5);
        ctx.currentAudio.samples[i] = static_cast<float>(hum60 + hum120 + noise);
    }

    detector.process(ctx);
    assert(!ctx.activeAnomalies.empty() && "Falha: Não detectou o loop de terra injetado!");
    assert(ctx.activeAnomalies[0].type == head::core::AnomalyType::GROUND_LOOP_MAINS_HUM);
    std::cout << "PASSOU! (SNR Detectado: " << ctx.activeAnomalies[0].numericMetrics["hum_ratio"] << "x)\n";
}

void test_voltage_ripple_detector() {
    std::cout << "[TEST] Executando test_voltage_ripple_detector... ";
    head::config::PowerConfig cfg;
    cfg.maxRippleStdDevMv = 35.0;
    cfg.sampleHistorySize = 30;

    head::filters::VoltageRippleDetector detector(cfg);
    head::core::DiagnosticContext ctx;

    // Simula 25 leituras com ripple instável (+- 80mV de oscilação)
    for (int step = 0; step < 25; ++step) {
        ctx.activeAnomalies.clear();
        ctx.powerReadings.clear();
        
        head::core::PowerSensorReading r;
        r.chipName = "vrm_controller";
        r.sensorName = "in0_vcore";
        r.unit = "mV";
        // Oscila entre 1150mV e 1300mV (ripple violento)
        r.valueMilliUnits = 1200.0 + ((step % 2 == 0) ? 75.0 : -75.0);
        ctx.powerReadings.push_back(r);

        detector.process(ctx);
    }

    assert(!ctx.activeAnomalies.empty() && "Falha: Não detectou ripple de voltagem instável!");
    assert(ctx.activeAnomalies[0].type == head::core::AnomalyType::VOLTAGE_RIPPLE_INSTABILITY);
    std::cout << "PASSOU! (StdDev medido: " << ctx.activeAnomalies[0].numericMetrics["stddev_mv"] << " mV)\n";
}

int main() {
    std::cout << "\n=== EXECUTANDO SUÍTE DE TESTES UNITÁRIOS DO HEAD ===\n\n";
    test_clean_audio_no_ground_loop();
    test_synthetic_ground_loop_detection();
    test_voltage_ripple_detector();
    std::cout << "\nTODOS OS 3 TESTES PASSARAM COM SUCESSO!\n\n";
    return 0;
}
