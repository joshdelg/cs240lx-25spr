#ifndef __SW_UART_H__
#define __SW_UART_H__

#include <stdbool.h>

typedef struct i2c {
    // is this a transmitter or receiver?
    unsigned is_transmit_p;
    bool started_p;

    uint8_t addr;
    uint8_t SCL;
    uint8_t SDA;

    // these can switch back and forth. 
    unsigned SCL_is_input_p:1;
    unsigned SDA_is_input_p:1;
} i2c_t;

i2c_t sw_i2c_init(uint8_t addr, uint32_t scl, uint32_t sda);
int sw_i2c_write(i2c_t *h, uint8_t data[], unsigned nbytes);
int sw_i2c_read(i2c_t *h, uint8_t data[], unsigned nbytes);

// void I2C_delay(void);
// bool read_SCL(i2c_t *h); // Return current level of SCL line, 0 or 1
// bool read_SDA(i2c_t *h); // Return current level of SDA line, 0 or 1
// void set_SCL(i2c_t *h); // Do not drive SCL (set pin high-impedance)
// void clear_SCL(i2c_t *h); // Actively drive SCL signal low
// void set_SDA(i2c_t *h); // Do not drive SDA (set pin high-impedance)
// void clear_SDA(i2c_t *h); // Actively drive SDA signal low
// void arbitration_lost(void);
// bool i2c_write_byte(bool send_start, bool send_stop, unsigned char byte, i2c_t *h);
// unsigned char i2c_read_byte(bool nack, bool send_stop, i2c_t *h);
// unsigned char i2c_read_byte(i2c_t *h, bool done_p);

#endif
