#include "GPIOpin.h"

class SPI {
    static const size_t busPinIndex = 11;
    int handle;
    size_t slaveCount;
    GPIOpin *bus, *slaveCS;
    public:
    SPI(size_t slaveCount, uint32_t frequency);
    ~SPI();
    size_t getSlaveCount() const;
    uint32_t getMaxFrequency() const;
    bool transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size) const;
};
