#ifndef JOSH_I2C_H
#define JOSH_I2C_H

enum {
	I2C_SDA = 2,
	I2C_SCL = 3,
	I2C_SDA_FUNC = GPIO_FUNC_ALT0,
	I2C_SCL_FUNC = GPIO_FUNC_ALT0,
};

enum {
    I2C_BASE = 0x20804000,
    I2C_CTRL = I2C_BASE + 0x0,
    I2C_STATUS = I2C_BASE + 0x4,
    I2C_DLEN = I2C_BASE + 0x8,
    I2C_A = I2C_BASE + 0xC,
    I2C_FIFO = I2C_BASE + 0x10,
    I2C_DIV = I2C_BASE + 0x14,
    I2C_DEL = I2C_BASE + 0x18,
    I2C_CLKT = I2C_BASE + 0x1C
};

enum {
    CLOCK_DIV_RESET = 0x5dc
};

void my_i2c_init(void);

uint32_t my_i2c_check_transfer_done(void);

void my_i2c_clear_transfer_done(void);

uint32_t my_i2c_check_transfer_active(void);

void my_i2c_set_transfer_type(uint32_t is_read);;

uint32_t my_i2c_fifo_has_data(void);

uint32_t my_i2c_fifo_has_space(void);

uint32_t my_i2c_get_transfer_length(void);

void my_i2c_set_transfer_length(uint16_t);

uint32_t my_i2c_get_secondary_addr(void);

void my_i2c_set_secondary_addr(uint32_t addr);

void my_i2c_write_fifo(uint8_t data);

uint32_t my_i2c_read_fifo(void);

void my_i2c_set_clock_div(uint16_t div);

uint32_t my_i2c_get_clock_div(void);

uint32_t my_i2c_get_clk_err(void);

uint32_t my_i2c_get_ack_err(void);

// Side Effect: Panics if error
void my_i2c_check_err(void);

void my_i2c_start_transfer(void);

// Reads <nbytes> from 
uint32_t my_i2c_read_nbytes(uint32_t nbytes, uint32_t addr, uint8_t *buf);

uint32_t my_i2c_write_nbytes(uint32_t nbytes, uint32_t addr, uint8_t *buf);

uint32_t my_i2c_check_fifo_empty(void);

#endif