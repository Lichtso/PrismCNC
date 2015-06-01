#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>

class SPI {
    static const size_t lowestPin = 11;
    public:
    int handle;
    size_t slaveCount;
    SPI(size_t slaveCount);
    ~SPI();
    uint32_t getMaxFrequency();
    bool transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size);
};
