// extensions: 
//  1. tune the different delays
//  2. make sure you handle failures and return error codes or 
//     explicitly die

#include "rpi.h"
#include "sw-i2c.h"

// Hardware-specific support functions that MUST be customized:
#define I2CSPEED 100

// can swap the two interfaces
#define USE_PULLUP
#ifdef USE_PULLUP

// put all the code that assumes the use of pullups.
// this worked for me if i used pullup
static void pin_setup(uint32_t scl, uint32_t sda) {
    gpio_set_input(scl);
    gpio_set_input(sda);
    gpio_set_pullup(scl);
    gpio_set_pullup(sda);
}
#else

// or put the code that assumes you explicitly set scl/sda

// this worked for me if i explicitly set values.
static void pin_setup(uint32_t scl, uint32_t sda) {
    gpio_set_output(scl);
    gpio_set_output(sda);
    gpio_write(scl,1);
    gpio_write(scl,1);
}

#endif

i2c_t sw_i2c_init(uint8_t addr, uint32_t scl, uint32_t sda) {
    assert(scl<32);
    assert(sda<32);
    pin_setup(scl,sda);

    return (i2c_t) { 
        .is_transmit_p = 1, 
        .SCL = scl, 
        .SDA = sda, 
        .addr = addr 
    };
}

// should let them configure.
static inline void I2C_delay() { 
    delay_us(4);
    // delay_us(100);
}

// int sw_i2c_read(i2c_t *h, uint8_t data[], unsigned nbytes) {
//     // todo("implement: make sure you send addr<<1|1!");

//     // LSB 1 bc reading
//     if(i2c_write_byte(1, 0, h->addr << 1 | 1, h))
//         panic("Error sending address: %x\n", h->addr);

//     for(int i=0; i<nbytes; i++) {
//         data[i] = i2c_read_byte(0, i == nbytes-1, h);
//     }

//     return 1;
// }


// Hardware-specific support functions that MUST be customized:
// #define I2CSPEED 100

// Return current level of SCL line, 0 or 1
bool read_SCL(i2c_t *h) {
    // if(!h->SCL_is_input_p) {
        gpio_set_input(h->SCL);
        // h->SCL_is_input_p = 1;
    // }

    return gpio_read(h->SCL);
}

// Return current level of SDA line, 0 or 1
bool read_SDA(i2c_t *h) {
    // if(!h->SDA_is_input_p) {
        gpio_set_input(h->SDA);
        // h->SDA_is_input_p = 1;
    // }
    return gpio_read(h->SDA);
}

 // Do not drive SCL (set pin high-impedance)
void set_SCL(i2c_t *h) {
    // if (!h->SCL_is_input_p) {
        gpio_set_input(h->SCL);
        // h->SCL_is_input_p = 1;
    // }
}

// Actively drive SCL signal low
void clear_SCL(i2c_t *h) {
    // if(h->SCL_is_input_p) {
        gpio_set_output(h->SCL);
        // h->SCL_is_input_p = 0;
    // }
    
    gpio_write(h->SCL,0);
}

 // Do not drive SDA (set pin high-impedance)
void set_SDA(i2c_t *h) {
    // if(!h->SDA_is_input_p) {
        gpio_set_input(h->SDA);
        // h->SDA_is_input_p = 1;
    // }
}

// Actively drive SDA signal low
void clear_SDA(i2c_t *h) {
    // if(h->SDA_is_input_p) {
        gpio_set_output(h->SDA);
        // h->SDA_is_input_p = 0;
    // }
    
    gpio_write(h->SDA, 0);
}

void arbitration_lost(void) {
    panic("Arbitration lost! Should not get here??\n");
}

bool started = false; // global data

void i2c_start_cond(i2c_t *h)
{
  if (started) { 
    // if started, do a restart condition
    // set SDA to 1
    set_SDA(h);
    I2C_delay(h);
    set_SCL(h);
    while (read_SCL(h) == 0) { // Clock stretching
      // You should add timeout to this loop
    }

    // Repeated start setup time, minimum 4.7us
    I2C_delay(h);
  }

  if (read_SDA(h) == 0) {
    arbitration_lost();
  }

  // SCL is high, set SDA from 1 to 0.
  clear_SDA(h);
  I2C_delay();
  clear_SCL(h);
  started = true;
}

void i2c_stop_cond(i2c_t *h)
{
  // set SDA to 0
  clear_SDA(h);
  I2C_delay();

  set_SCL(h);
  // Clock stretching
  while (read_SCL(h) == 0) {
    // add timeout to this loop.
  }

  // Stop bit setup time, minimum 4us
  I2C_delay();

  // SCL is high, set SDA from 0 to 1
  set_SDA(h);
  I2C_delay();

  if (read_SDA(h) == 0) {
    arbitration_lost();
  }

  started = false;
}

// Write a bit to I2C bus
void i2c_write_bit(bool bit, i2c_t *h)
{
  if (bit) {
    set_SDA(h);
  } else {
    clear_SDA(h);
  }

  // SDA change propagation delay
  I2C_delay();

  // Set SCL high to indicate a new valid SDA value is available
  set_SCL(h);

  // Wait for SDA value to be read by target, minimum of 4us for standard mode
  I2C_delay();

  while (read_SCL(h) == 0) { // Clock stretching
    // You should add timeout to this loop
  }

  // SCL is high, now data is valid
  // If SDA is high, check that nobody else is driving SDA
  if (bit && (read_SDA(h) == 0)) {
    arbitration_lost();
  }

  // Clear the SCL to low in preparation for next change
  clear_SCL(h);
}

// Read a bit from I2C bus
bool i2c_read_bit(i2c_t *h)
{
  bool bit;

  // Let the target drive data
  set_SDA(h);

  // Wait for SDA value to be written by target, minimum of 4us for standard mode
  I2C_delay();

  // Set SCL high to indicate a new valid SDA value is available
  set_SCL(h);

  while (read_SCL(h) == 0) { // Clock stretching
    // You should add timeout to this loop
  }

  // Wait for SDA value to be written by target, minimum of 4us for standard mode
  I2C_delay();

  // SCL is high, read out bit
  bit = read_SDA(h);

  // Set SCL low in preparation for next operation
  clear_SCL(h);

  return bit;
}

// Write a byte to I2C bus. Return 0 if ack by the target.
bool i2c_write_byte(
    bool send_start,
                    bool send_stop,
                    unsigned char byte,
                    i2c_t *h)
{
  unsigned bit;
  bool     nack;

  if (send_start) {
    i2c_start_cond(h);
  }

  for (bit = 0; bit < 8; ++bit) {
    i2c_write_bit((byte & 0x80) != 0, h);
    byte <<= 1;
  }

  nack = i2c_read_bit(h);

  if (send_stop) {
    i2c_stop_cond(h);
  }

  return nack;
}

// Read a byte from I2C bus
// unsigned char i2c_read_byte(bool nack, bool send_stop, i2c_t *h)
// {
//   unsigned char byte = 0;
//   unsigned char bit;

//   for (bit = 0; bit < 8; ++bit) {
//     byte = (byte << 1) | i2c_read_bit(h);
//   }

//   i2c_write_bit(nack, h);

//   if (send_stop) {
//     i2c_stop_cond(h);
//   }

//   return byte;
// }


// Read a byte from I2C bus
uint8_t i2c_read_byte(i2c_t *h, bool done_p) {
    uint8_t byte = 0;

    // msb reads
    for (unsigned bit = 0; bit < 8; ++bit)
        byte = (byte << 1) | i2c_read_bit(h);

    // if it's the last byte, <done_p>=1
    i2c_write_bit(done_p, h);
    return byte;
}


int sw_i2c_read(i2c_t *h, uint8_t data[], unsigned nbytes) {
    i2c_start_cond(h);  // xfer start

    // send address for read: low bit is 1
    if(i2c_write_byte(0, 0, h->addr<<1|1, h))
        panic("nake: failed to write byte\n");

    // write a bit after each bit: are we done?
    for(unsigned i = 0; i < nbytes; i++)
        data[i] = i2c_read_byte(h, i==(nbytes-1));

    i2c_stop_cond(h);  // xfer end
    return 1;
}

int sw_i2c_write(i2c_t *h, uint8_t data[], unsigned nbytes) {
    
    if(i2c_write_byte(1, 0, (h->addr << 1) | 0, h)) // LSB 0 bc writing
        panic("Error sending write address: %x\n", h->addr);
    
    for(int i=0; i<nbytes; i++) {
        if(i2c_write_byte(0, i == nbytes-1, data[i], h))
            panic("Error sending byte %d: %x\n", i, data[i]);
    }

    return 1;
}