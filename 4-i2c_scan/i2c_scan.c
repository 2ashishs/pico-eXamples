// Sweep through all 7-bit I2C addresses, to see if any slaves are present on
// the I2C bus. Print out a table that looks like this:
//
// I2C Bus Scan
//    0 1 2 3 4 5 6 7 8 9 A B C D E F
// 00 . . . . . . . . . . . . . . . .
// 10 . . @ . . . . . . . . . . . . .
// 20 . . . . . . . . . . . . . . . .
// 30 . . . . @ . . . . . . . . . . .
// 40 . . . . . . . . . . . . . . . .
// 50 . . . . . . . . . . . . . . . .
// 60 . . . . . . . . . . . . . . . .
// 70 . . . . . . . . . . . . . . . .
// E.g. if addresses 0x12 and 0x34 were acknowledged.

#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define I2C_BAUDRATE 100000 //100kHz

// I2C reserves some addresses for special purposes. We exclude these from the scan.
// These are any addresses of the form 000 0xxx or 111 1xxx
bool reserved_addr(uint8_t addr) {
    return (addr & 0x78) == 0 || (addr & 0x78) == 0x78;
}

int main() {
    stdio_init_all();
    printf("i2c init");
    gpio_init(I2C_SDA_PIN);
    gpio_set_function(I2C_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SDA_PIN);
    gpio_init(I2C_SDA_PIN);
    gpio_set_function(I2C_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_SCL_PIN);
    i2c_init(i2c0, I2C_BAUDRATE);
    while(true) {
        printf("i2c0 initialized.\n");

		printf("\nI2C Bus Scan\n");
		printf("   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F\n");

		for (int addr = 0; addr < (1 << 7); ++addr) {
			if (addr % 16 == 0) {
				printf("%02x ", addr);
			}

			// Perform a 1-byte dummy read from the probe address. If a slave
			// acknowledges this address, the function returns the number of bytes
			// transferred. If the address byte is ignored, the function returns
			// -1.

			// Skip over any reserved addresses.
			int ret;
			uint8_t rxdata[2];
			rxdata[0] = 1;
			rxdata[1] = 2;
			if (reserved_addr(addr)) {
				printf("R-%02x-u", addr);
			} else {
				printf("A-%02x", addr);
				ret = i2c_read_timeout_us(i2c0, addr, rxdata, 1, false, 100000);
				printf(ret < 1 ? "-u":"-s");
			}

			printf(addr % 16 == 15 ? "\n" : "  ");
			sleep_ms(250);
		}

		
        sleep_ms(500);
    }
    return 0;
}
