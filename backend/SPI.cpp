#include <linux/spi/spidev.h>
#include "SPI.h"

SPI::SPI(size_t slaveCount, uint32_t frequency) {
    handle = open("/dev/spidev0.0", O_RDWR);
    if(!handle) {
        printf("SPI opening device failed: %s\n", strerror(errno));
        return;
    }

    uint8_t setting = 0; // SPI_NO_CS SPI_CPHA SPI_CPOL
    if(ioctl(handle, SPI_IOC_WR_MODE, &setting) < 0) {
        printf("SPI changing mode failed: %s\n", strerror(errno));
        return;
    }

    setting = 8;
    if(ioctl(handle, SPI_IOC_WR_BITS_PER_WORD, &setting) < 0) {
        printf("SPI changing bpw failed: %s\n", strerror(errno));
        return;
    }

    if(ioctl(handle, SPI_IOC_WR_MAX_SPEED_HZ, &frequency) < 0) {
        printf("SPI changing frequency failed: %s\n", strerror(errno));
        return;
    }

    bool success = true;
    for(size_t i = 0; i < 3; ++i) {
        GPIOpin pin(busPinIndex+i);
        success &= pin.setMode(2);
        bus.push_back(pin);
    }

    for(size_t i = 0; i < slaveCount; ++i) {
        GPIOpin pin(busPinIndex-slaveCount+i);
        success &= pin.setMode(1);
        success &= pin.setValue(1)
        slaveCS.push_back(pin);
    }

    if(!success) {
        printf("SPI setting pin mode failed.\n");
        return;
    }
}

SPI::~SPI() {
    if(handle)
        close(handle);
}

size_t SPI::getSlaveCount() const {
    return slaveCS.size();
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

    /*for(size_t i = 0; i < slaveCS.size(); ++i)
        if(!slaveCS[i].setValue(1))
            return false;*/

    for(size_t i = 0; i < size; ++i) {
        transfer.tx_buf = transfer.rx_buf = (uint64_t)&buffer[i];
        if(!slaveCS[slaveIndex].setValue(0) ||
           ioctl(handle, SPI_IOC_MESSAGE(1), &transfer) != transfer.len ||
           !slaveCS[slaveIndex].setValue(1))
            return false;
    }

    return true;
}
