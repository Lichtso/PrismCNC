#include <vector>
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
    bool set(int fd, size_t value) const;
    bool get(int fd, size_t& value) const;

    public:
    size_t index;
    GPIOpin(size_t index);
    ~GPIOpin();
    bool isValid() const;
    bool setMode(size_t value) const;
    bool setValue(size_t value) const;
    bool getMode(size_t& value) const;
    bool getValue(size_t& value) const;
};
