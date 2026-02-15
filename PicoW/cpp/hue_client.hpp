#pragma once

#include "config.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include <string>

// To avoid flooding the bridge
#define HUE_UPDATE_INTERVAL_MS 1000

class HueClient {
public:
  HueClient();
  void init();
  void update(Color color);
  void turn_off();
  void on_request_complete() { request_in_progress = false; }
  bool check_reachable();

  bool hub_reachable;

private:
  uint32_t last_update_ms;
  Color last_color;
  bool request_in_progress;
  bool first_run;

  void send_request(uint16_t hue, uint8_t sat, uint8_t bri);
};
