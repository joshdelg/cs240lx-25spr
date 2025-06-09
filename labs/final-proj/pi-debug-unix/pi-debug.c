// engler, cs140e: driver for "bootloader" for an r/pi connected via 
// a tty-USB device.
//
// most of it is argument parsing.
//
// Unless you know what you are doing:
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//              DO NOT MODIFY THIS CODE!
//
// You shouldn't have to modify any code in this file.  Though, if you find
// a bug or improve it, let us know!
#include <ctype.h>
#include <stdarg.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>

#include "libunix.h"
#include "put-code.h"

static char *progname = 0;

static void usage(const char *msg, ...) {
    va_list args;
    va_start(args, msg);
    vfprintf(stderr, msg, args);
    va_end(args);

    output("\nusage: %s  [--trace-all] [--trace-control] ([device] | [--last] | [--first] [--device <device>]) <pi-program> \n", progname);
    output("    pi-program = has a '.bin' suffix\n");
    output("    specify a device using any method:\n");
    output("        <device>: has a '/dev' prefix\n");
    output("       --last: gets the last serial device mounted\n");
    output("        --first: gets the first serial device mounted\n");
    output("        --device <device>: manually specify <device>\n");
    output("    --baud <baud_rate>: manually specify baud_rate\n");
    output("    --trace-all: trace all put/get between rpi and unix side\n");
    output("    --trace-control: trace only control [no data] messages\n");
    exit(1);
}

int main(int argc, char *argv[]) { 
    char *dev_name = 0;
    char *pi_prog = 0;
    char *debug_prog = 0;

    // used to pass the file descriptor to another program.
    char **exec_argv = 0;

    unsigned baud_rate = B115200;

    // by default is 0x8000
    unsigned boot_addr = ARMBASE;

    // we do manual option parsing to make things a bit more obvious.
    // you might rewrite using getopt().
    progname = argv[0];
    // Expect pi-debug test.bin to load the debug-harness.bin as the primary program
    // then load test.bin as the debugging target
    pi_prog = "debug-harness.bin";
    // pi_prog = "hello.bin";

    if (argc < 2) {
        panic("No debug program provided\n");
    }

    if (!suffix_cmp(argv[1], ".bin")) {
        panic("Debug program must be a .bin file %s\n", argv[1]);
    }

    debug_prog = argv[1];

    debug_output("Running pi-debug for target %s\n", debug_prog);

    // 1. get the name of the ttyUSB.
    if(!dev_name) {
        dev_name = find_ttyusb_last();
        if(!dev_name)
            panic("didn't find a device\n");
    }

    // 2. open the ttyUSB in 115200, 8n1 mode
    int tty = open_tty(dev_name);
    if(tty < 0)
        panic("can't open tty <%s>\n", dev_name);

    // timeout is in tenths of a second.  tuning this can speed up
    // checking.
    //
    // if you are on linux you can shrink down the <2*8> timeout
    // threshold.  if your my-install isn't reseting when used 
    // during checkig, it's likely due to this timeout being too
    // small.
    double timeout_tenths = 2*5;
    int fd = set_tty_to_8n1(tty, baud_rate, timeout_tenths);
    if(fd < 0)
        panic("could not set tty: <%s>\n", dev_name);

    // 3. read in program [probably should just make a <file_size>
    //    call and then shard out the pieces].
	unsigned nbytes;
    uint8_t *code = read_file(&nbytes, pi_prog);

    // @joshdelg Now, read in the debug target
    unsigned debug_nbytes;
    uint8_t *debug_code = read_file(&debug_nbytes, debug_prog);

    // 4. let's send it!
	debug_output("%s: tty-usb=<%s> program=<%s>: about to boot\n", 
                progname, dev_name, pi_prog);
    simple_boot(fd, boot_addr, code, nbytes, debug_code, debug_nbytes);

    // 5. echo output from pi
    if(!exec_argv)
        pi_echo(0, fd, dev_name);

	return 0;
}
