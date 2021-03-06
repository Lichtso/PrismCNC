#include "L6470.h"

const float speedFixFactor = 0.01490116119; // pow(2, –28)/(0.000000250)
const size_t driverSteps = 128;

L6470::L6470(SPI* _bus, size_t _slaveIndex)
    :bus(_bus), slaveIndex(_slaveIndex), motorSteps(200) {
    setParam(ParamName::ACC, 180); // 138
    setParam(ParamName::DEC, 180); // 138
    setParam(ParamName::MIN_SPEED, 0);
    setParam(ParamName::MAX_SPEED, 50); // 65
    setParam(ParamName::FS_SPD, 39);
    setParam(ParamName::OCD_TH, 4); // 8
    setParam(ParamName::STALL_TH, 50); // 64
    resetHome();
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
    absPos = 0;
    return set(0, 0xD8, 0);
}

bool L6470::resetDevice() {
    absPos = 0;
    return set(0, 0xC0, 0);
}

bool L6470::resetFlags(uint32_t& status) {
    return get(2, 0xD0, status);
}

bool L6470::runInHz(float speed) {
    return run(fabsf(speed/speedFixFactor*motorSteps), speed >= 0);
}

bool L6470::goToInTurns(float position) {
    int64_t dstPos = position*motorSteps*driverSteps;
    return goTo(dstPos);
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
        return "Hardware Button";
    return NULL;
}

bool L6470::updatePosition() {
    const int64_t wrapAround = 0x00400000,
          wrapMask = wrapAround-1,
          signMask = wrapAround>>1;

    int32_t prevPos = absPos&wrapMask, postPos;
    if(!getParam(ParamName::ABS_POS, (uint32_t&)postPos)) return false;
    bool signPrev = prevPos&signMask, signPost = postPos&signMask;
    prevPos |= (~wrapMask)*signPrev;
    postPos |= (~wrapMask)*signPost;
    int64_t diff = postPos-prevPos;

    auto _absPos = absPos;
    absPos &= ~wrapMask;
    absPos |= postPos&wrapMask;
    if(signPrev != signPost && std::abs(diff) < (wrapAround>>2)) {
        if(signPrev)
            absPos += wrapAround;
        else
            absPos -= wrapAround;
    }

    return true;
}

float L6470::getSpeedInHz() {
    uint32_t speed;
    getParam(ParamName::SPEED, speed);
    return speed*speedFixFactor/motorSteps;
}

float L6470::getPositionInTurns() {
    return (float)absPos/(motorSteps*driverSteps);
}

bool L6470::isAtPositionInTurns(float position) {
    int64_t dstPos = position*motorSteps*driverSteps;
    return (absPos == dstPos);
}
