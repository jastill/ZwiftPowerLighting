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
static bool hue_enabled = true; // Default ON

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

// Smoothing Buffer (Size 5 for approx 1.5s smoothing if ~3Hz updates)
static std::deque<uint16_t> power_history;
static const size_t SMOOTHING_WINDOW = 5;

static uint32_t btn_x_press_start = 0;
static bool btn_x_handled = false;

// Auto-Repeat State
static uint32_t btn_a_press_start = 0;
static uint32_t btn_b_press_start = 0;
static uint32_t last_repeat_time = 0;

// Auto-Off State
static uint32_t last_active_power_time = 0;
static bool hue_auto_off_sent = false;

void heartbeat_handler(btstack_timer_source_t *ts) {
  if (client.is_connected()) {
    printf("[Status] Connected | Power: %d W | FTP: %d\n", last_power,
           current_ftp);
    client.check_watchdog();

    // Auto Hue Off (60s timeout)
    if (hue_enabled && !hue_auto_off_sent && last_power == 0) {
      uint32_t now = to_ms_since_boot(get_absolute_time());
      // Initialize last_active if 0 (first run)
      if (last_active_power_time == 0)
        last_active_power_time = now;

      if (now - last_active_power_time > 60000) {
        printf("[Auto] Hue Timeout > 60s. Turning Off.\n");
        cyw43_arch_lwip_begin();
        hue.turn_off();
        cyw43_arch_lwip_end();
        hue_auto_off_sent = true;
        display.set_led({0, 0, 0}); // Sync LED Off
      }
    }
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

  if (avg_power > 0) {
    last_active_power_time = to_ms_since_boot(get_absolute_time());
    hue_auto_off_sent = false;
  }

  printf("Power: %d W (Raw: %d)\n", avg_power, raw_power);

  Color zone_color = leds.update_from_power(avg_power, current_ftp);
  display.update_status(true, avg_power, zone_color, show_ftp, current_ftp,
                        hue_enabled);

  // Gate LED Control matches Hue State
  if (hue_enabled && !hue_auto_off_sent) {
    display.set_led(zone_color);
  } else {
    display.set_led({0, 0, 0});
  }

  // Update Hue
  if (hue_enabled) {
    cyw43_arch_lwip_begin();
    hue.update(zone_color);
    cyw43_arch_lwip_end();
  }
}

void ui_handler(btstack_timer_source_t *ts) {
  // Poll Buttons
  if (btn_y.just_pressed()) {
    show_ftp = !show_ftp;
    printf("UI: Toggle FTP Display -> %d\n", show_ftp);
  }

  bool changed = false;
  uint32_t now = to_ms_since_boot(get_absolute_time());

  if (show_ftp) {
    // --- Button A (Decrement) ---
    if (btn_a.just_pressed()) {
      if (current_ftp > 1)
        current_ftp--; // Single click min 1
      changed = true;
      btn_a_press_start = now;
      printf("UI: FTP -1 -> %d\n", current_ftp);
    }

    if (btn_a.is_pressed()) {
      if (btn_a_press_start > 0 && (now - btn_a_press_start > 1000)) {
        // Auto Decrement
        if (now - last_repeat_time >= 20) {
          if (current_ftp > 50) { // Auto min 50
            current_ftp--;
            changed = true;
          }
          last_repeat_time = now;
        }
      }
    } else {
      btn_a_press_start = 0;
    }

    // --- Button B (Increment) ---
    if (btn_b.just_pressed()) {
      if (current_ftp < 8000)
        current_ftp++; // Single click max 8000
      changed = true;
      btn_b_press_start = now;
      printf("UI: FTP +1 -> %d\n", current_ftp);
    }

    if (btn_b.is_pressed()) {
      if (btn_b_press_start > 0 && (now - btn_b_press_start > 1000)) {
        // Auto Increment
        if (now - last_repeat_time >= 20) {
          if (current_ftp < 8000) { // Auto max 8000
            current_ftp++;
            changed = true;
          }
          last_repeat_time = now;
        }
      }
    } else {
      btn_b_press_start = 0;
    }
  }

  // Button X Long Press Logic (Hue Toggle)
  if (btn_x.is_pressed()) {
    if (btn_x_press_start == 0) {
      btn_x_press_start = now;
      btn_x_handled = false;
    } else {
      if (!btn_x_handled && (now - btn_x_press_start > 2000)) {
        // Long Press Triggered
        hue_enabled = !hue_enabled;
        printf("UI: Hue Toggle -> %d\n", hue_enabled);

        cyw43_arch_lwip_begin();
        if (!hue_enabled) {
          hue.turn_off();
          display.set_led({0, 0, 0}); // Sync LED Off
        } else {
          // Immediate Wake with current settings
          Color zone_color = leds.update_from_power(last_power, current_ftp);
          hue.update(zone_color);
          display.set_led(zone_color); // Sync LED On
        }
        cyw43_arch_lwip_end();

        btn_x_handled = true;
        changed = true;
      }
    }
  } else {
    btn_x_press_start = 0;
    btn_x_handled = false;
  }

  // Force display update if UI changed and we are connected (so the screen is
  // active)
  if (changed || (btn_y.just_pressed() && client.is_connected())) {
    Color zone_color = leds.update_from_power(last_power, current_ftp);
    display.update_status(true, last_power, zone_color, show_ftp, current_ftp,
                          hue_enabled);
  }

  btstack_run_loop_set_timer(ts, 20); // Poll at 50Hz (20ms)
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
