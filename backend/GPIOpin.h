#include <math.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>

class GPIOpin {
    int mode, pin;
    bool set(int fd, size_t value);
    bool get(int fd, size_t& value);

    public:
    size_t index;
    GPIOpin(size_t _index) :index(_index);
    ~GPIOpin();
    bool setMode(size_t value);
    bool setValue(bool value);
    bool getMode(size_t& value);
    bool getValue(bool& value);
};
