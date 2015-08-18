#include "GPIOpin.h"

bool GPIOpin::set(int fd, size_t value) const {
    if(!fd) return false;
    char buffer[4];
    memset(buffer, 0, 4);
    sprintf(buffer, "%d", value);
    lseek(fd, 0, SEEK_SET);
    return write(fd, buffer, 4) == 4;
}

bool GPIOpin::get(int fd, size_t& value) const {
    if(!fd) return false;
    lseek(fd, 0, SEEK_END);
    size_t length = lseek(fd, 0, SEEK_CUR);
    lseek(fd, 0, SEEK_SET);
    char buffer[length];
    if(read(fd, buffer, length) != length) return false;
    sscanf(buffer, "%d", &value);
    return true;
}

GPIOpin::GPIOpin(size_t _index) :index(_index) {
    char buffer[256];
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/mode/gpio%d", index);
    mode = open(buffer, O_RDWR);
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/pin/gpio%d", index);
    pin = open(buffer, O_RDWR);
}

GPIOpin::~GPIOpin() {
    if(mode) close(mode);
    if(pin) close(pin);
}

bool GPIOpin::isValid() const {
    return mode && pin;
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
