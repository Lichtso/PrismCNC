#include "L6470.h"

const size_t motorMacroSteps = 200,
             motorMicroSteps = 128,
             motorSteps = motorMacroSteps*motorMicroSteps;

int main(int argc, char** argv) {
    SPI bus(3);

    L6470* motors[3];
    for(size_t i = 0; i < sizeof(motors)/sizeof(void*); ++i)
        motors[i] = new L6470(&bus, i);

    double timeStep = 0.001;
    for(double a = 0.0; a < 2.0*M_PI; a += 2.0*M_PI*timeStep) {
        usleep(timeStep*1000000);
        float speed = 1000, m0 = sin(a)*speed, m1 = cos(a)*speed;
        motors[0]->run(abs(m0), (m0 > 0.0));
        motors[1]->run(abs(m1), (m1 > 0.0));
    }
    motors[0]->setIdle(false);
    motors[1]->setIdle(false);

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
