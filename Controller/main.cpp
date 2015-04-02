#include "StepperDriver.h"

int main(int argc, char** argv) {
    SPI bus(3);
    printf("Max speed: %d Hz\n", bus.getMaxFrequency());

    return 0;
}
