#include "sources/PcieAerSource.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

namespace head::sources {

PcieAerSource::PcieAerSource(const config::BusConfig& config)
    : m_config(config) {}

bool PcieAerSource::initialize() {
    m_monitoredDevices.clear();
    if (!m_config.checkAerErrors || !fs::exists(m_config.pciBaseDir)) {
        return true;
    }

    try {
        for (const auto& entry : fs::directory_iterator(m_config.pciBaseDir)) {
            if (entry.is_directory()) {
                m_monitoredDevices.push_back(entry.path().string());
            }
        }
    } catch (...) {}

    return true;
}

bool PcieAerSource::acquire(std::vector<core::BusErrorReading>& outData) {
    outData.clear();
    auto now = std::chrono::steady_clock::now();

    for (const auto& devPath : m_monitoredDevices) {
        std::string aerCorr = devPath + "/aer_dev_correctable";
        std::string aerFatal = devPath + "/aer_dev_fatal";
        std::string aerNonFatal = devPath + "/aer_dev_nonfatal";

        unsigned int cErr = 0, fErr = 0, nfErr = 0;
        bool hasAer = false;

        std::ifstream fc(aerCorr);
        if (fc >> cErr) hasAer = true;
        std::ifstream ff(aerFatal);
        if (ff >> fErr) hasAer = true;
        std::ifstream fnf(aerNonFatal);
        if (fnf >> nfErr) hasAer = true;

        if (hasAer && (cErr > 0 || fErr > 0 || nfErr > 0)) {
            core::BusErrorReading reading;
            reading.pciAddress = fs::path(devPath).filename().string();
            reading.correctableErrors = cErr;
            reading.fatalErrors = fErr;
            reading.nonFatalErrors = nfErr;
            reading.timestamp = now;
            outData.push_back(reading);
        }
    }

    return true;
}

void PcieAerSource::release() {
    m_monitoredDevices.clear();
}

} // namespace head::sources
