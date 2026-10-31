#pragma once

#include "core/HardwareTopology.hpp"
#include <string>

namespace head::sources {

class TopologyDiscoverySource {
public:
    static core::SystemHardwareTopology scanSystem();
    static void printTopologyReport(const core::SystemHardwareTopology& topo);
    static std::string serializeToJson(const core::SystemHardwareTopology& topo);

private:
    static void scanAudioCards(core::SystemHardwareTopology& topo);
    static void scanUsbDevices(core::SystemHardwareTopology& topo);
    static void scanPciDevices(core::SystemHardwareTopology& topo);
    static void inspectAlsaMixer(int cardIndex, core::AudioCardInfo& cardInfo);
};

} // namespace head::sources
