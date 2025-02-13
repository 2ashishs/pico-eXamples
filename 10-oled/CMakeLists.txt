cmake_minimum_required(VERSION 3.13...3.27)

# Pull in SDK (must be before project)
include(pico_sdk_import.cmake)
# Pull in SDK Extras (optional)
include(pico_extras_import.cmake)
# Pull in FreeRTOS (optional)
#include(FreeRTOS_Kernel_import.cmake)

project(pico_play C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# If you want debug output from USB (pass -DPICO_STDIO_USB=1) this ensures you don't lose any debug output while USB is set up
if (NOT DEFINED PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS)
    set(PICO_STDIO_USB_CONNECT_WAIT_TIMEOUT_MS 3000)
endif()

# initialize the Raspberry Pi Pico SDK
pico_sdk_init()

add_compile_options(
		-Wall
        -Wno-format          # int != int32_t as far as the compiler is concerned because gcc has int32_t as long int
        -Wno-unused-function # we have some for the docs that aren't called
        )
if (CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-Wno-maybe-uninitialized)
endif()

add_executable(10-oled 10-oled.c)

# pull in common dependencies
target_link_libraries(10-oled pico_stdlib)

# enable/disable usb/uart
pico_enable_stdio_uart(10-oled 0)
pico_enable_stdio_usb(10-oled 1)

# create map/bin/hex/uf2 file etc.
pico_add_extra_outputs(10-oled)

