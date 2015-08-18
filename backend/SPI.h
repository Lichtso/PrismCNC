#include "GPIOpin.h"

class SPI {
    static const size_t busPinIndex = 11;
    int handle;
    public:
    std::vector<GPIOpin> bus, slaveCS;
    SPI(size_t slaveCount);
    ~SPI();
    uint32_t getMaxFrequency();
    bool transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size);
};
