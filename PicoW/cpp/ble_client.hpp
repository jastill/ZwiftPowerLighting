#pragma once

#include "btstack.h"
#include "config.h"
#include "pico/stdlib.h"
#include <functional>

// Callback type for power updates
using PowerCallback = std::function<void(uint16_t)>;
// Callback type for scan results (MAC, Name)
using ScanCallback = std::function<void(const char *, const char *)>;

class BLEClient {
public:
  BLEClient();
  void init();
  void set_power_callback(PowerCallback cb);
  void set_scan_callback(ScanCallback cb);
  bool is_connected();

  // Internal use (public so C-style callbacks can reach them)
  void packet_handler(uint8_t packet_type, uint16_t channel, uint8_t *packet,
                      uint16_t size);

private:
  PowerCallback power_callback;
  ScanCallback scan_callback;
  bool connected;
  hci_con_handle_t connection_handle;

  // GATT Handles
  gatt_client_service_t service;
  gatt_client_characteristic_t characteristic;
  gatt_client_notification_t notification_registration;
  btstack_packet_callback_registration_t hci_event_callback_registration;
};
