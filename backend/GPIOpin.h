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
    FILE *mode, *pin;
    bool set(FILE* fd, size_t value);
    size_t get(FILE* fd);

    public:
    GPIOpin();
    ~GPIOpin();
    bool isValid() const;
    void setIndex(size_t index);
    size_t getIndex() const;
    void setMode(size_t value);
    size_t getMode();
    void setValue(size_t value);
    size_t getValue();
};
