#include "L6470.h"

const size_t motorCount = 3;

struct ControlPoint {
    float coord[3];
};

int main(int argc, char** argv) {
    SPI bus(motorCount, 5000000);
    L6470* motors[motorCount];
    GPIOpin motorDriversActive;
    motorDriversActive.setIndex(7);
    motorDriversActive.setMode(1);
    motorDriversActive.setValue(1);

    for(size_t i = 0; i < motorCount; ++i)
        motors[i] = new L6470(&bus, i);

    size_t pCount = 128;
    ControlPoint points[pCount];
    for(size_t p = 0; p < pCount; ++p) {
        float angle = (float)p/pCount*M_PI*2.0, radius = 10.0;
        ControlPoint& point = points[p];
        point.coord[0] = sin(angle)*radius;
        point.coord[1] = cos(angle)*radius;
        point.coord[2] = 0.0;
        printf("%d %f %f\n", p, point.coord[0], point.coord[1]);
    }

    bool running = true;
    for(size_t p = 0; running && p < 1000; ++p) {
        for(size_t i = 0; i < motorCount; ++i) {
            motors[i]->run(p*20, true);
            uint32_t value;
            motors[i]->resetFlags(value);
            if(value & (uint32_t)L6470::DriverStatus::SW_EVN) {
                printf("Stop signal\n");
                running = false;
                break;
            }
            if(value & L6470::DriverErrorFlags) {
                printf("Error signal\n");
                running = false;
                break;
            }

            motors[i]->getParam(L6470::ParamName::ABS_POS, value);
            printf("%d %d", i, value);
            motors[i]->getParam(L6470::ParamName::MAX_SPEED, value);
            printf(" %d\n", value);
        }
        usleep(1000);
    }

    /*for(size_t i = 0; i < motorCount; ++i)
        motors[i]->move(motors[i]->getStepsPerRound(), true);
    usleep(900000);*/

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
