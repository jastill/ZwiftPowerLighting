cmake_minimum_required(VERSION 3.13)

# Initialize Pico SDK
include(pico_sdk_import.cmake)

project(ZwiftPowerLighting C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the SDK
pico_sdk_init()

add_executable(ZwiftPowerLighting
    main.cpp
    leds.cpp
    display.cpp
    ble_client.cpp
)

# Generate PIO header
pico_generate_pio_header(ZwiftPowerLighting ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Enable BLE (BTstack)
target_include_directories(ZwiftPowerLighting PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Link libraries
target_link_libraries(ZwiftPowerLighting
    pico_stdlib
    pico_cyw43_arch_none # We handle cyw43 for BLE but using BTstack helpers usually
    pico_btstack_ble
    pico_btstack_cyw43
    hardware_pio
    hardware_spi
    hardware_dma
    hardware_pwm
)

# Enable USB output (for printf)
pico_enable_stdio_usb(ZwiftPowerLighting 1)
pico_enable_stdio_uart(ZwiftPowerLighting 0)

# Create map file
pico_add_extra_outputs(ZwiftPowerLighting)
