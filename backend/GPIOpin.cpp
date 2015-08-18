#include "GPIOpin.h"

bool GPIOpin::set(int fd, size_t value) {
    if(!fd) return false;
    lseek(fd, 0, SEEK_SET);
    memset(buffer, 0, 4);
    sprintf(buffer, "%d", value);
    return write(fd, buffer, 4) == 4;
}

bool GPIOpin::get(int fd, size_t& value) {
    if(!fd) return false;
    lseek(fd, 0, SEEK_SET);
    if(read(fd, buffer, 4) != 4) return false;
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

bool GPIOpin::setMode(size_t value) {
    return set(mode, value);
}

bool GPIOpin::setValue(bool value) {
    return set(pin, value);
}

bool GPIOpin::getMode(size_t& value) {
    return get(mode, value);
}

bool GPIOpin::getValue(bool& value) {
    return get(pin, value);
}
