#include <hardware/i2c.h>
#include <pico/i2c_slave.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

static const uint I2C_SLAVE_ADDRESS = 0x17;
static const uint I2C_BAUDRATE = 100000; // 100 kHz

// For this example, we run both the master from pin GP18 (SDA) and pin GP19 (SCL).
static const uint I2C_MASTER_SDA_PIN = 18;
static const uint I2C_MASTER_SCL_PIN = 19;

static void setup_master() {
    gpio_init(I2C_MASTER_SDA_PIN);
    gpio_set_function(I2C_MASTER_SDA_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MASTER_SDA_PIN);

    gpio_init(I2C_MASTER_SCL_PIN);
    gpio_set_function(I2C_MASTER_SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(I2C_MASTER_SCL_PIN);

    i2c_init(i2c1, I2C_BAUDRATE);
}

static void run_master() {
    setup_master();

    for (uint8_t mem_address = 0;; mem_address = (mem_address + 32) % 256) {
        char msg[32];
        snprintf(msg, sizeof(msg), "Hello, I2C slave! - 0x%02X", mem_address);
        uint8_t msg_len = strlen(msg);

        printf("Message=%s, Len=%d \n",msg, msg_len);

        uint8_t buf[32];
        buf[0] = mem_address;
        memcpy(buf + 1, msg, msg_len);

        // write message at mem_address
        printf("Write at 0x%02X: '%s'\n", mem_address, msg);
        int count = i2c_write_blocking(i2c1, I2C_SLAVE_ADDRESS, buf, 1 + msg_len, false);
        if (count < 0) {
            puts("Couldn't write to slave, please check your wiring!");
            return;
        }
        hard_assert(count == 1 + msg_len);

        // seek to mem_address
        count = i2c_write_blocking(i2c1, I2C_SLAVE_ADDRESS, buf, 1, true);
        hard_assert(count == 1);

        // partial read
        uint8_t split = 5;
        count = i2c_read_blocking(i2c1, I2C_SLAVE_ADDRESS, buf, split, true);
        hard_assert(count == split);
        buf[count] = '\0'; //null terminator
        printf("Read  at 0x%02X: '%s'\n", mem_address, buf);
        hard_assert(memcmp(buf, msg, split) == 0);

        // read the remaining bytes, continuing from last address
        count = i2c_read_blocking(i2c1, I2C_SLAVE_ADDRESS, buf, msg_len - split, false);
        hard_assert(count == msg_len - split);
        buf[count] = '\0'; //null terminator
        printf("Read  at 0x%02X: '%s'\n", mem_address + split, buf);
        hard_assert(memcmp(buf, msg + split, msg_len - split) == 0);

        printf("\n");
        sleep_ms(2000);
    }
}

int main() {
    stdio_init_all();
    //Code here
    printf("\nI2C master :");
    run_master();
}
