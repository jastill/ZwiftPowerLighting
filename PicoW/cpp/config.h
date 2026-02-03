#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Hardware Configuration
constexpr uint LED_PIN = 28;
constexpr uint NUM_LEDS = 30;

// Display Pins (Pimoroni Pico Display)
constexpr uint DISPLAY_WIDTH = 240;
constexpr uint DISPLAY_HEIGHT = 135;
constexpr uint SPI_PORT = 0; // spi0
constexpr uint PIN_MISO = 16;
constexpr uint PIN_CS = 17;
constexpr uint PIN_SCK = 18;
constexpr uint PIN_MOSI = 19;
constexpr uint PIN_DC =
    16; // Note: Schematics vary, usually 16 or 20 for DC/BL on some packs.
        // Pimoroni Pico Display: CS=17, SCK=18, MOSI=19, DC=16, BL=20
constexpr uint PIN_BL = 20;

constexpr uint PIN_BUTTON_A = 12;
constexpr uint PIN_BUTTON_B = 13;
constexpr uint PIN_BUTTON_X = 14;
constexpr uint PIN_BUTTON_Y = 15;

constexpr uint PIN_LED_R = 6;
constexpr uint PIN_LED_G = 7;
constexpr uint PIN_LED_B = 8;

// Bluetooth Configuration
const std::string BLE_TARGET_NAME = "KICKR CORE 5D21";

// Rider Configuration
constexpr uint16_t DEFAULT_FTP = 227;

struct Color {
  uint8_t r, g, b;
};

struct PowerZone {
  uint16_t min_percent;
  uint16_t max_percent;
  Color color;
};

// Start bytes for color logic
const std::vector<PowerZone> POWER_ZONES = {
    {0, 60, {255, 255, 255}},  // Zone 1: White
    {60, 76, {0, 0, 255}},     // Zone 2: Blue
    {76, 90, {0, 255, 0}},     // Zone 3: Green
    {90, 105, {255, 255, 0}},  // Zone 4: Yellow
    {105, 119, {255, 165, 0}}, // Zone 5: Orange
    {119, 999, {255, 0, 0}}    // Zone 6: Red
};
