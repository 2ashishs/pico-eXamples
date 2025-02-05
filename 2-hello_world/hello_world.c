#include <stdio.h>
#include "pico/stdlib.h"

int count = 0;
int main() {
    stdio_init_all();
    while (true) {
        count = (count + 1) % 10;
        printf("Hello Agent 00%d!\n",count);
        sleep_ms(1000);
    }
}
