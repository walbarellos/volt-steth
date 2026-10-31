#include "sources/TopologyDiscoverySource.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <alsa/asoundlib.h>

namespace fs = std::filesystem;

namespace head::sources {

void TopologyDiscoverySource::inspectAlsaMixer(int cardIndex, core::AudioCardInfo& cardInfo) {
    std::string cardName = "hw:" + std::to_string(cardIndex);
    snd_mixer_t* handle = nullptr;

    if (snd_mixer_open(&handle, 0) < 0) return;
    if (snd_mixer_attach(handle, cardName.c_str()) < 0) {
        snd_mixer_close(handle);
        return;
    }
    if (snd_mixer_selem_register(handle, nullptr, nullptr) < 0 || snd_mixer_load(handle) < 0) {
        snd_mixer_close(handle);
        return;
    }

    snd_mixer_selem_id_t* sid;
    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, "Capture");

    snd_mixer_elem_t* elem = snd_mixer_find_selem(handle, sid);
    if (elem) {
        long minVol = 0, maxVol = 100, curVol = 0;
        snd_mixer_selem_get_capture_volume_range(elem, &minVol, &maxVol);
        snd_mixer_selem_get_capture_volume(elem, SND_MIXER_SCHN_FRONT_LEFT, &curVol);

        if (maxVol > minVol) {
            cardInfo.captureVolumePercent = static_cast<int>((curVol - minVol) * 100 / (maxVol - minVol));
        }

        int sw = 1;
        snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_FRONT_LEFT, &sw);
        cardInfo.captureMuted = (sw == 0);
    }

    snd_mixer_close(handle);
}

void TopologyDiscoverySource::scanAudioCards(core::SystemHardwareTopology& topo) {
    std::ifstream cardsFile("/proc/asound/cards");
    if (!cardsFile.is_open()) return;

    std::string line;
    while (std::getline(cardsFile, line)) {
        size_t firstNonSpace = line.find_first_not_of(" \t");
        if (firstNonSpace == std::string::npos || !std::isdigit(line[firstNonSpace])) {
            continue;
        }

        std::stringstream ss(line.substr(firstNonSpace));
        int index = 0;
        std::string cardId;
        if (ss >> index >> cardId) {
            core::AudioCardInfo info;
            info.cardIndex = index;
            info.cardId = cardId;

            // Segunda linha tem a descrição longa
            std::string descLine;
            if (std::getline(cardsFile, descLine)) {
                info.driverName = descLine;
                if (descLine.find("NVidia") != std::string::npos || descLine.find("HDMI") != std::string::npos) {
                    info.isDigital = true;
                } else {
                    info.isAnalog = true;
                }
            }

            inspectAlsaMixer(index, info);
            topo.audioCards.push_back(info);
        }
    }
}

void TopologyDiscoverySource::scanUsbDevices(core::SystemHardwareTopology& topo) {
    std::string usbBase = "/sys/bus/usb/devices";
    if (!fs::exists(usbBase)) return;

    try {
        for (const auto& entry : fs::directory_iterator(usbBase)) {
            std::string devDir = entry.path().string();
            std::string prodFile = devDir + "/product";
            std::string mfgFile = devDir + "/manufacturer";

            if (fs::exists(prodFile)) {
                std::ifstream pf(prodFile);
                std::string product;
                std::getline(pf, product);

                std::string mfg;
                if (fs::exists(mfgFile)) {
                    std::ifstream mf(mfgFile);
                    std::getline(mf, mfg);
                }

                core::UsbDeviceInfo u;
                u.bus = entry.path().filename().string();
                u.description = mfg.empty() ? product : (mfg + " " + product);
                topo.usbDevices.push_back(u);
            }
        }
    } catch (...) {}
}

void TopologyDiscoverySource::scanPciDevices(core::SystemHardwareTopology& topo) {
    std::string pciBase = "/sys/bus/pci/devices";
    if (!fs::exists(pciBase)) return;

    try {
        for (const auto& entry : fs::directory_iterator(pciBase)) {
            core::PciDeviceInfo p;
            p.pciAddress = entry.path().filename().string();
            topo.pciBuses.push_back(p);
        }
    } catch (...) {}
}

core::SystemHardwareTopology TopologyDiscoverySource::scanSystem() {
    core::SystemHardwareTopology topo;
    scanAudioCards(topo);
    scanUsbDevices(topo);
    scanPciDevices(topo);

    // Pre-flight check: valida se o canal analógico não está mutado ou zerado
    for (const auto& card : topo.audioCards) {
        if (card.isAnalog) {
            if (card.captureVolumePercent <= 5) {
                topo.preflightStatus = "WARNING_MUTED";
                topo.preflightWarning = "AVISO CRÍTICO: Placa Analógica (" + card.cardId + 
                                        ") está com volume de captura em " + 
                                        std::to_string(card.captureVolumePercent) + 
                                        "%. O sinal está sendo silenciado antes de chegar ao detector!";
            } else if (card.captureMuted) {
                topo.preflightStatus = "WARNING_MUTED";
                topo.preflightWarning = "AVISO CRÍTICO: Placa Analógica (" + card.cardId + 
                                        ") está MUTADA no mixer do ALSA!";
            }
            break;
        }
    }

    return topo;
}

void TopologyDiscoverySource::printTopologyReport(const core::SystemHardwareTopology& topo) {
    std::cout << "\n================================================================================\n";
    std::cout << "         VOLTSTETH: MAPEAMENTO DA TOPOLOGIA FÍSICA E PRÉ-FLIGHT CHECK           \n";
    std::cout << "================================================================================\n";
    
    std::cout << "\n[1. CONTROLADORES DE ÁUDIO IDENTIFICADOS (Eixo Vertical)]\n";
    for (const auto& card : topo.audioCards) {
        std::cout << "  • Placa " << card.cardIndex << " [" << card.cardId << "] - " 
                  << (card.isAnalog ? "ANALÓGICA (P2/Line)" : "DIGITAL (HDMI/GPU)") << "\n";
        std::cout << "     └─ Volume de Captura: " << card.captureVolumePercent << "% | "
                  << (card.captureMuted ? "MUTADA" : "DESMUTADA (Ativa)") << "\n";
    }

    std::cout << "\n[2. DISPOSITIVOS USB NO BARRAMENTO (Eixo Horizontal)]\n";
    for (const auto& u : topo.usbDevices) {
        std::cout << "  • USB [" << u.bus << "]: " << u.description << "\n";
    }

    std::cout << "\n[3. STATUS DO PRE-FLIGHT CHECK (Validação de Sensor)]\n";
    if (topo.preflightStatus == "OK") {
        std::cout << "  \033[32m✅ SENSOR ATIVO E OPERANTE: Volume adequado, medição analógica válida.\033[0m\n";
    } else {
        std::cout << "  \033[31m⚠️ " << topo.preflightWarning << "\033[0m\n";
        std::cout << "  \033[33m   Dica: Aumente o volume de captura ('Capture') no alsamixer ou pavucontrol.\033[0m\n";
    }
    std::cout << "================================================================================\n\n";
}

std::string TopologyDiscoverySource::serializeToJson(const core::SystemHardwareTopology& topo) {
    std::stringstream ss;
    ss << "{\"preflight_status\":\"" << topo.preflightStatus << "\""
       << ",\"preflight_warning\":\"" << topo.preflightWarning << "\""
       << ",\"audio_cards\":[";
    for (size_t i = 0; i < topo.audioCards.size(); ++i) {
        const auto& c = topo.audioCards[i];
        if (i > 0) ss << ",";
        ss << "{\"index\":" << c.cardIndex
           << ",\"id\":\"" << c.cardId << "\""
           << ",\"is_analog\":" << (c.isAnalog ? "true" : "false")
           << ",\"volume\":" << c.captureVolumePercent
           << ",\"muted\":" << (c.captureMuted ? "true" : "false") << "}";
    }
    ss << "],\"usb_devices\":[";
    for (size_t i = 0; i < topo.usbDevices.size(); ++i) {
        if (i > 0) ss << ",";
        ss << "{\"bus\":\"" << topo.usbDevices[i].bus << "\""
           << ",\"desc\":\"" << topo.usbDevices[i].description << "\"}";
    }
    ss << "]}";
    return ss.str();
}

} // namespace head::sources
