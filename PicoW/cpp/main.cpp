#include "ble_client.hpp"
#include "btstack_run_loop.h"
#include "display.hpp"
#include "leds.hpp"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <cstdio>
#include <deque>
#include <numeric>

LEDController leds;
Display display;
BLEClient client;

static btstack_timer_source_t heartbeat;
static uint16_t last_power = 0;

// Smoothing Buffer (Size 3 for approx 1s smoothing if ~3Hz updates)
static std::deque<uint16_t> power_history;
static const size_t SMOOTHING_WINDOW = 3;

void heartbeat_handler(btstack_timer_source_t *ts) {
  if (client.is_connected()) {
    printf("[Status] Connected | Power: %d W\n", last_power);
  } else {
    printf("[Status] Scanning...\n");
  }
  btstack_run_loop_set_timer(ts, 5000);
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

  Color zone_color = leds.update_from_power(avg_power);
  display.update_status(true, avg_power, zone_color);
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
  printf("ZwiftPowerLighting C++ Starting...\n");

  // Initialize CYW43 (Required for WiFi/BT)
  if (cyw43_arch_init()) {
    printf("Failed to initialize cyw43_arch\n");
    return -1;
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
  btstack_run_loop_set_timer(&heartbeat, 2000);
  btstack_run_loop_add_timer(&heartbeat);

  // Note: In C++ / generic BTstack, the 'main loop' is handled by BTstack.
  // We don't write a `while(true)` loop.
  // Instead we rely on callbacks.
  printf("Entering BTstack Run Loop...\n");
  btstack_run_loop_execute();

  return 0;
}
