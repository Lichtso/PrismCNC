#include <fstream>
#include <iostream>
#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

class GPIOpin {
    size_t index;
    std::fstream mode, pin;
    bool set(std::fstream& fd, size_t value);
    bool get(std::fstream& fd, size_t& value);

    public:
    bool isValid() const;
    bool setIndex(size_t index);
    size_t getIndex() const;
    bool setMode(size_t value);
    bool setValue(size_t value);
    bool getMode(size_t& value);
    bool getValue(size_t& value);
};
