#include "L6470.h"

const size_t motorSteps = 200,
             driverSteps = 128;

L6470::L6470(SPI* _bus, size_t _slaveIndex)
    :bus(_bus), slaveIndex(_slaveIndex) {
    setParam(L6470::ParamName::ACC, 138);
    setParam(L6470::ParamName::DEC, 138);
    setParam(L6470::ParamName::MIN_SPEED, 0);
    setParam(L6470::ParamName::MAX_SPEED, 55); // 65
    setParam(L6470::ParamName::FS_SPD, 39);
    setParam(L6470::ParamName::OCD_TH, 4); // 8
    setParam(L6470::ParamName::STALL_TH, 50); // 64
}

bool L6470::set(uint8_t len, uint8_t key, uint32_t value) {
    uint8_t buffer[4];
    buffer[0] = key;
    buffer[1] = buffer[2] = buffer[3] = 0x00;
    for(size_t i = 1; i <= len; ++i)
        buffer[i] = value >> (8*(len-i));
    return bus->transfer(slaveIndex, buffer, 1+len);
}

bool L6470::get(uint8_t len, uint8_t key, uint32_t& value) {
    uint8_t buffer[4];
    buffer[0] = key;
    buffer[1] = buffer[2] = buffer[3] = 0x00;
    if(!bus->transfer(slaveIndex, buffer, 1+len))
        return false;
    value = 0;
    for(size_t i = 1; i <= len; ++i)
        value |= buffer[i] << (8*(len-i));
    return true;
}

bool L6470::setParam(ParamName param, uint32_t value) {
    uint8_t key = static_cast<uint32_t>(param);
    if(key == 0 || key >= sizeof(ParamSize))
        return false;
    return set((ParamSize[key]+7)/8, key, value);
}

bool L6470::getParam(ParamName param, uint32_t& value) {
    uint8_t key = static_cast<uint32_t>(param);
    if(key == 0 || key >= sizeof(ParamSize))
        return false;
    return get((ParamSize[key]+7)/8, key | 0x20, value);
}

bool L6470::run(uint32_t speed, bool forward) {
    return set(3, 0x50 | forward, speed & 0xFFFFF);
}

bool L6470::goUntil(uint32_t speed, bool forward, bool mark) {
    return set(3, 0x82 | (mark << 3) | forward, speed & 0xFFFFF);
}

bool L6470::releaseSW(bool forward, bool mark) {
    return set(0, 0x92 | (mark << 3) | forward, 0);
}

bool L6470::goHome(bool mark) {
    return set(0, 0x70 | (mark << 3), 0);
}

bool L6470::goTo(uint32_t position) {
    return set(3, 0x60, position & 0x3FFFFF);
}

bool L6470::goTo(uint32_t position, bool forward) {
    return set(3, 0x68 | forward, position & 0x3FFFFF);
}

bool L6470::stepClock(bool forward) {
    return set(0, 0x58 | forward, 0);
}

bool L6470::move(uint32_t microSteps, bool forward) {
    return set(3, 0x40 | forward, microSteps & 0x3FFFFF);
}

bool L6470::stop(bool immediately) {
    return set(0, 0xB0 | (immediately << 3), 0);
}

bool L6470::setIdle(bool immediately) {
    return set(0, 0xA0 | (immediately << 3), 0);
}

bool L6470::resetHome() {
    return set(0, 0xD8, 0);
}

bool L6470::resetDevice() {
    return set(0, 0xC0, 0);
}

bool L6470::resetFlags(uint32_t& status) {
    return get(2, 0xD0, status);
}

bool L6470::isErrorFlagSet(uint32_t value) {
    const uint32_t DriverErrorFlags =
        (uint32_t)DriverStatus::UVLO |
        (uint32_t)DriverStatus::TH_WRN |
        (uint32_t)DriverStatus::TH_SD |
        (uint32_t)DriverStatus::OCD |
        (uint32_t)DriverStatus::STEP_LOSS_A |
        (uint32_t)DriverStatus::STEP_LOSS_B;
    return (value & DriverErrorFlags) != DriverErrorFlags;
}

size_t L6470::getStepsPerRound() const {
    return motorSteps*driverSteps;
}
