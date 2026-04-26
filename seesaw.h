#pragma once
#include <cstdint>
#include <string>

class Seesaw {
public:
    // GPIO modes
    static const uint8_t INPUT          = 0x00;
    static const uint8_t OUTPUT         = 0x01;
    static const uint8_t INPUT_PULLUP   = 0x02;
    static const uint8_t INPUT_PULLDOWN = 0x03;

    Seesaw();
    ~Seesaw();

    bool begin(const std::string &i2cDevice, uint8_t addr);
    void end();

    // GPIO
    void pinMode(uint8_t pin, uint8_t mode);
    bool digitalRead(uint8_t pin);

    // Encoder
    int32_t getEncoderPosition(uint8_t encoder);
    void setEncoderPosition(int32_t pos, uint8_t encoder);
    void enableEncoderInterrupt(uint8_t encoder);

    // Status
    uint32_t getVersion();

private:
    int      m_fd;
    uint8_t  m_addr;

    // Register bases
    static const uint8_t STATUS_BASE  = 0x00;
    static const uint8_t GPIO_BASE    = 0x01;
    static const uint8_t ENCODER_BASE = 0x11;

    // Status registers
    static const uint8_t STATUS_HW_ID  = 0x01;
    static const uint8_t STATUS_VERSION = 0x02;
    static const uint8_t STATUS_SWRST  = 0x7F;

    // GPIO registers
    static const uint8_t GPIO_DIRSET_BULK = 0x02;
    static const uint8_t GPIO_DIRCLR_BULK = 0x03;
    static const uint8_t GPIO_BULK        = 0x04;
    static const uint8_t GPIO_BULK_SET    = 0x05;
    static const uint8_t GPIO_BULK_CLR    = 0x06;
    static const uint8_t GPIO_PULLENSET   = 0x0B;
    static const uint8_t GPIO_PULLENCLR   = 0x0C;

    // Encoder registers
    static const uint8_t ENCODER_STATUS   = 0x00;
    static const uint8_t ENCODER_INTENSET = 0x10;
    static const uint8_t ENCODER_INTENCLR = 0x20;
    static const uint8_t ENCODER_POSITION = 0x30;
    static const uint8_t ENCODER_DELTA    = 0x40;

    // Low level I²C
    bool     write(uint8_t regBase, uint8_t reg,
                   const uint8_t *buf, uint8_t len);
    bool     read(uint8_t regBase, uint8_t reg,
                  uint8_t *buf, uint8_t len, uint32_t delayUs = 125);
    bool     write8(uint8_t regBase, uint8_t reg, uint8_t val);
    uint8_t  read8(uint8_t regBase, uint8_t reg);
};

