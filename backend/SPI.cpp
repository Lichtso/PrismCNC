#include <errno.h>
#include <sys/ioctl.h>
#include <linux/spi/spidev.h>
#include "SPI.h"
#define slavePin(index) (lowestPin-slaveCount+(index))

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

    uint32_t frequency = 5000000;
    if(ioctl(handle, SPI_IOC_WR_MAX_SPEED_HZ, &frequency) < 0) {
        printf("SPI changing frequency failed: %s\n", strerror(errno));
        return;
    }

    bool success = true;
    for(size_t i = lowestPin-slaveCount; i < lowestPin; ++i)
        success &= setPin(i, 1, true);

    for(size_t i = lowestPin; i < lowestPin+3; ++i)
        success &= setPin(i, 2, true);

    success &= setPin(slavePin(-1), 1, true);
    success &= setPin(slavePin(-1), 1);
    
    if(!success) {
        printf("SPI setting pin mode failed.\n");
        return;
    }
}

SPI::~SPI() {
    setPin(slavePin(-1), 0);
    if(handle)
        close(handle);
}

uint32_t SPI::getMaxFrequency() {
    if(!handle)
        return 0;
    printf("Handle is open %p:%d.\n", this, handle);
    uint32_t value = 0;
    ioctl(handle, SPI_IOC_RD_MAX_SPEED_HZ, &value);
    return value;
}

bool SPI::transfer(size_t slaveIndex, uint8_t* buffer, uint64_t size) {
    if(!handle) {
        printf("Handle is NOT open %p:%d.\n", this, handle);
        return false;
    }

    struct spi_ioc_transfer transfer;
    memset(&transfer, 0, sizeof(transfer));
    transfer.len = 1;
    /*transfer.speed_hz = 5000000;
    transfer.delay_usecs = 1000;
    transfer.bits_per_word = 8;
    transfer.cs_change = 0;
    transfer.pad = 0;*/

    for(size_t i = 0; i < slaveCount; ++i)
        if(!setPin(slavePin(i), 1))
            return false;

    for(size_t i = 0; i < size; ++i) {
        transfer.tx_buf = transfer.rx_buf = (uint64_t)&buffer[i];
        if(!setPin(slavePin(slaveIndex), 0) ||
           ioctl(handle, SPI_IOC_MESSAGE(1), &transfer) != transfer.len ||
           !setPin(slavePin(slaveIndex), 1))
            return false;
    }

    return true;
}
