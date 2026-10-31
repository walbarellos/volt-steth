#pragma once

#include "core/Types.hpp"

namespace head::core {

template <typename TData>
class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual bool initialize() = 0;
    virtual bool acquire(TData& outData) = 0;
    virtual void release() = 0;
    virtual std::string getName() const = 0;
};

using IAudioSource = IDataSource<AudioSampleFrame>;
using IPowerSource = IDataSource<std::vector<PowerSensorReading>>;
using IBusSource = IDataSource<std::vector<BusErrorReading>>;

} // namespace head::core
