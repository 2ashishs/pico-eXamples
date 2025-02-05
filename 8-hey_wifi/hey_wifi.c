#include <stdio.h>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/vreg.h"
#include "hardware/clocks.h"

static int scan_result(void *env, const cyw43_ev_scan_result_t *result) {
    if (result && result->rssi > -50) {
        printf("ssid: %-32s rssi: %4d chan: %3d mac: %02x:%02x:%02x:%02x:%02x:%02x sec: %u\n",
            result->ssid, result->rssi, result->channel,
            result->bssid[0], result->bssid[1], result->bssid[2], result->bssid[3], result->bssid[4], result->bssid[5],
            result->auth_mode);
    }
    return 0;
}

int main() {
    // Initializations
    stdio_init_all();
    if (cyw43_arch_init()) {
        printf("failed to initialise wifi\n");
        return 1;
    }
    cyw43_arch_enable_sta_mode();
    // Code here
    absolute_time_t scan_time = nil_time;
    bool scan_in_progress = false;
    while(true) {
        if (absolute_time_diff_us(get_absolute_time(), scan_time) < 0) {
            if (!scan_in_progress) {
                cyw43_wifi_scan_options_t scan_options = {0};
                int err = cyw43_wifi_scan(&cyw43_state, &scan_options, NULL, scan_result);
                if (err == 0) {
                    printf("\nPerforming wifi scan\n");
                    scan_in_progress = true;
                    cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 1);
                } else {
                    printf("Failed to start scan: %d\n", err);
                    scan_time = make_timeout_time_ms(10000); // wait 10s and scan again
                }
            } else if (!cyw43_wifi_scan_active(&cyw43_state)) {
                printf("Will try again in a few seconds..\n");
                scan_time = make_timeout_time_ms(10000); // wait 10s and scan again
                scan_in_progress = false;
                cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, 0);
            }
        }
        // the following #ifdef is only here so this same example can be used in multiple modes;
        // you do not need it in your code
#if PICO_CYW43_ARCH_POLL
        // if you are using pico_cyw43_arch_poll, then you must poll periodically from your
        // main loop (not from a timer) to check for Wi-Fi driver or lwIP work that needs to be done.
        cyw43_arch_poll();
        // you can poll as often as you like, however if you have nothing else to do you can
        // choose to sleep until either a specified time, or cyw43_arch_poll() has work to do:
        cyw43_arch_wait_for_work_until(scan_time);
#else
        // if you are not using pico_cyw43_arch_poll, then WiFI driver and lwIP work
        // is done via interrupt in the background. This sleep is just an example of some (blocking)
        // work you might be doing.
        sleep_ms(3000);
#endif
        //tight_loop_contents();
    }
    cyw43_arch_deinit();
    return 0;
}
