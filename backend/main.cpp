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
        point.coords[0] = sin(angle)*radius;
        point.coords[1] = cos(angle)*radius;
        point.coords[2] = 0.0;
        printf("%d %f %f\n", p, point.coords[0], point.coords[1]);
    }

    for(size_t p = 0; p < pCount; ++p) {
        for(size_t i = 0; i < motorCount; ++i) {
            motors[i]->run(t*20, true);
            uint32_t value;
            motors[i]->resetFlags(value);
            if(value & L6470::DriverStatus::SW_EVN) {
                printf("Stop signal\n");
                break;
            }
            if(value & L6470::DriverStatus::UVLO ||
               value & L6470::DriverStatus::TH_WRN ||
               value & L6470::DriverStatus::TH_SD ||
               value & L6470::DriverStatus::OCD ||
               value & L6470::DriverStatus::STEP_LOSS_A ||
               value & L6470::DriverStatus::STEP_LOSS_B) {
                printf("Error signal\n");
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
