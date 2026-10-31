#include "sources/AlsaAudioSource.hpp"
#include <iostream>
#include <cmath>

namespace head::sources {

AlsaAudioSource::AlsaAudioSource(const config::AudioConfig& config)
    : m_config(config) {
    m_rawBuffer.resize(m_config.bufferSize * m_config.channels);
}

AlsaAudioSource::~AlsaAudioSource() {
    release();
}

bool AlsaAudioSource::initialize() {
    int err = snd_pcm_open(&m_pcmHandle, m_config.device.c_str(), SND_PCM_STREAM_CAPTURE, SND_PCM_NONBLOCK);
    if (err < 0) {
        std::cerr << "[AlsaAudioSource] Aviso: Falha ao abrir dispositivo ALSA '" 
                  << m_config.device << "' (" << snd_strerror(err) << "). Operando em modo tolerante.\n";
        m_fallbackMode = true;
        m_initialized = true;
        return true;
    }

    snd_pcm_hw_params_t* hwParams;
    snd_pcm_hw_params_alloca(&hwParams);
    snd_pcm_hw_params_any(m_pcmHandle, hwParams);
    snd_pcm_hw_params_set_access(m_pcmHandle, hwParams, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(m_pcmHandle, hwParams, SND_PCM_FORMAT_S16_LE);
    
    unsigned int rate = m_config.sampleRate;
    snd_pcm_hw_params_set_rate_near(m_pcmHandle, hwParams, &rate, nullptr);
    snd_pcm_hw_params_set_channels(m_pcmHandle, hwParams, m_config.channels);
    
    snd_pcm_uframes_t frames = m_config.bufferSize;
    snd_pcm_hw_params_set_period_size_near(m_pcmHandle, hwParams, &frames, nullptr);

    err = snd_pcm_hw_params(m_pcmHandle, hwParams);
    if (err < 0) {
        std::cerr << "[AlsaAudioSource] Erro ao definir hw params: " << snd_strerror(err) << "\n";
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
        m_fallbackMode = true;
        m_initialized = true;
        return true;
    }

    snd_pcm_prepare(m_pcmHandle);
    m_initialized = true;
    return true;
}

bool AlsaAudioSource::acquire(core::AudioSampleFrame& outData) {
    if (!m_initialized) return false;

    outData.sampleRate = m_config.sampleRate;
    outData.timestamp = std::chrono::steady_clock::now();
    outData.samples.resize(m_config.bufferSize);

    if (m_fallbackMode || !m_pcmHandle) {
        std::fill(outData.samples.begin(), outData.samples.end(), 0.0f);
        return true;
    }

    snd_pcm_sframes_t framesRead = snd_pcm_readi(m_pcmHandle, m_rawBuffer.data(), m_config.bufferSize);
    if (framesRead < 0) {
        if (framesRead == -EPIPE) {
            snd_pcm_prepare(m_pcmHandle);
        } else if (framesRead != -EAGAIN) {
            snd_pcm_recover(m_pcmHandle, framesRead, 0);
        }
        std::fill(outData.samples.begin(), outData.samples.end(), 0.0f);
        return true;
    }

    for (size_t i = 0; i < static_cast<size_t>(framesRead); ++i) {
        outData.samples[i] = static_cast<float>(m_rawBuffer[i * m_config.channels]) / 32768.0f;
    }
    for (size_t i = static_cast<size_t>(framesRead); i < m_config.bufferSize; ++i) {
        outData.samples[i] = 0.0f;
    }

    return true;
}

void AlsaAudioSource::release() {
    if (m_pcmHandle) {
        snd_pcm_close(m_pcmHandle);
        m_pcmHandle = nullptr;
    }
    m_initialized = false;
}

} // namespace head::sources
