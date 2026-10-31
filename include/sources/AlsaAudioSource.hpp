#pragma once

#include "core/IDataSource.hpp"
#include "config/AppConfig.hpp"
#include <alsa/asoundlib.h>
#include <vector>

namespace head::sources {

class AlsaAudioSource : public core::IAudioSource {
public:
    explicit AlsaAudioSource(const config::AudioConfig& config);
    ~AlsaAudioSource() override;

    bool initialize() override;
    bool acquire(core::AudioSampleFrame& outData) override;
    void release() override;
    std::string getName() const override { return "AlsaAudioSource"; }

private:
    config::AudioConfig m_config;
    snd_pcm_t* m_pcmHandle{nullptr};
    std::vector<int16_t> m_rawBuffer;
    bool m_initialized{false};
    bool m_fallbackMode{false};
};

} // namespace head::sources
