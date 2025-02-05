#include <stdio.h>
#include "pico/stdlib.h"

#ifndef LED_DELAY_MS
#define LED_DELAY_MS 750
#endif

// Perform initialisation
int pico_led_init(void) {
    // A device like Pico that uses a GPIO for the LED will define PICO_DEFAULT_LED_PIN
    // so we can use normal GPIO functionality to turn the led on and off
    gpio_init(PICO_DEFAULT_LED_PIN);
    gpio_set_dir(PICO_DEFAULT_LED_PIN, GPIO_OUT);
    return PICO_OK;
}

void pico_set_led(bool led_on) {
    // Just set the GPIO on or off
    gpio_put(PICO_DEFAULT_LED_PIN, led_on);
}

int count = 0;

int main() {
    stdio_init_all(); // Can be kept below pico_led_init(). I/O can happen, only after this.
    int rc = pico_led_init();
    hard_assert(rc == PICO_OK);
    while (true) {
        pico_set_led(true);
        printf("LED On - %d!\n",count);
        sleep_ms(LED_DELAY_MS);
        pico_set_led(false);
        printf("LED Off - %d!\n",count);
        sleep_ms(LED_DELAY_MS);
        count = (count + 1) % 10;
    }
}
