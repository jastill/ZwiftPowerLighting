#pragma once

#include "config.h"
#include "hardware/pio.h"
#include "pico/stdlib.h"

class LEDController {
public:
  LEDController();
  void init();
  void fill(Color color);
  void clear();
  void startup_cycle();
  Color update_from_power(uint16_t power);
  void flash_green();

private:
  void put_pixel(uint32_t pixel_grb);
  uint32_t urgb_u32(uint8_t r, uint8_t g, uint8_t b);

  PIO pio;
  uint sm;
};
