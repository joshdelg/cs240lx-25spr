// do a raw dump of all the values for a given keypress. 
//
// this is useful for seeing how your remote behaves.
#include "rpi.h"
#include "pi-random.h"

// simple-minded log of timed reads (value, usec)
enum { TR_MAX = 255 };
typedef struct timed_reads {
    unsigned n;
    struct timed_read { 
        uint32_t usec;
        uint32_t v;
    } r[TR_MAX];
} tr_t;

enum {
    ONE = 16753245,
    TWO = 16736925,
    THREE = 16769565,
    FOUR = 16720605,
    FIVE = 16712445,
    SIX = 16761405,
    SEVEN = 16769055,
    EIGHT = 16754775,
    NINE = 16748655,
    STAR = 16738455,
    ZERO = 16750695,
    HASH = 16756815,
    UP = 16718055,
    LEFT = 16716015,
    OK = 16726215,
    RIGHT = 16734885,
    DOWN = 16730805
};

enum { 
    input = 21,         // input pin: "S" on IR
    N = 10,             // total readings
    timeout = 40000,    // timeout in usec
    header_0 = 9000,
    header_1 = 4500,
    zerobit_0 = 600,
    zerobit_1 = 600,
    onebit_0 = 600,
    onebit_1 = 1600,
    stop_0 = 0,
    stop_1 = 20000
 };

static inline tr_t tr_mk(void) {
    return (tr_t){};
}
// return timed read element at <i>: null if none.
struct timed_read *tr_elem(tr_t *t, unsigned i) {
    if(i >= t->n)
        return 0;
    return &t->r[i];
}

// LIFO add entry to timed read log <l>
static void tr_push(tr_t *l, uint32_t v, uint32_t usec) {
    if(l->n >= TR_MAX)
        panic("too many entries!\n");
    let e = &l->r[l->n++];
    e->usec = usec;
    e->v = v;
}

// loop while(gpio_read(pin) == v) until either:
//   1. the pin changes (return the number of usec passed)
//   2. <timeout> is exceeded (return 0).
static uint32_t read_while_eq(int pin, int v, unsigned timeout) {
    unsigned start = timer_get_usec_raw();
    while(1) {
        // we add +1 to make sure always return != 0
        if(gpio_read(pin) != v)
            return timer_get_usec_raw() - start + 1;
        // if timeout, return 0.
        if((timer_get_usec_raw() - start) >= timeout)
            return 0;
    }
}

static uint32_t within_target(uint32_t value, uint32_t target, float margin) {
    return target * (1.0 - margin) <= value && value <= target * (1.0 + margin);
}

void log_key_press(uint32_t observed) {
    char *buf;
    char buf2[32];
    snprintk(buf2, 32, "UNKNOWN: %d", observed);
    buf = buf2;

    switch (observed) {
        case ONE:
            buf = "KEY 1";
            break;
        case TWO:
            buf = "KEY 2";
            break;
        case THREE:
            buf = "KEY 3";
            break;
        case FOUR:
            buf = "KEY 4";
            break;
        case FIVE:
            buf = "KEY 5";
            break;
        case SIX:
            buf = "KEY 6";
            break;
        case SEVEN:
            buf = "KEY 7";
            break;
        case EIGHT:
            buf = "KEY 8";
            break;
        case NINE:
            buf = "KEY 9";
            break;
        case STAR:
            buf = "KEY *";
            break;
        case ZERO:
            buf = "KEY 0";
            break;
        case HASH:
            buf = "KEY #";
            break;
        case UP:
            buf = "KEY UP";
            break;
        case LEFT:
            buf = "KEY LEFT";
            break;
        case OK:
            buf = "KEY OK";
            break;
        case RIGHT:
            buf = "KEY RIGHT";
            break;
        case DOWN:
            buf = "KEY DOWN";
            break;
    }
    output("key press: %s\n", buf);
}

enum {
    GREEN = 5,
    ORANGE = 6,
    BLUE = 13,
    RED = 18
};

enum {
    GREEN_KEY = UP,
    ORANGE_KEY = RIGHT,
    BLUE_KEY = DOWN,
    RED_KEY = LEFT
};

uint32_t get_next_light() {
    uint32_t rand = pi_random();

    switch (rand % 4) {
        case 0: return GREEN;
        case 1: return ORANGE;
        case 2: return BLUE;
        case 3: return RED;
        default: return GREEN;
    }
}

uint32_t light_to_key(uint32_t light) {
    switch (light) {
        case GREEN: return GREEN_KEY;
        case ORANGE: return ORANGE_KEY;
        case BLUE: return BLUE_KEY;
        case RED: return RED_KEY;
        default: return GREEN_KEY;
    }
}

uint32_t capture_key_press() {
    uint32_t v, t, idx;

    output("Waiting for key press...\n");
    tr_t l = tr_mk();

    // again: default is 1, so nothing is happening.
    while(gpio_read(input) == 1)
        ;
    v = 0;

    // read values until timeout
    for(idx = 0;  idx < 255; idx++) {
        // read until gpio_read(input) != v or timeout
        if(!(t = read_while_eq(input, v, timeout))) {
            tr_push(&l, v, timeout);
            break;
        }
        tr_push(&l, v, t);
        // flip so get the next value
        v = 1 - v;
    }
    // print them out, two at a time so it's easy to see 
    // whats going on.
    for(unsigned i = 0; i < idx; i += 2) {
        let e = tr_elem(&l, i);
        assert(e);

        output("%d: v=%d: usec=%d ", i, e->v, e->usec);
        // in case we don't have enough readings.
        e = tr_elem(&l, i+1);
        if(e)
            output("v=%d, usec=%d", e->v, e->usec);
        output("\n");
    }

    // Shift observed reads into an integer
    uint32_t observed = 0;
    uint32_t assembling_code = 0;

    for(int i = 0; i < l.n; i += 1) {
        if(i + 1 >= l.n) break;

        let e1 = tr_elem(&l, i);
        let e2 = tr_elem(&l, i+1);

        if(assembling_code) {
            if(e1->v == 0 && within_target(e1->usec, zerobit_0, 0.1)) {
                // Decide 1 or 0
                uint32_t midpoint = (zerobit_1 + onebit_1) / 2;

                if(e2->v == 1) {
                    if(e2->usec > stop_1) {
                        assembling_code = 0;
                        return observed;
                        observed = 0;
                    } else {
                        observed = observed << 1 | ((e2->usec < midpoint) ? 0 : 1);
                    }

                    i++;
                }
            }
        } else if(!assembling_code && e1->v == 0 && within_target(e1->usec, header_0, 0.1)) {
            if(e2->v == 1 && within_target(e2->usec, header_1, 0.1)) {
                assembling_code = 1;
                i += 1;
            }
        }
    }

    return -1;
}

void simon_says(void) {
    // Init lights
    gpio_set_function(RED, GPIO_FUNC_OUTPUT);
    gpio_set_function(GREEN, GPIO_FUNC_OUTPUT);
    gpio_set_function(ORANGE, GPIO_FUNC_OUTPUT);
    gpio_set_function(BLUE, GPIO_FUNC_OUTPUT);
 
    // gpio_write(RED, 1);
    // gpio_write(GREEN, 1);
    // gpio_write(ORANGE, 1);
    // gpio_write(BLUE, 1);

    uint32_t light_history[8];
    uint32_t max_lights = 0;

    light_history[max_lights++] = get_next_light();

    while(max_lights <= 8) {
        // Flash current light history
        for(int i = 0; i < max_lights; i++) {
            // Flash light
            gpio_write(light_history[i], 1);

            // Sleep
            delay_ms(1000);

            // Turn off light
            gpio_write(light_history[i], 0);
        }

        // Sleep for 2 seconds
        delay_ms(2000);

        // User Inputs sequence
        for(int i = 0; i < max_lights; i++) {
            uint32_t observed = capture_key_press();
            log_key_press(observed);
            
            // We got a key press
            if(observed != light_to_key(light_history[i])) {
                output("WRONG\n");
                return;
            }
        }
        
        // Add a new light
        light_history[max_lights++] = get_next_light();
    }

    output("YOU WON!!!\n");
}

void notmain(void) {
    gpio_set_input(input);
    // IR goes to 0 when there is signal.
    // We use a pullup to make sure no signal = 1
    // for sure.
    gpio_set_pullup(input);     

    // if this fails, your hardware isn't hooked up right
    assert(gpio_read(input) == 1);

    simon_says();
    return;
    
    output("will try to do a raw dump of %d readings\n", N);
    for(int i = 0; i < N; i++) {
        uint32_t v, t, idx;

        output("trial %d: about to read\n", i);
        tr_t l = tr_mk();

        // again: default is 1, so nothing is happening.
        while(gpio_read(input) == 1)
            ;
        v = 0;
    
        // read values until timeout
        for(idx = 0;  idx < 255; idx++) {
            // read until gpio_read(input) != v or timeout
            if(!(t = read_while_eq(input, v, timeout))) {
                tr_push(&l, v, timeout);
                break;
            }
            tr_push(&l, v, t);
            // flip so get the next value
            v = 1 - v;
        }
        // print them out, two at a time so it's easy to see 
        // whats going on.
        for(unsigned i = 0; i < idx; i += 2) {
            let e = tr_elem(&l, i);
            assert(e);

            output("%d: v=%d: usec=%d ", i, e->v, e->usec);
            // in case we don't have enough readings.
            e = tr_elem(&l, i+1);
            if(e)
                output("v=%d, usec=%d", e->v, e->usec);
            output("\n");
        }

        // Shift observed reads into an integer
        uint32_t observed = 0;
        uint32_t assembling_code = 0;

        for(int i = 0; i < l.n; i += 1) {
            if(i + 1 >= l.n) break;

            let e1 = tr_elem(&l, i);
            let e2 = tr_elem(&l, i+1);

            if(assembling_code) {
                if(e1->v == 0 && within_target(e1->usec, zerobit_0, 0.1)) {
                    // Decide 1 or 0
                    uint32_t midpoint = (zerobit_1 + onebit_1) / 2;

                    if(e2->v == 1) {
                        if(e2->usec > stop_1) {
                            assembling_code = 0;
                            log_key_press(observed);
                            observed = 0;
                        } else {
                            observed = observed << 1 | ((e2->usec < midpoint) ? 0 : 1);
                        }

                        i++;
                    }
                }
            } else if(!assembling_code && e1->v == 0 && within_target(e1->usec, header_0, 0.1)) {
                if(e2->v == 1 && within_target(e2->usec, header_1, 0.1)) {
                    assembling_code = 1;
                    i += 1;
                }
            }
        }
    }
}
