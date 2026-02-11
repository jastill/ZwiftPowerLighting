#include "leds.hpp"
#include "ws2812.pio.h"
#include <cstdio>

LEDController::LEDController() {
  pio = pio0;
  sm = 0;
}

void LEDController::init() {
  uint offset = pio_add_program(pio, &ws2812_program);
  ws2812_program_init(pio, sm, offset, LED_PIN, 800000, false);
  clear();
}

uint32_t LEDController::urgb_u32(uint8_t r, uint8_t g, uint8_t b) {
  return ((uint32_t)(r) << 8) | ((uint32_t)(g) << 16) | (uint32_t)(b);
}

void LEDController::put_pixel(uint32_t pixel_grb) {
  pio_sm_put_blocking(pio, sm, pixel_grb << 8u);
}

void LEDController::fill(Color color) {
  uint32_t pixel = urgb_u32(color.r, color.g, color.b);
  for (int i = 0; i < NUM_LEDS; ++i) {
    put_pixel(pixel);
  }
  sleep_us(50); // Reset time
}

void LEDController::clear() { fill({0, 0, 0}); }

void LEDController::startup_cycle() {
  printf("Starting LED cycle...\n");

  // Progressively light each LED with its zone color
  for (size_t step = 0; step < POWER_ZONES.size() && step < NUM_LEDS; ++step) {
    for (size_t i = 0; i <= step; ++i) {
      put_pixel(urgb_u32(POWER_ZONES[i].color.r, POWER_ZONES[i].color.g,
                          POWER_ZONES[i].color.b));
    }
    // Fill remaining LEDs with off
    for (size_t i = step + 1; i < NUM_LEDS; ++i) {
      put_pixel(0);
    }
    sleep_us(50);
    sleep_ms(300);
  }

  // Hold all zone colors for a moment
  sleep_ms(500);
  clear();
}

void LEDController::flash_green() {
  for (int i = 0; i < 6; i++) { // 3 seconds, 500ms interval = 6 toggles
    if (i % 2 == 0)
      fill({0, 255, 0});
    else
      clear();
    sleep_ms(500);
  }
  clear();
}

Color LEDController::update_from_power(uint16_t power, uint16_t ftp) {
  float percentage = ((float)power / (float)ftp) * 100.0f;
  Color target_color = {0, 0, 0};

  for (const auto &zone : POWER_ZONES) {
    if (percentage >= zone.min_percent && percentage < zone.max_percent) {
      target_color = zone.color;
      break;
    }
  }

  // Check for last zone (handle > max)
  const auto &last_zone = POWER_ZONES.back();
  if (percentage >= last_zone.min_percent) {
    target_color = last_zone.color;
  }

  fill(target_color);
  return target_color;
}
