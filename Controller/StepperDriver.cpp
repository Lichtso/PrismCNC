#include "StepperDriver.h"

StepperDriver::StepperDriver(SPI* _bus, size_t _slaveIndex)
    :bus(_bus), slaveIndex(_slaveIndex) { }

bool StepperDriver::set(uint8_t len, uint8_t key, uint32_t value) {
    if(len > 3) return false;
    uint8_t buffer[4];
    buffer[0] = key;
    for(size_t i = 0; i < len; ++i)
        buffer[i] = (value >> (8*(len-i-1))) & 0xFF;
    return spi->transfer(slaveIndex, buffer, len+1);
}

bool StepperDriver::get(uint8_t len, uint8_t key, uint32_t value) {
    if(len == 0 || len > 3) return false;
    uint8_t buffer[4];
    buffer[0] = key;
    buffer[1] = buffer[2] = buffer[3] = 0x00;
    if(!spi->transfer(slaveIndex, buffer, len+1))
        return false;
    value = 0x00;
    for(size_t i = 0; i < len; ++i)
        value |= buffer[i] << (8*(len-i-1));
    return true;
}

bool StepperDriver::setParam(uint8_t key, uint32_t value) {
    if(key == 0 || key >= sizeof(paramSize))
        return false;
    return set((paramSize[key]+7)/8, key, value);
}

bool StepperDriver::getParam(uint8_t key, uint32_t& value) {
    if(key == 0 || key >= sizeof(paramSize))
        return false;
    return get((paramSize[key]+7)/8, key|0x20, value);
}
