#include "L6470.h"
#define MOTOR_STEPS 200
#define MOTOR_MICROSTEPS 128

void waitUntilDone(L6470* motor) {
    uint32_t value;
    do {
        usleep(10000);
        motor->getParam(L6470::ParamName::SPEED, value);
        printf("SPEED: %d\n", value);
    } while(value);
}

int main(int argc, char** argv) {
    SPI bus(3);

    L6470* motors[3];
    for(size_t i = 0; i < sizeof(motors)/sizeof(void*); ++i)
        motors[i] = new L6470(&bus, i);

    motors[0]->setParam(L6470::ParamName::STALL_TH, 10);
    motors[0]->setParam(L6470::ParamName::MAX_SPEED, 30);
    motors[0]->resetHome();
    motors[0]->move(MOTOR_STEPS*MOTOR_MICROSTEPS, true);
    waitUntilDone(motors[0]);
    motors[0]->move(MOTOR_STEPS*MOTOR_MICROSTEPS, false);
    waitUntilDone(motors[0]);
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
