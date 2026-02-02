#include "display.hpp"
#include <cstdio>
#include <cstring>

// Simple 5x7 font data (ISO 8859-1 subset)
static const uint8_t font[] = {
    0x00, 0x00, 0x00, 0x00, 0x00, // SPACE
    0x00, 0x00, 0x5F, 0x00, 0x00, // !
    0x00, 0x07, 0x00, 0x07, 0x00, // "
    0x14, 0x7F, 0x14, 0x7F, 0x14, // #
    0x24, 0x2A, 0x7F, 0x2A, 0x12, // $
    0x23, 0x13, 0x08, 0x64, 0x62, // %
    0x36, 0x49, 0x55, 0x22, 0x50, // &
    0x00, 0x05, 0x03, 0x00, 0x00, // '
    0x00, 0x1C, 0x22, 0x41, 0x00, // (
    0x00, 0x41, 0x22, 0x1C, 0x00, // )
    0x14, 0x08, 0x3E, 0x08, 0x14, // *
    0x08, 0x08, 0x3E, 0x08, 0x08, // +
    0x00, 0x50, 0x30, 0x00, 0x00, // ,
    0x08, 0x08, 0x08, 0x08, 0x08, // -
    0x00, 0x60, 0x60, 0x00, 0x00, // .
    0x20, 0x10, 0x08, 0x04, 0x02, // /
    0x3E, 0x51, 0x49, 0x45, 0x3E, // 0
    0x00, 0x42, 0x7F, 0x40, 0x00, // 1
    0x42, 0x61, 0x51, 0x49, 0x46, // 2
    0x21, 0x41, 0x45, 0x4B, 0x31, // 3
    0x18, 0x14, 0x12, 0x7F, 0x10, // 4
    0x27, 0x45, 0x45, 0x45, 0x39, // 5
    0x3C, 0x4A, 0x49, 0x49, 0x30, // 6
    0x01, 0x71, 0x09, 0x05, 0x03, // 7
    0x36, 0x49, 0x49, 0x49, 0x36, // 8
    0x06, 0x49, 0x49, 0x29, 0x1E, // 9
    0x00, 0x36, 0x36, 0x00, 0x00, // :
    0x00, 0x56, 0x36, 0x00, 0x00, // ;
    0x08, 0x14, 0x22, 0x41, 0x00, // <
    0x14, 0x14, 0x14, 0x14, 0x14, // =
    0x00, 0x41, 0x22, 0x14, 0x08, // >
    0x02, 0x01, 0x51, 0x09, 0x06, // ?
    0x32, 0x49, 0x79, 0x41, 0x3E, // @
    0x7E, 0x11, 0x11, 0x11, 0x7E, // A
    0x7F, 0x49, 0x49, 0x49, 0x36, // B
    0x3E, 0x41, 0x41, 0x41, 0x22, // C
    0x7F, 0x41, 0x41, 0x22, 0x1C, // D
    0x7F, 0x49, 0x49, 0x49, 0x41, // E
    0x7F, 0x09, 0x09, 0x09, 0x01, // F
    0x3E, 0x41, 0x49, 0x49, 0x7A, // G
    0x7F, 0x08, 0x08, 0x08, 0x7F, // H
    0x00, 0x41, 0x7F, 0x41, 0x00, // I
    0x20, 0x40, 0x41, 0x3F, 0x01, // J
    0x7F, 0x08, 0x14, 0x22, 0x41, // K
    0x7F, 0x40, 0x40, 0x40, 0x40, // L
    0x7F, 0x02, 0x0C, 0x02, 0x7F, // M
    0x7F, 0x04, 0x08, 0x10, 0x7F, // N
    0x3E, 0x41, 0x41, 0x41, 0x3E, // O
    0x7F, 0x09, 0x09, 0x09, 0x06, // P
    0x3E, 0x41, 0x51, 0x21, 0x5E, // Q
    0x7F, 0x09, 0x19, 0x29, 0x46, // R
    0x46, 0x49, 0x49, 0x49, 0x31, // S
    0x01, 0x01, 0x7F, 0x01, 0x01, // T
    0x3F, 0x40, 0x40, 0x40, 0x3F, // U
    0x1F, 0x20, 0x40, 0x20, 0x1F, // V
    0x3F, 0x40, 0x38, 0x40, 0x3F, // W
    0x63, 0x14, 0x08, 0x14, 0x63, // X
    0x07, 0x08, 0x70, 0x08, 0x07, // Y
    0x61, 0x51, 0x49, 0x45, 0x43, // Z
};

Display::Display() {}

// ST7789 240x135 offsets
static const uint16_t X_OFFSET = 40;
static const uint16_t Y_OFFSET = 53;

void Display::init() {
  spi_init(spi_default, 10 * 1000 * 1000); // 10MHz
  spi_set_format(spi_default, 8, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);
  gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
  gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);

  gpio_init(PIN_CS);
  gpio_set_dir(PIN_CS, GPIO_OUT);
  gpio_put(PIN_CS, 1);

  gpio_init(PIN_DC);
  gpio_set_dir(PIN_DC, GPIO_OUT);
  gpio_put(PIN_DC, 0);

  // Initialize backlight
  gpio_init(PIN_BL);
  gpio_set_dir(PIN_BL, GPIO_OUT);
  gpio_put(PIN_BL, 1);

  // Initialization sequence for ST7789
  write_command(0x01); // SWRESET
  sleep_ms(150);
  write_command(0x11); // SLPOUT
  sleep_ms(50);
  write_command(0x3A); // COLMOD
  write_data(0x05);    // 16-bit color
  write_command(0x36); // MADCTL
  // 0x70: MX | MY | MV (Exchange XY) -> Landscape
  write_data(0x70);
  write_command(0x21); // INVON
  write_command(0x13); // NORON
  write_command(0x29); // DISPON

  // Allocate 64KB Buffer
  framebuffer = (uint16_t *)malloc(DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
  if (!framebuffer) {
    printf("Display: Failed to allocate framebuffer!\n");
  } else {
    memset(framebuffer, 0, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
  }

  clear();
  update();
}

void Display::update() {
  if (!framebuffer)
    return;
  set_window(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT);
  write_data((uint8_t *)framebuffer, DISPLAY_WIDTH * DISPLAY_HEIGHT * 2);
}

void Display::write_command(uint8_t cmd) {
  gpio_put(PIN_DC, 0);
  gpio_put(PIN_CS, 0);
  spi_write_blocking(spi_default, &cmd, 1);
  gpio_put(PIN_CS, 1);
}

void Display::write_data(uint8_t data) {
  gpio_put(PIN_DC, 1);
  gpio_put(PIN_CS, 0);
  spi_write_blocking(spi_default, &data, 1);
  gpio_put(PIN_CS, 1);
}

void Display::write_data(const uint8_t *data, size_t len) {
  gpio_put(PIN_DC, 1);
  gpio_put(PIN_CS, 0);
  spi_write_blocking(spi_default, data, len);
  gpio_put(PIN_CS, 1);
}

void Display::set_window(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  x += X_OFFSET;
  y += Y_OFFSET;

  write_command(0x2A); // CASET
  uint8_t data_x[] = {(uint8_t)(x >> 8), (uint8_t)(x & 0xFF),
                      (uint8_t)((x + w - 1) >> 8),
                      (uint8_t)((x + w - 1) & 0xFF)};
  write_data(data_x, 4);

  write_command(0x2B); // RASET
  uint8_t data_y[] = {(uint8_t)(y >> 8), (uint8_t)(y & 0xFF),
                      (uint8_t)((y + h - 1) >> 8),
                      (uint8_t)((y + h - 1) & 0xFF)};
  write_data(data_y, 4);

  write_command(0x2C); // RAMWR
}

uint16_t Display::color565(Color color) {
  return ((color.r & 0xF8) << 8) | ((color.g & 0xFC) << 3) | (color.b >> 3);
}

void Display::clear(Color color) {
  fill_rect(0, 0, DISPLAY_WIDTH, DISPLAY_HEIGHT, color);
}

void Display::fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                        Color color) {
  if (x >= DISPLAY_WIDTH || y >= DISPLAY_HEIGHT)
    return;
  if (x + w > DISPLAY_WIDTH)
    w = DISPLAY_WIDTH - x;
  if (y + h > DISPLAY_HEIGHT)
    h = DISPLAY_HEIGHT - y;

  if (!framebuffer)
    return;

  uint16_t c = color565(color);
  // Swap bytes because Pico is Little Endian but Display wants Big Endian (MSB
  // first) when sent as byte stream.
  uint16_t swapped = (c >> 8) | (c << 8);

  for (uint16_t j = 0; j < h; j++) {
    for (uint16_t i = 0; i < w; i++) {
      framebuffer[(y + j) * DISPLAY_WIDTH + (x + i)] = swapped;
    }
  }
}

void Display::draw_char(char c, uint16_t x, uint16_t y, Color color,
                        uint8_t scale) {
  if (c >= 'a' && c <= 'z') {
    c -= 32; // Convert to uppercase
  }
  if (c < 32 || c > 90)
    return; // Simple subset check
  const uint8_t *glyph = &font[(c - 32) * 5];

  for (int i = 0; i < 5; i++) {
    uint8_t line = glyph[i];
    for (int j = 0; j < 7; j++) {
      if (line & 0x01) {
        // Pixel on
        fill_rect(x + i * scale, y + j * scale, scale, scale, color);
      }
      line >>= 1;
    }
  }
}

void Display::text(const char *msg, uint16_t x, uint16_t y, Color color,
                   uint8_t scale) {
  uint16_t cursor_x = x;
  uint16_t cursor_y = y;
  while (*msg) {
    if (*msg == '\n') {
      cursor_y += 8 * scale; // 7 height + 1 gap
      cursor_x = x;
    } else {
      draw_char(*msg, cursor_x, cursor_y, color, scale);
      cursor_x += 6 * scale; // 5 width + 1 gap
    }
    msg++;
  }
}

// Bresenham's line algorithm
void Display::draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1,
                        Color color) {
  int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
  int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
  int err = dx + dy, e2;

  while (true) {
    fill_rect(x0, y0, 1, 1, color);
    if (x0 == x1 && y0 == y1)
      break;
    e2 = 2 * err;
    if (e2 >= dy) {
      err += dy;
      x0 += sx;
    }
    if (e2 <= dx) {
      err += dx;
      y0 += sy;
    }
  }
}

void Display::update_status(bool connected, uint16_t power, Color zone_color,
                            bool show_ftp, uint16_t ftp) {
  if (connected) {
    // Fill screen with zone color
    clear(zone_color);

    // Determine text color for contrast
    uint32_t brightness =
        (zone_color.r * 299 + zone_color.g * 587 + zone_color.b * 114) / 1000;
    Color text_color =
        (brightness > 128) ? Color{0, 0, 0} : Color{255, 255, 255};

    // --- Draw Graph ---
    power_history.push_back(power);
    if (power_history.size() > DISPLAY_WIDTH) {
      power_history.pop_front();
    }

    uint16_t prev_x = 0;
    uint16_t prev_y = DISPLAY_HEIGHT - 1; // Start at bottom

    // Scaling: 400W -> ~Half screen? Or full?
    // Let's map 0-500W to DISPLAY_HEIGHT-1 to 0
    const uint16_t MAX_GRAPH_POWER = 500;

    for (size_t i = 0; i < power_history.size(); i++) {
      uint16_t p = power_history[i];
      if (p > MAX_GRAPH_POWER)
        p = MAX_GRAPH_POWER;

      uint16_t y =
          DISPLAY_HEIGHT - 1 - (p * (DISPLAY_HEIGHT - 1) / MAX_GRAPH_POWER);
      uint16_t x = i;

      if (i > 0) {
        draw_line(prev_x, prev_y, x, y, text_color);
      }
      prev_x = x;
      prev_y = y;
    }
    // ------------------

    // Draw "WATTS"
    text("WATTS", 80, 20, text_color, 2);

    // Draw FTP if enabled (Top Right)
    if (show_ftp) {
      char ftp_buf[16];
      sprintf(ftp_buf, "FTP: %d", ftp);
      // Align top right. Width ~8 chars * 6 * 2 (scale 2) = 96px
      // x = 240 - 96 - 5 = ~139
      text(ftp_buf, 140, 5, text_color, 2);
    }

    // Draw Power Number (Large)
    char buf[16];
    sprintf(buf, "%d", power);

    // Simple centering attempt (assumes char width = 6 * scale)
    // Scale 10 = 60px width per char
    // Screen width 240
    int len = strlen(buf);
    int char_width_px = 6 * 10;
    int total_width = len * char_width_px;
    int start_x = (DISPLAY_WIDTH - total_width) / 2;
    if (start_x < 0)
      start_x = 0; // Prevent wrapping

    text(buf, start_x, 50, text_color, 10);
  } else {
    // Scanning Mode (Black Background)
    clear({0, 0, 0});
    text("SCANNING...", 10, 10, {255, 0, 0}, 3);
    // Draw logs handles its own text drawing
  }

  if (!connected)
    draw_logs(); // Draw logs on top of scanning screen

  update(); // <--- FLUSH TO SCREEN
}

void Display::add_log_line(const char *msg) {
  if (log_lines.size() >= MAX_LOG_LINES) {
    log_lines.erase(log_lines.begin());
  }
  log_lines.push_back(std::string(msg));
  draw_logs();
}

void Display::draw_logs() {
  uint16_t start_y = 50;
  // clear log area (y=50 to bottom)
  fill_rect(0, start_y, DISPLAY_WIDTH, DISPLAY_HEIGHT - start_y, {0, 0, 0});

  for (size_t i = 0; i < log_lines.size(); i++) {
    text(log_lines[i].c_str(), 5, start_y + (i * 10), {200, 200, 200}, 1);
  }
  // Note: draw_logs is usually called inside update_status when !connected,
  // so update() will be called there. If called linearly (e.g. init),
  // explicit update() might be needed, but for the flickering issue (dynamic
  // updates), handling it in update_status is sufficient.
}
