#pragma once

#include "config.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include <deque>
#include <string>
#include <vector>

class Display {
public:
  Display();
  void init();
  void clear(Color color = {0, 0, 0});
  void text(const char *msg, uint16_t x, uint16_t y,
            Color color = {255, 255, 255}, uint8_t scale = 1);
  void update_status(bool connected, uint16_t power, Color zone_color,
                     bool show_ftp, uint16_t ftp);
  void add_log_line(const char *msg);
  void draw_logs();

  // Basic drawing primitives
  void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, Color color);
  void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                 Color color);

private:
  void write_command(uint8_t cmd);
  void write_data(uint8_t data);
  void write_data(const uint8_t *data, size_t len);

  void set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h);
  uint16_t color565(Color color);

  void draw_char(char c, uint16_t x, uint16_t y, Color color, uint8_t scale);

  std::vector<std::string> log_lines;
  const size_t MAX_LOG_LINES = 5;
  std::deque<uint16_t> power_history;
};
