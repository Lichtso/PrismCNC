#include "GPIOpin.h"

bool GPIOpin::set(std::fstream& fd, size_t value) const {
    char buffer[4];
    memset(buffer, 0, 4);
    sprintf(buffer, "%d", value);
    fd.seekg(0, std::ios::beg);
    return fd.write(buffer, 4) == 4 && !fd.fail();
}

bool GPIOpin::get(std::fstream& fd, size_t& value) const {
    fd.seekg(0, std::ios::beg);
    value << fd;
    return !fd.fail();
}

GPIOpin::GPIOpin(size_t _index) :index(_index) {
    char buffer[256];
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/mode/gpio%d", index);
    mode.open(buffer);
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/pin/gpio%d", index);
    pin.open(buffer);
}

bool GPIOpin::isValid() const {
    return mode.is_open() && pin.is_open();
}

bool GPIOpin::setMode(size_t value) const {
    printf("SET Pin Mode %d %d\n", index, value);
    return set(mode, value);
}

bool GPIOpin::setValue(size_t value) const {
    printf("SET Pin Value %d %d\n", index, value);
    return set(pin, value);
}

bool GPIOpin::getMode(size_t& value) const {
    return get(mode, value);
}

bool GPIOpin::getValue(size_t& value) const {
    return get(pin, value);
}
