#include "rpi.h"
#include "myi2c.h"
#include "gpio.h"
#include "bit-support.h"

void my_i2c_init(void) {
    dev_barrier();

    // Set GPIO Pins
    gpio_set_function(I2C_SDA, I2C_SDA_FUNC);
    gpio_set_function(I2C_SCL, I2C_SCL_FUNC);

    dev_barrier();

    // p.29 Write enable and 0 everwhere else to control reg
    PUT32(I2C_CTRL, 1 << 15);
    my_i2c_set_clock_div(CLOCK_DIV_RESET);
    
    // Clear Status, errors and done
    PUT32(I2C_STATUS, 0x0);
    PUT32(I2C_STATUS, (1 << 9) | (1 << 8) | (1 << 1));

    if(my_i2c_check_transfer_active()) {
        panic("Active transfer before initialization finished\n");
    }

    dev_barrier();
}

uint32_t my_i2c_check_transfer_done(void) {
    return (GET32(I2C_STATUS) >> 1) & 1;
}

void my_i2c_clear_transfer_done(void) {
    // uint32_t i2c_status = GET32(I2C_STATUS);
    // PUT32(I2C_STATUS, i2c_status | (1 << 1));
    PUT32(I2C_STATUS, 1 << 1);
}

uint32_t my_i2c_check_transfer_active(void) {
    return GET32(I2C_STATUS) & 1;
}

void my_i2c_set_transfer_type(uint32_t is_read) {
    uint32_t i2c_control = GET32(I2C_CTRL) & ~1;
    PUT32(I2C_CTRL, i2c_control | is_read);
}

uint32_t my_i2c_fifo_has_data(void) {
    return (GET32(I2C_STATUS) >> 5) & 1;
}

uint32_t my_i2c_fifo_has_space(void) {
    return (GET32(I2C_STATUS) >> 4) & 1;
}

uint32_t my_i2c_get_transfer_length(void) {
    return GET32(I2C_DLEN) & 0xFFFFFFFF;
}

void my_i2c_set_transfer_length(uint16_t len) {
    PUT32(I2C_DLEN, len);
}

uint32_t my_i2c_get_secondary_addr(void) {
    return GET32(I2C_A) & 0b1111111;
}

void my_i2c_set_secondary_addr(uint32_t addr) {
    PUT32(I2C_A, addr);
}

void my_i2c_write_fifo(uint8_t data) {
    PUT32(I2C_FIFO, data);
}

uint32_t my_i2c_read_fifo(void) {
    return GET32(I2C_FIFO) & 0xFF;
}

void my_i2c_set_clock_div(uint16_t div) {
    PUT32(I2C_DIV, div);
}

uint32_t my_i2c_get_clock_div(void) {
    return GET32(I2C_DIV) & 0xFFFF;
}

uint32_t my_i2c_get_clk_err(void) { 
    return (GET32(I2C_STATUS) >> 9) & 1;
}

uint32_t my_i2c_get_ack_err(void) {
    return (GET32(I2C_STATUS) >> 8) & 1;
}

void my_i2c_check_err(void) {
    if(my_i2c_get_ack_err()) {
        panic("Found error in I2C_ACK_ERR\n");
    } else if(my_i2c_get_clk_err()) {
        panic("Found error in I2C_CLK_ERR\n");
    }
}

// RMW control register
void my_i2c_start_transfer(void) {
    uint32_t i2c_control = GET32(I2C_CTRL);
    PUT32(I2C_CTRL, i2c_control | (1 << 7));
}

uint32_t my_i2c_read_nbytes(uint32_t nbytes, uint32_t addr, uint8_t *buf) {
    dev_barrier();

    // Wait until transfer not active
	while(my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

    if(!my_i2c_check_fifo_empty()) panic("Trying to read while FIFO not empty :(\n");
	my_i2c_check_err();

    my_i2c_set_secondary_addr(addr);

	// Set dlen (Pg. 32)
    my_i2c_set_transfer_length(nbytes);


    my_i2c_set_transfer_type(1);
    my_i2c_start_transfer();

	// Wait until transfer has started
	while(!my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

	// Read nbytes of data
	for(int i = 0; i < nbytes; i++) {

        while(!my_i2c_fifo_has_data()) {/* ¯\_(ツ)_/¯ */}
		buf[i] = my_i2c_read_fifo();
	}

	// Wait until DONE received
	while(!my_i2c_check_transfer_done()) {/* ¯\_(ツ)_/¯ */}

    if(my_i2c_check_transfer_active()) {
        panic("Read finished but transfer still active\n");
    }

	// Check for errors
    my_i2c_check_err();

	// Clear done!
    my_i2c_clear_transfer_done();

	dev_barrier();

	return 1;
}

uint32_t my_i2c_write_nbytes(uint32_t nbytes, uint32_t addr, uint8_t *buf) {
    dev_barrier();

    // Wait until transfer not active
	while(my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

    if(!my_i2c_check_fifo_empty()) panic("Trying to write while FIFO not empty :(\n");
	my_i2c_check_err();

    my_i2c_set_secondary_addr(addr);

    my_i2c_set_transfer_length(nbytes);

    my_i2c_set_transfer_type(0);
    my_i2c_start_transfer();

	// Wait until transfer has started
	while(!my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

	// Write nbytes of data
	for(int i = 0; i < nbytes; i++) {
        while(!my_i2c_fifo_has_space()) {/* ¯\_(ツ)_/¯ */}

		my_i2c_write_fifo(buf[i]);
	}

	// Wait until DONE received
	while(!my_i2c_check_transfer_done()) {/* ¯\_(ツ)_/¯ */}


    if(my_i2c_check_transfer_active()) {
        panic("Weird! Done recieved but transfer still active??\n");
    }

	// Check for errors
	my_i2c_check_err();

	// Clear done!
    my_i2c_clear_transfer_done();
	dev_barrier();

	return 1;
}

uint32_t my_i2c_check_fifo_empty() {
    return GET32(I2C_STATUS) & (1 << 6);
}