#include "ble_client.hpp"
#include <cstdio>
#include <cstring>

static BLEClient *instance = nullptr;

static void static_packet_handler(uint8_t packet_type, uint16_t channel,
                                  uint8_t *packet, uint16_t size) {
  if (instance) {
    instance->packet_handler(packet_type, channel, packet, size);
  }
}

BLEClient::BLEClient()
    : connected(false), connection_handle(HCI_CON_HANDLE_INVALID),
      last_notification_ms(0) {
  instance = this;
}

void BLEClient::set_power_callback(PowerCallback cb) { power_callback = cb; }
void BLEClient::set_scan_callback(ScanCallback cb) { scan_callback = cb; }

bool BLEClient::is_connected() { return connected; }

void BLEClient::init() {
  l2cap_init();
  sm_init(); // Initialize Security Manager
  gatt_client_init();

  hci_event_callback_registration.callback = &static_packet_handler;
  hci_add_event_handler(&hci_event_callback_registration);

  // Turn on Bluetooth
  hci_power_control(HCI_POWER_ON);
}

void BLEClient::packet_handler(uint8_t packet_type, uint16_t channel,
                               uint8_t *packet, uint16_t size) {
  if (packet_type != HCI_EVENT_PACKET)
    return;

  uint8_t event = hci_event_packet_get_type(packet);
  printf("HCI Event: 0x%02x\n", event);

  switch (event) {
  case BTSTACK_EVENT_STATE:
    if (btstack_event_state_get_state(packet) == HCI_STATE_WORKING) {
      printf("BLE Enabled. Scanning for %s...\n", BLE_TARGET_NAME.c_str());
      // Active scanning (1), Interval 48ms (0x30), Window 48ms (0x30)
      gap_set_scan_parameters(1, 0x0030, 0x0030);
      gap_start_scan();
      printf("gap_start_scan called\n");
    }
    break;

  case GAP_EVENT_ADVERTISING_REPORT: {
    uint8_t event_type =
        gap_event_advertising_report_get_advertising_event_type(packet);
    uint8_t data_length = gap_event_advertising_report_get_data_length(packet);
    const uint8_t *data = gap_event_advertising_report_get_data(packet);

    // printf("Adv Report: Type %d, Len %d\n", event_type, data_length);

    int i = 0;
    while (i < data_length) {
      uint8_t len = data[i];
      if (len == 0)
        break;
      uint8_t type = data[i + 1];

      if (type == 0x09 || type == 0x08) { // Complete or Shortened Local Name
        char name_str[32] = {0};
        int name_len = len - 1;
        if (name_len > 31)
          name_len = 31;
        memcpy(name_str, &data[i + 2], name_len);

        bd_addr_t address;
        gap_event_advertising_report_get_address(packet, address);
        const char *mac_str = bd_addr_to_str(address);

        // printf("  Found Name: %s (MAC: %s)\n", name_str, mac_str);

        if (instance && instance->scan_callback) {
          instance->scan_callback(mac_str, name_str);
        }

        if (len - 1 == BLE_TARGET_NAME.length()) {
          if (memcmp(&data[i + 2], BLE_TARGET_NAME.c_str(), len - 1) == 0) {
            if (event_type == 0 || event_type == 1 || event_type == 4) {
              printf("Found Target! Connecting...\n");
              gap_stop_scan();
              bd_addr_t addr;
              gap_event_advertising_report_get_address(packet, addr);
              gap_connect(
                  addr,
                  (bd_addr_type_t)gap_event_advertising_report_get_address_type(
                      packet));
              break;
            } else {
              // Not connectable
            }
          }
        }
      }
      i += len + 1;
    }
    break;
  }

  case HCI_EVENT_LE_META:
    if (hci_event_le_meta_get_subevent_code(packet) ==
        HCI_SUBEVENT_LE_CONNECTION_COMPLETE) {
      connection_handle =
          hci_subevent_le_connection_complete_get_connection_handle(packet);
      printf("Connected! Handle: 0x%04x. Starting Service Discovery...\n",
             connection_handle);
      connected = true;
      last_notification_ms = to_ms_since_boot(get_absolute_time());

      // Update UI immediately
      if (instance && instance->power_callback) {
        instance->power_callback(0);
      }

      // LOG BEFORE CALLING
      printf("Calling gatt_client_discover_primary_services...\n");

      discovery_state = DiscoveryState::DISCOVERING_SERVICES;
      // Discover ALL services
      uint8_t status = gatt_client_discover_primary_services(
          static_packet_handler, connection_handle);

      // FLUSH STDOUT? Use fflush(stdout);
      // Pico SDK stdio might not support it, but worth a try or just rely on \n
      printf("Discovery Request Sent. Status: 0x%02x\n", status);
    }
    break;

    // Removed GATT_EVENT_MTU case entirely

  case HCI_EVENT_DISCONNECTION_COMPLETE:
    printf("Disconnected.\n");
    connected = false;
    connection_handle = HCI_CON_HANDLE_INVALID;
    discovery_state = DiscoveryState::IDLE;
    memset(&service, 0, sizeof(service));
    memset(&characteristic, 0, sizeof(characteristic));
    gap_start_scan();
    break;

  case GATT_EVENT_SERVICE_QUERY_RESULT: {
    gatt_client_service_t found_service;
    gatt_event_service_query_result_get_service(packet, &found_service);

    printf("Service: Start 0x%04x, End 0x%04x. UUID: ",
           found_service.start_group_handle, found_service.end_group_handle);
    for (int i = 0; i < 16; i++)
      printf("%02x", found_service.uuid128[i]);
    printf("\n");

    // Check for 0x1818 (Cycling Power) - Big Endian check for bytes 2,3
    if (found_service.uuid128[2] == 0x18 && found_service.uuid128[3] == 0x18) {
      printf("CHECK MATCH: Found 0x1818!\n");
      service = found_service;
    }
    // Also Check for 0x1800 (Generic Access) for validation
    if (found_service.uuid128[2] == 0x18 && found_service.uuid128[3] == 0x00) {
      printf("CHECK MATCH: Found Generic Access (0x1800)!\n");
    }
    break;
  }

  case GATT_EVENT_CHARACTERISTIC_QUERY_RESULT: {
    gatt_client_characteristic_t temp_char;
    gatt_event_characteristic_query_result_get_characteristic(packet,
                                                              &temp_char);

    printf("Char: ValHandle 0x%04x. UUID: ", temp_char.value_handle);
    for (int i = 0; i < 16; i++)
      printf("%02x", temp_char.uuid128[i]);
    printf("\n");

    // Check for 0x2A63 (Power Measurement)
    // Based on logs: 00002a63... -> Index 2=0x2a, Index 3=0x63
    if (temp_char.uuid128[2] == 0x2a && temp_char.uuid128[3] == 0x63) {
      printf("CHECK MATCH: Found Cycling Power (0x2A63)!\n");
      characteristic = temp_char;
    }
    break;
  }

  case GATT_EVENT_QUERY_COMPLETE:
    printf("GATT Query Complete. Status: 0x%02x. State: %d\n",
           gatt_event_query_complete_get_att_status(packet),
           (int)discovery_state);

    if (gatt_event_query_complete_get_att_status(packet) != 0) {
      printf("Query failed or finished with error.\n");
      if (gatt_event_query_complete_get_att_status(packet) == 0x7F) {
        printf("Error 0x7F might mean Attribute Not Found.\n");
      }
    }

    switch (discovery_state) {
    case DiscoveryState::DISCOVERING_SERVICES:
      if (service.start_group_handle != 0) {
        printf("Service Found! Discovering Characteristics...\n");
        discovery_state = DiscoveryState::DISCOVERING_CHARS;
        // Discover ALL characteristics to ensure we see 2A63
        gatt_client_discover_characteristics_for_service(
            static_packet_handler, connection_handle, &service);
      } else {
        printf("FATAL: Service 0x1818 was not found. STALLING.\n");
        // discovery_state = DiscoveryState::IDLE;
      }
      break;

    case DiscoveryState::DISCOVERING_CHARS:
      if (characteristic.value_handle != 0) {
        printf("Characteristic 2A63 Found (Handle 0x%04x). Subscribing...\n",
               characteristic.value_handle);
        discovery_state = DiscoveryState::SUBSCRIBING;

        // 1. Register Notification Listener
        gatt_client_listen_for_characteristic_value_updates(
            &notification_registration, static_packet_handler,
            connection_handle, &characteristic);

        // 2. Enable Notifications via CCCD
        gatt_client_write_client_characteristic_configuration(
            static_packet_handler, connection_handle, &characteristic,
            GATT_CLIENT_CHARACTERISTICS_CONFIGURATION_NOTIFICATION);
      } else {
        printf("FATAL: Characteristic 0x2A63 not found.\n");
        discovery_state = DiscoveryState::IDLE;
      }
      break;

    case DiscoveryState::SUBSCRIBING:
      printf("Subscription Complete (Notifications Enabled). State -> "
             "SUBSCRIBED.\n");
      discovery_state = DiscoveryState::SUBSCRIBED;
      break;

    default:
      break;
    }
    break;

  case GATT_EVENT_NOTIFICATION: {
    uint16_t value_len = gatt_event_notification_get_value_length(packet);
    const uint8_t *value = gatt_event_notification_get_value(packet);

    printf("Notification! Len: %d. Data: ", value_len);
    for (int i = 0; i < value_len && i < 10; i++)
      printf("%02x ", value[i]);
    printf("\n");

    if (value_len >= 4) {
      int16_t power = (int16_t)(value[2] | (value[3] << 8));
      // printf("Parsed Power: %d\n", power);
      if (power_callback) {
        power_callback((uint16_t)power);
      }
    }
    // Update Watchdog
    if (instance) {
      instance->last_notification_ms = to_ms_since_boot(get_absolute_time());
    }
    break;
  }
  }
}

void BLEClient::check_watchdog() {
  if (!connected)
    return;

  uint32_t now = to_ms_since_boot(get_absolute_time());
  if (now - last_notification_ms > 5000) {
    printf("[Watchdog] No notifications for 5s. Forcing Disconnect!\n");
    gap_disconnect(connection_handle);
  }
}
