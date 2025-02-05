/**
 * Copyright (c) 2020 Raspberry Pi (Trading) Ltd.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "hardware/uart.h"

/// \tag::hello_uart[]

#define UART_ID uart0
#define BAUD_RATE 115200

// We are using pins 0 and 1, but see the GPIO function select table in the
// datasheet for information on which other pins can be used.
#define UART_TX_PIN 0
#define UART_RX_PIN 1


char* get_agent_code(const char* agent, int agent_id) {
    // Calculate the maximum size needed
    // Include space for the agent, integer (max 12 chars), and null terminator
    size_t buffer_size = strlen(agent) + 12;

    // Allocate memory for the result
    char* result = (char*)malloc(buffer_size);
    if (result == NULL) {
        fprintf(stderr, "Memory allocation failed.\n");
        return NULL; // Handle memory allocation failure
    }

    // Combine agent and agent_id into the result buffer
    snprintf(result, buffer_size, "%s%d", agent, agent_id);

    return result; // Caller is responsible for freeing the memory
}


int count = 0;

int main() {
    // Set up our UART with the required speed.
    uart_init(UART_ID, BAUD_RATE);

    // Set the TX and RX pins by using the function select on the GPIO
    // Set datasheet for more information on function select
    gpio_set_function(UART_TX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_TX_PIN));
    gpio_set_function(UART_RX_PIN, UART_FUNCSEL_NUM(UART_ID, UART_RX_PIN));

    // Use some the various UART functions to send out data
    // In a default system, printf will also output via the default UART
    while (true) {
        // Send out a character without any conversions
        uart_puts(UART_ID, "Pico2");

        // Send out a character but do CR/LF conversions
        uart_putc(UART_ID, ':');

        // Send out a string, with CR/LF conversions
        uart_puts(UART_ID, " Hello, UART !! I am ");
        uart_puts(UART_ID, get_agent_code("Agent-", count));
        uart_putc_raw(UART_ID, '\n');
        count = (count + 1) % 10;
        sleep_ms(1000);
    }
    return 0;
}

/// \end::hello_uart[]
