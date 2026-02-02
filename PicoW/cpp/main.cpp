#include "ble_client.hpp"
#include "btstack_run_loop.h"
#include "display.hpp"
#include "hue_client.hpp"
#include "leds.hpp"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <deque>
#include <numeric>

LEDController leds;
Display display;
BLEClient client;
HueClient hue;

static btstack_timer_source_t heartbeat;
static btstack_timer_source_t ui_timer;

static uint16_t last_power = 0;
static uint16_t current_ftp = DEFAULT_FTP;
static bool show_ftp = false;

// Button State Tracking (Simple Polling)
struct Button {
  uint pin;
  bool last_state; // true = pressed (LOW due to pullup)

  void init(uint p) {
    pin = p;
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    gpio_pull_up(pin);
    last_state = false;
  }

  bool is_pressed() {
    return !gpio_get(pin); // Active Low
  }

  bool just_pressed() {
    bool pressed = is_pressed();
    bool just = pressed && !last_state;
    last_state = pressed;
    return just;
  }
};

Button btn_a, btn_b, btn_x, btn_y;

// Smoothing Buffer (Size 3 for approx 1s smoothing if ~3Hz updates)
static std::deque<uint16_t> power_history;
static const size_t SMOOTHING_WINDOW = 3;

void heartbeat_handler(btstack_timer_source_t *ts) {
  if (client.is_connected()) {
    printf("[Status] Connected | Power: %d W | FTP: %d\n", last_power,
           current_ftp);
    client.check_watchdog();
  } else {
    // printf("[Status] Scanning...\n");
  }
  btstack_run_loop_set_timer(ts, 1000);
  btstack_run_loop_add_timer(ts);
}

void on_power_update(uint16_t raw_power) {
  // Add to buffer
  power_history.push_back(raw_power);
  if (power_history.size() > SMOOTHING_WINDOW) {
    power_history.pop_front();
  }

  // Calculate Average
  uint32_t sum = 0;
  for (uint16_t p : power_history)
    sum += p;
  uint16_t avg_power = sum / power_history.size();

  last_power = avg_power;
  printf("Power: %d W (Raw: %d)\n", avg_power, raw_power);

  Color zone_color = leds.update_from_power(avg_power, current_ftp);
  display.update_status(true, avg_power, zone_color, show_ftp, current_ftp);

  // Update Hue
  cyw43_arch_lwip_begin();
  hue.update(zone_color);
  cyw43_arch_lwip_end();
}

void ui_handler(btstack_timer_source_t *ts) {
  // Poll Buttons
  if (btn_y.just_pressed()) {
    show_ftp = !show_ftp;
    printf("UI: Toggle FTP Display -> %d\n", show_ftp);
  }

  bool changed = false;
  if (show_ftp) {
    if (btn_a.just_pressed()) {
      current_ftp++;
      changed = true;
      printf("UI: FTP +1 -> %d\n", current_ftp);
    }
    if (btn_b.just_pressed()) {
      if (current_ftp > 0)
        current_ftp--;
      changed = true;
      printf("UI: FTP -1 -> %d\n", current_ftp);
    }
  }

  // Force display update if UI changed and we are connected (so the screen is
  // active)
  if (changed || (btn_y.just_pressed() && client.is_connected())) {
    Color zone_color = leds.update_from_power(last_power, current_ftp);
    display.update_status(true, last_power, zone_color, show_ftp, current_ftp);
  }

  btstack_run_loop_set_timer(ts, 50); // 20Hz polling
  btstack_run_loop_add_timer(ts);
}

void on_scan_result(const char *mac, const char *name) {
  printf("Found: %s | %s\n", mac, name);
  char buf[64];
  if (strlen(name) > 0) {
    snprintf(buf, sizeof(buf), "> %s", name);
  } else {
    snprintf(buf, sizeof(buf), "> %s", mac);
  }
  display.add_log_line(buf);
}

int main() {
  stdio_init_all();
  sleep_ms(5000); // Wait for USB serial
  printf("ZwiftPowerLighting C++ Starting... VERSION 2.0 (STABLE)\n");

  // Initialize CYW43 (Required for WiFi/BT)
  if (cyw43_arch_init()) {
    printf("Failed to initialize cyw43_arch\n");
    return -1;
  }

  // WiFi Connection
  cyw43_arch_enable_sta_mode();
  printf("Connecting to WiFi: %s...\n", WIFI_SSID);
  int res = cyw43_arch_wifi_connect_timeout_ms(WIFI_SSID, WIFI_PASSWORD,
                                               CYW43_AUTH_WPA2_AES_PSK, 15000);
  if (res) {
    printf("WiFi Connection Failed! Error: %d\n", res);
    // Continue anyway? Or halt? Let's continue but log error.
  } else {
    printf("WiFi Connected!\n");
    hue.init();
  }

  // 1. Initialize
  leds.init();
  display.init();
  display.text("ZwiftPowerLighting\nC++ Starting...", 10, 10, {255, 255, 255},
               2);

  // 2. Startup Cycle
  leds.startup_cycle();
  display.clear();

  // 3. Set White (Scan Mode)
  leds.fill({255, 255, 255});
  display.text("Scanning...", 10, 10, {255, 0, 0}, 3);
  display.draw_logs(); // Initial draw

  // 4. Initialize BLE
  client.set_power_callback(on_power_update);
  client.set_scan_callback(on_scan_result);
  client.init();

  // 5. Setup Heartbeat Timer
  heartbeat.process = &heartbeat_handler;
  btstack_run_loop_set_timer(&heartbeat, 1000);
  btstack_run_loop_add_timer(&heartbeat);

  // 6. Setup UI/Button Timer
  btn_a.init(PIN_BUTTON_A);
  btn_b.init(PIN_BUTTON_B);
  btn_x.init(PIN_BUTTON_X);
  btn_y.init(PIN_BUTTON_Y);

  ui_timer.process = &ui_handler;
  btstack_run_loop_set_timer(&ui_timer, 50);
  btstack_run_loop_add_timer(&ui_timer);

  // Note: In C++ / generic BTstack, the 'main loop' is handled by BTstack.
  // We don't write a `while(true)` loop.
  // Instead we rely on callbacks.
  printf("Entering BTstack Run Loop...\n");
  btstack_run_loop_execute();

  return 0;
}
