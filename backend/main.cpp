#include "L6470.h"

const size_t motorCount = 3;

int main(int argc, char** argv) {
    SPI bus(motorCount, 5000000);
    L6470* motors[motorCount];
    GPIOpin motorDriversActive;
    motorDriversActive.setIndex(7);
    motorDriversActive.setMode(1);
    motorDriversActive.setValue(1);

    size_t value;
    printf("GET Pin Value 7 %d %d\n", motorDriversActive.getValue(value), value);

    for(size_t i = 0; i < motorCount; ++i)
        motors[i] = new L6470(&bus, i);

    /*for(size_t t = 0; t < 1000; ++t) {
        for(size_t i = 0; i < motorCount; ++i)
            motors[i]->run(t*20, true);
        usleep(1000);
    }*/

    for(size_t i = 0; i < motorCount; ++i)
        motors[i]->run(1000, true);
    usleep(900000);

    for(size_t i = 0; i < motorCount; ++i)
        motors[i]->setIdle(false);

    /*FILE* pipe = popen("frontend/bin/server", "w");
    if(!pipe) {
        printf("Could not open child process.\n");
        return 1;
    }
    char buffer[512];
    while(fread(buffer, 1, sizeof(buffer), pipe)) {
        printf("%s\n", buffer);
    }
    pclose(pipe);*/

    motorDriversActive.setValue(0);
    return 0;
}
