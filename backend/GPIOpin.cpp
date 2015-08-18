#include "GPIOpin.h"

bool GPIOpin::set(std::fstream& fd, size_t value) {
    char buffer[4];
    memset(buffer, 0, 4);
    sprintf(buffer, "%d", value);
    fd.seekg(0, std::ios::beg);
    fd.write(buffer, 4);
    return !fd.fail();
}

bool GPIOpin::get(std::fstream& fd, size_t& value) {
    fd.seekg(0, std::ios::beg);
    fd >> value;
    return !fd.fail();
}

bool GPIOpin::isValid() const {
    return !mode.fail() && !pin.fail();
}

bool GPIOpin::setIndex(size_t _index) {
    index = _index;
    char buffer[256];
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/mode/gpio%d", index);
    mode.open(buffer);
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/pin/gpio%d", index);
    pin.open(buffer);
    return mode.is_open() && pin.is_open();
}

size_t GPIOpin::getIndex() const {
    return index;
}

bool GPIOpin::setMode(size_t value) {
    printf("SET Pin Mode %d %d\n", index, value);
    return set(mode, value);
}

bool GPIOpin::setValue(size_t value) {
    printf("SET Pin Value %d %d\n", index, value);
    return set(pin, value);
}

bool GPIOpin::getMode(size_t& value) {
    return get(mode, value);
}

bool GPIOpin::getValue(size_t& value) {
    return get(pin, value);
}
