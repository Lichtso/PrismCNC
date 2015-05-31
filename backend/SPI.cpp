#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "SPI.h"

static bool setPin(size_t pin, size_t value, bool mode = false) {
    const char* key = (mode) ? "mode" : "pin";
    char buffer[256];
    sprintf(buffer, "/sys/devices/virtual/misc/gpio/%s/gpio%d", key, pin);
    int fp = open(buffer, O_RDWR);
    if(!fp) return false;
    lseek(fp, 0, SEEK_SET);
    memset(buffer, 0, 4);
    sprintf(buffer, "%d", value);
    if(write(fp, buffer, 4) != 4) return false;
    close(fp);
    return true;
}

SPI::SPI(size_t _slaveCount) :slaveCount(_slaveCount) {
    for(size_t i = lowestPin-slaveCount; i < lowestPin; ++i)
        setPin(i, 1, true);

    for(size_t i = lowestPin; i < lowestPin+3; ++i)
        setPin(i, 2, true);

    handle = open("/dev/spidev0.0", O_RDWR);
    if(handle) {
        char setting = SPI_NO_CS; // SPI_CPHA SPI_CPOL
        ioctl(handle, SPI_IOC_WR_MODE, &setting);
        /*setting = 0;
        ioctl(handle, SPI_IOC_WR_LSB_FIRST, &setting);
        setting = 8;
        ioctl(handle, SPI_IOC_WR_BITS_PER_WORD, &setting);*/
    }
}

uint32_t SPI::getMaxFrequency() {
    if(!handle)
        return 0;
    uint32_t value = 0;
    ioctl(handle, SPI_IOC_RD_MAX_SPEED_HZ, &value);
    return value;
}

SPI::~SPI() {
    if(handle)
        close(handle);
}

bool SPI::transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size) {
    if(!handle)
        return false;

    for(size_t i = 0; i < slaveCount; ++i) {
        printf("Selecting %d : %d\n", i, i != slaveIndex);
        if(!setPin(lowestPin-slaveCount+i, i != slaveIndex))
            return false;
    }

    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.tx_buf = (uint64_t)buffer;
    transfer.rx_buf = (uint64_t)buffer;
    transfer.len = size;
    transfer.speed_hz = 500000;
    //transfer.cs_change = 1;
    //transfer.delay_usecs = 1000;

    printf("transfering %d\n", size);
    if(ioctl(handle, SPI_IOC_MESSAGE(1), &transfer) != 0)
        return false;

    for(size_t i = 0; i < slaveCount; ++i) {
        printf("Deselecting %d\n", i);
        if(!setPin(lowestPin-slaveCount+i, 1))
            return false;
    }

    return true;
}
