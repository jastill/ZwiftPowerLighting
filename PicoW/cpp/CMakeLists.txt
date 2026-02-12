# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico CACHE STRING "Board type")

cmake_minimum_required(VERSION 3.13)

# Parse .env if it exists
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.env" ConfigContents)
    foreach(NameVar ${ConfigContents})
        if("${NameVar}" MATCHES "^([^=]+)=(.*)$")
            set(ENV_VAR_NAME "${CMAKE_MATCH_1}")
            set(ENV_VAR_VALUE "${CMAKE_MATCH_2}")
            # Strip quotes if present (optional but good safety)
            string(REPLACE "\"" "" ENV_VAR_VALUE "${ENV_VAR_VALUE}")
            add_compile_definitions(${ENV_VAR_NAME}="${ENV_VAR_VALUE}")
            message(STATUS "Loaded from .env: ${ENV_VAR_NAME}=${ENV_VAR_VALUE}")
        endif()
    endforeach()
else()
    message(WARNING ".env file not found! WiFi credentials will be missing.")
endif()

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
    hue_client.cpp
)

# Generate PIO header
pico_generate_pio_header(ZwiftPowerLighting ${CMAKE_CURRENT_LIST_DIR}/ws2812.pio)

# Enable BLE (BTstack)
target_include_directories(ZwiftPowerLighting PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# Link libraries
target_link_libraries(ZwiftPowerLighting
    pico_stdlib
    pico_cyw43_arch_lwip_threadsafe_background
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
