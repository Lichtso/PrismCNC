#include <linux/spi/spidev.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <fcntl.h>
#include "SPI.h"

SPI::SPI(size_t _slaveCount, uint32_t frequency) :slaveCount(_slaveCount) {
    handle = open("/dev/spidev0.0", O_RDWR);
    if(!handle) {
        printf("SPI opening device failed: %s\n", strerror(errno));
        return;
    }

    uint8_t setting = SPI_MODE_3; // SPI_CPOL | SPI_CPHA
    if(ioctl(handle, SPI_IOC_WR_MODE, &setting) < 0)
        printf("SPI changing mode failed: %s\n", strerror(errno));

    setting = 8;
    if(ioctl(handle, SPI_IOC_WR_BITS_PER_WORD, &setting) < 0)
        printf("SPI changing bpw failed: %s\n", strerror(errno));

    if(ioctl(handle, SPI_IOC_WR_MAX_SPEED_HZ, &frequency) < 0)
        printf("SPI changing frequency failed: %s\n", strerror(errno));

    bus = new GPIOpin[3];
    for(size_t i = 0; i < 3; ++i) {
        bus[i].setIndex(busPinIndex+i);
        bus[i].setMode(2);
    }

    slaveCS = new GPIOpin[slaveCount];
    for(size_t i = 0; i < slaveCount; ++i) {
        slaveCS[i].setIndex(busPinIndex-slaveCount+i);
        slaveCS[i].setMode(1);
        slaveCS[i].setValue(1);
    }
}

SPI::~SPI() {
    if(!handle) return;
    close(handle);
    delete[] bus;
    delete[] slaveCS;
}

size_t SPI::getSlaveCount() const {
    return slaveCount;
}

uint32_t SPI::getMaxFrequency() const {
    if(!handle) return 0;
    uint32_t value = 0;
    ioctl(handle, SPI_IOC_RD_MAX_SPEED_HZ, &value);
    return value;
}

bool SPI::transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size) const {
    if(!handle) return false;

    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.len = 1;
    /*transfer.speed_hz = 5000000;
    transfer.delay_usecs = 1000;
    transfer.bits_per_word = 8;
    transfer.cs_change = 0;
    transfer.pad = 0;*/

    for(size_t i = 0; i < size; ++i) {
        slaveCS[slaveIndex].setValue(0);
        transfer.tx_buf = transfer.rx_buf = (uint64_t)&buffer[i];
        if(ioctl(handle, SPI_IOC_MESSAGE(1), &transfer) != transfer.len)
            return false;
        slaveCS[slaveIndex].setValue(1);
    }

    return true;
}
