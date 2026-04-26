#include "seesaw.h"
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <cstring>
#include <cstdio>

Seesaw::Seesaw() : m_fd(-1), m_addr(0) {}

Seesaw::~Seesaw() { end(); }

void Seesaw::end()
{
    if (m_fd >= 0) {
        close(m_fd);
        m_fd = -1;
    }
}

bool Seesaw::begin(const std::string &i2cDevice, uint8_t addr)
{
    m_addr = addr;
    m_fd = open(i2cDevice.c_str(), O_RDWR);
    if (m_fd < 0) {
        perror("Seesaw: open i2c device failed");
        return false;
    }

    if (ioctl(m_fd, I2C_SLAVE, m_addr) < 0) {
        perror("Seesaw: ioctl I2C_SLAVE failed");
        end();
        return false;
    }

    // Software reset
    write8(STATUS_BASE, STATUS_SWRST, 0xFF);
    usleep(500000); // 500ms for reset

    // Verify HW ID
    uint8_t hwId = read8(STATUS_BASE, STATUS_HW_ID);
    printf("Seesaw @ 0x%02X HW ID: 0x%02X\n", addr, hwId);

    return true;
}

// ---- Low level I²C ----

bool Seesaw::write(uint8_t regBase, uint8_t reg,
                   const uint8_t *buf, uint8_t len)
{
    uint8_t packet[2 + len];
    packet[0] = regBase;
    packet[1] = reg;
    if (len > 0 && buf)
        memcpy(&packet[2], buf, len);

    if (::write(m_fd, packet, 2 + len) != 2 + len) {
        perror("Seesaw: write failed");
        return false;
    }
    return true;
}

bool Seesaw::read(uint8_t regBase, uint8_t reg,
                  uint8_t *buf, uint8_t len, uint32_t delayUs)
{
    // Write register address
    uint8_t addr[2] = { regBase, reg };
    if (::write(m_fd, addr, 2) != 2) {
        perror("Seesaw: read (write addr) failed");
        return false;
    }

    usleep(delayUs);

    // Read response
    if (::read(m_fd, buf, len) != len) {
        perror("Seesaw: read (read data) failed");
        return false;
    }
    return true;
}

bool Seesaw::write8(uint8_t regBase, uint8_t reg, uint8_t val)
{
    return write(regBase, reg, &val, 1);
}

uint8_t Seesaw::read8(uint8_t regBase, uint8_t reg)
{
    uint8_t val = 0;
    read(regBase, reg, &val, 1);
    return val;
}

// ---- Status ----

uint32_t Seesaw::getVersion()
{
    uint8_t buf[4] = {0};
    read(STATUS_BASE, STATUS_VERSION, buf, 4);
    return ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
         | ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
}

// ---- GPIO ----

void Seesaw::pinMode(uint8_t pin, uint8_t mode)
{
    uint32_t pinMask = (1UL << pin);
    uint8_t cmd[4] = {
        (uint8_t)(pinMask >> 24),
        (uint8_t)(pinMask >> 16),
        (uint8_t)(pinMask >> 8),
        (uint8_t)(pinMask)
    };

    if (mode == OUTPUT) {
        write(GPIO_BASE, GPIO_DIRSET_BULK, cmd, 4);
    } else {
        write(GPIO_BASE, GPIO_DIRCLR_BULK, cmd, 4);
        if (mode == INPUT_PULLUP) {
            write(GPIO_BASE, GPIO_PULLENSET, cmd, 4);
            write(GPIO_BASE, GPIO_BULK_SET,  cmd, 4); // pull high
        } else if (mode == INPUT_PULLDOWN) {
            write(GPIO_BASE, GPIO_PULLENSET, cmd, 4);
            write(GPIO_BASE, GPIO_BULK_CLR,  cmd, 4); // pull low
        }
    }
}

bool Seesaw::digitalRead(uint8_t pin)
{
    uint8_t buf[4] = {0};
    read(GPIO_BASE, GPIO_BULK, buf, 4);
    uint32_t val = ((uint32_t)buf[0] << 24) | ((uint32_t)buf[1] << 16)
                 | ((uint32_t)buf[2] << 8)  |  (uint32_t)buf[3];
    return (val >> pin) & 1;
}

// ---- Encoder ----

int32_t Seesaw::getEncoderPosition(uint8_t encoder)
{
    uint8_t buf[4] = {0};
    read(ENCODER_BASE, ENCODER_POSITION + encoder, buf, 4, 125);
    int32_t val = ((int32_t)buf[0] << 24) | ((int32_t)buf[1] << 16)
                | ((int32_t)buf[2] << 8)  |  (int32_t)buf[3];
    return val;
}

void Seesaw::setEncoderPosition(int32_t pos, uint8_t encoder)
{
    uint8_t cmd[4] = {
        (uint8_t)(pos >> 24),
        (uint8_t)(pos >> 16),
        (uint8_t)(pos >> 8),
        (uint8_t)(pos)
    };
    write(ENCODER_BASE, ENCODER_POSITION + encoder, cmd, 4);
}

void Seesaw::enableEncoderInterrupt(uint8_t encoder)
{
    write8(ENCODER_BASE, ENCODER_INTENSET + encoder, 0x01);
}

