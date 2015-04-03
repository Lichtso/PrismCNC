#include "L6470.h"

int main(int argc, char** argv) {
    SPI bus(3);
    printf("Max speed: %d Hz\n", bus.getMaxFrequency());

    return 0;
}
