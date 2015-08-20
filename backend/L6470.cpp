#include "L6470.h"

const size_t driverSteps = 128;

L6470::L6470(SPI* _bus, size_t _slaveIndex)
    :bus(_bus), slaveIndex(_slaveIndex), motorSteps(200), mmPerRound(1.0) {
    setParam(L6470::ParamName::ACC, 138);
    setParam(L6470::ParamName::DEC, 138);
    setParam(L6470::ParamName::MIN_SPEED, 0);
    setParam(L6470::ParamName::MAX_SPEED, 55); // 65
    setParam(L6470::ParamName::FS_SPD, 39);
    setParam(L6470::ParamName::OCD_TH, 4); // 8
    setParam(L6470::ParamName::STALL_TH, 50); // 64
    getStatus();
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

const char* L6470::getStatus() {
    uint32_t value;
    if(!resetFlags(value))
        return "PCI";
    if(!(value & (uint32_t)L6470::DriverStatus::TH_SD))
        return "Thermal Shutdown"; // 160°C
    if(!(value & (uint32_t)L6470::DriverStatus::TH_WRN))
        return "Thermal Warning"; // 130°C
    if(!(value & (uint32_t)L6470::DriverStatus::OCD))
        return "Overcurrent";
    if(!(value & (uint32_t)L6470::DriverStatus::UVLO))
        return "Undervoltage Lockout";
    if(!(value & (uint32_t)L6470::DriverStatus::STEP_LOSS_B) ||
       !(value & (uint32_t)L6470::DriverStatus::STEP_LOSS_A))
        return "Step Loss";
    if(value & (uint32_t)L6470::DriverStatus::SW_EVN)
        return "Switch";
    return NULL;
}

bool L6470::updatePosition() {
    const int64_t overflowRef = 0x00400000;

    int32_t prevPos = absPos&(overflowRef-1), postPos;
    if(!getParam(L6470::ParamName::ABS_POS, (uint32_t&)postPos)) return false;
    bool signPrev = prevPos&(overflowRef>>1), signPost = postPos&(overflowRef>>1);
    prevPos |= 0xFFE00000*signPrev;
    postPos |= 0xFFE00000*signPost;

    absPos &= ~(overflowRef-1);
    int64_t diff = postPos-prevPos;
    printf("Motor %d diff %lld\n", slaveIndex, diff);

    if(std::abs(diff) > (overflowRef>>2)) {
        if(signPost) {
            absPos += overflowRef;
            printf("Motor %d overflow\n", slaveIndex);
        }else{
            absPos -= overflowRef;
            printf("Motor %d underflow\n", slaveIndex);
        }
    }
    absPos |= postPos;
    printf("Motor %d pos %lld\n", slaveIndex, absPos);

    return true;
}

float L6470::getPositionInMM() {
    return (float)absPos/(motorSteps*driverSteps)*mmPerRound;
}
