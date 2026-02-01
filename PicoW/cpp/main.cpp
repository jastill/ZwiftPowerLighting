#include "ble_client.hpp"
#include "btstack_run_loop.h"
#include "display.hpp"
#include "leds.hpp"
#include "pico/stdlib.h"
#include <cstdio>

LEDController leds;
Display display;
BLEClient client;

static btstack_timer_source_t heartbeat;
static uint16_t last_power = 0;

void heartbeat_handler(btstack_timer_source_t *ts) {
  if (client.is_connected()) {
    printf("[Status] Connected | Power: %d W\n", last_power);
  } else {
    printf("[Status] Scanning...\n");
  }
  btstack_run_loop_set_timer(ts, 2000);
  btstack_run_loop_add_timer(ts);
}

void on_power_update(uint16_t power) {
  last_power = power;
  printf("Power: %d W\n", power);
  Color zone_color = leds.update_from_power(power);
  display.update_status(true, power, zone_color);
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
