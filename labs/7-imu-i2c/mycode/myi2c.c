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
    uint32_t i2c_status = GET32(I2C_STATUS);
    PUT32(I2C_STATUS, i2c_status | (1 << 1));
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

	// Check that FIFO is empty, no clock stretch or erros (Pg. 31-32)
	unsigned status = GET32(I2C_STATUS);
	if(!(status & (1 << 6))) panic("Trying to read while FIFO not empty :(\n");
	my_i2c_check_err();

	// Set follower device address (Pg. 33)
	PUT32(I2C_A, addr);

	// Set dlen (Pg. 32)
	PUT32(I2C_DLEN, nbytes);

	// Set to read mode and start! (!!RMW!!) (Pg. 30)
	unsigned control = GET32(I2C_CTRL);
	control = bit_set(control, 0);
	control = bit_set(control, 7);
	PUT32(I2C_CTRL, control);

	// Wait until transfer has started
	while(!my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

	// Read nbytes of data
	for(int i = 0; i < nbytes; i++) {
		unsigned status = GET32(I2C_STATUS);

		while((status & (1 << 5)) == 0) {
			status = GET32(I2C_STATUS);
		}

		buf[i] = my_i2c_read_fifo();
	}

	// Wait until DONE received
	while(!(GET32(I2C_STATUS) & (1 << 1))) {/* ¯\_(ツ)_/¯ */}

	status = GET32(I2C_STATUS);
	if(status & 1) panic("Weird! Done recieved but transfer still active??\n");

	// Check for errors
    my_i2c_check_err();

	// Clear done!
	PUT32(I2C_STATUS, 1 << 1);

	dev_barrier();

	return 1;
    // dev_barrier();
    
    // trace("Starting read...\n");
    // trace("Waiting until no transfer active..\n");
    // while(my_i2c_check_transfer_active()) { /* Spin */ }
    // trace("No transfer active\n");

    // // // assert(my_i2c_fifo_has_data());
    // // trace("Waiting until FIFO has data...\n");
    // // // while(!my_i2c_fifo_has_data()) { /* Spin */ }
    // // // trace("FIFO has data\n");
    // // if(!my_i2c_fifo_has_data()) {
    // //     panic("FIFO has no data\n");
    // // }

    // // Check that FIFO is empty (???)
    // trace("Checking FIFO empty...\n");
    // if(!my_i2c_check_fifo_empty()) {
    //     panic("Attempting to read while FIFO is not empty!\n");
    // }
    // trace("FIFO empty\n");

    // my_i2c_check_err();

    // trace("Setting transfer len %d, add %d, type 1 (read)\n", nbytes, addr);
    // my_i2c_set_transfer_length(nbytes);
    // my_i2c_set_secondary_addr(addr);
    // // my_i2c_set_transfer_type(1);
    // // trace("Starting transfer...\n");

    // // my_i2c_start_transfer();

    // // Set to read mode and start! (!!RMW!!) (Pg. 30)
	// unsigned control = GET32(I2C_CTRL);
	// control = bit_set(control, 0);
	// control = bit_set(control, 7);
	// PUT32(I2C_CTRL, control);

    // trace("Waiting until transfer active...\n");
    // while(!my_i2c_check_transfer_active()) { /* Spin */ }
    // trace("Transfer active\n");

    // for(int i = 0; i < nbytes; i++) {
    //     trace("Byte %d\n", i);
    //     trace("Waiting until FIFO has data...\n");
    //     while(!my_i2c_fifo_has_data()) { /* Spin */ }
    //     trace("FIFO has data, reading...\n");
    //     buf[i] = my_i2c_read_fifo();
    // }

    // trace("Waiting until transfer done...\n");
    // while(!my_i2c_check_transfer_done()) { /* Spin */ }
    // trace("Transfer done\n");

    // assert(!my_i2c_check_transfer_active());
    // my_i2c_check_err();

    // trace("Clearing transfer done...\n");
    // my_i2c_clear_transfer_done();
    // trace("Status reg is %x\n", GET32(I2C_STATUS));

    // dev_barrier();

    // return nbytes;
}

uint32_t my_i2c_write_nbytes(uint32_t nbytes, uint32_t addr, uint8_t *buf) {
    dev_barrier();

    // Wait until transfer not active
	while(my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

	// Check that FIFO is empty, no clock stretch or erros (Pg. 31-32)
	unsigned status = GET32(I2C_STATUS);
	if((status & (1 << 6)) == 0) panic("Trying to write while FIFO not empty :(\n");
	my_i2c_check_err();

	// Set follower device address (Pg. 33)
	PUT32(I2C_A, addr);

	// Set dlen (Pg. 32)
	PUT32(I2C_DLEN, nbytes);

	// Set to write mode and start! (!!RMW!!) (Pg. 30)
	unsigned control = GET32(I2C_CTRL);
	control = bit_clr(control, 0);
	control = bit_set(control, 7);
	PUT32(I2C_CTRL, control);

	// Wait until transfer has started
	while(!my_i2c_check_transfer_active()) {/* ¯\_(ツ)_/¯ */}

	// Write nbytes of data
	for(int i = 0; i < nbytes; i++) {
		unsigned status = GET32(I2C_STATUS);
		while((status & (1 << 4)) == 0) {
			status = GET32(I2C_STATUS);
		}

		my_i2c_write_fifo(buf[i]);
	}

	// Wait until DONE received
	while(!(GET32(I2C_STATUS) & (1 << 1))) {/* ¯\_(ツ)_/¯ */}

	status = GET32(I2C_STATUS);
	if(status & 1) panic("Weird! Done recieved but transfer still active??\n");

	// Check for errors
	my_i2c_check_err();

	// Clear done!
	PUT32(I2C_STATUS, 1 << 1);
	dev_barrier();

	return 1;
    // dev_barrier();

    // trace("Waiting until transfer not active...\n");
    // while(my_i2c_check_transfer_active()) { /* Spin */ }
    // trace("Transfer not active\n");

    // trace("Checking FIFO empty...\n");
    // if(!my_i2c_check_fifo_empty()) {
    //     panic("Attempting to write while FIFO is not empty!\n");
    // }
    // trace("FIFO empty\n");

    // trace("Checking for errors...\n");
    // my_i2c_check_err();
    // trace("No errors\n");

    // trace("Setting transfer length...\n");
    // my_i2c_set_transfer_length(nbytes);
    // trace("Setting secondary address...\n");
    // my_i2c_set_secondary_addr(addr);
    // trace("Setting transfer type...\n");
    // // my_i2c_set_transfer_type(0);
    // // trace("Setup done\n");

    // // trace("Starting transfer...\n");
    // // my_i2c_start_transfer();

    // // Set to write mode and start! (!!RMW!!) (Pg. 30)
	// unsigned control = GET32(I2C_CTRL);
	// control = bit_clr(control, 0);
	// control = bit_set(control, 7);
	// PUT32(I2C_CTRL, control);

    

    // trace("Waiting until transfer active...\n");
    // while(!my_i2c_check_transfer_active()) { /* Spin */ }
    // trace("Transfer active\n");

    // for(int i = 0; i < nbytes; i++) {
    //     // TODO: We should just wait until it has space instead
    //     trace("Byte %d waiting until FIFO has space...\n", i);
    //     while(!my_i2c_fifo_has_space()) { /* Spin */ }

    //     trace("Writing byte %d to FIFO...\n", i);
    //     my_i2c_write_fifo(buf[i]);
    // }

    // trace("Waiting until transfer done...\n");
    // while(!my_i2c_check_transfer_done()) { /* Spin */ }
    // trace("Transfer done\n");

    // assert(!my_i2c_check_transfer_active());

    // my_i2c_check_err();

    // trace("Clearing transfer done...\n");
    // my_i2c_clear_transfer_done();
    // trace("Status reg is %x\n", GET32(I2C_STATUS));

    // dev_barrier();

    // return nbytes;
}

uint32_t my_i2c_check_fifo_empty() {
    return GET32(I2C_STATUS) & (1 << 6);
}