#include "L6470.h"
#define MOTOR_STEPS 200
#define MOTOR_MICROSTEPS 128

int main(int argc, char** argv) {
    SPI bus(3);
    printf("Max speed: %d Hz\n", bus.getMaxFrequency());

    uint32_t value;
    L6470* motors[1];
    for(size_t i = 0; i < sizeof(motors)/sizeof(void*); ++i)
        motors[i] = new L6470(&bus, i);

    motors[0]->setParam(L6470::ParamName::MAX_SPEED, 8000);
    motors[0]->resetHome();
    motors[0]->move(8*MOTOR_STEPS*MOTOR_MICROSTEPS, true);
    while(true) {
        usleep(10000);
        motors[0]->getParam(L6470::ParamName::ABS_POS, value);
        printf("ABS_POS: %d\n", value);
        motors[0]->getParam(L6470::ParamName::SPEED, value);
        printf("SPEED: %d\n", value);
        if(value == 0) break;
    }
    motors[0]->setIdle(false);

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

    return 0;
}
