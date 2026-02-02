#include "hue_client.hpp"
#include "lwip/ip_addr.h"
#include "lwip/tcp.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// Defined in CMake from .env
#ifndef HUE_IP
#define HUE_IP "192.168.1.100"
#endif
#ifndef HUE_USER
#define HUE_USER "user"
#endif
#ifndef HUE_GROUP
#define HUE_GROUP "1"
#endif

// TCP Callback prototypes
static err_t hue_connected(void *arg, struct tcp_pcb *tpcb, err_t err);
static void hue_err(void *arg, err_t err);

static HueClient *g_hue_client = nullptr;

HueClient::HueClient()
    : last_update_ms(0), request_in_progress(false), first_run(true) {
  last_color = {0, 0, 0};
  g_hue_client = this;
}

void HueClient::init() {
  printf("Hue Client Initialized. Target: %s Group: %s\n", HUE_IP, HUE_GROUP);
}

// Simple RGB to HSV/Hue mapping
// Hue: 0-65535
// Sat: 0-254
// Bri: 0-254
void HueClient::update(Color color) {
  uint32_t now = to_ms_since_boot(get_absolute_time());
  printf("[Hue] Update? Now:%lu Last:%lu InProg:%d First:%d Color:%d,%d,%d\n",
         now, last_update_ms, request_in_progress, first_run, color.r, color.g,
         color.b);
  if (now - last_update_ms < HUE_UPDATE_INTERVAL_MS) {
    // printf("[Hue] Throttled.\n");
    return;
  }
  if (request_in_progress) {
    if (now - last_update_ms > 5000) {
      printf("[Hue] Request stuck > 5s. Forcing reset.\n");
      request_in_progress = false;
    } else {
      printf("[Hue] Request in progress. Skipping.\n");
      return;
    }
  }

  // Check if color changed significantly?
  // Check if color changed significantly?
  if (color.r == last_color.r && color.g == last_color.g &&
      color.b == last_color.b) {
    if (now - last_update_ms > 5000) {
      printf("[Hue] Color unchanged (R%d G%d B%d). Skipping.\n", color.r,
             color.g, color.b);
      last_update_ms = now;
    }
    return;
  }

  last_update_ms = now;
  last_color = color;
  last_update_ms = now;
  last_color = color;
  request_in_progress = true;
  first_run = false;

  // Convert RGB to Hue/Sat/Bri
  // This is a rough approximation.
  // Normalized R,G,B [0,1]
  float r = color.r / 255.0f;
  float g = color.g / 255.0f;
  float b = color.b / 255.0f;

  float max_val = (r > g) ? ((r > b) ? r : b) : ((g > b) ? g : b);
  float min_val = (r < g) ? ((r < b) ? r : b) : ((g < b) ? g : b);
  float delta = max_val - min_val;

  float hue_f = 0;
  if (delta == 0) {
    hue_f = 0;
  } else if (max_val == r) {
    hue_f = 60 * fmod(((g - b) / delta), 6.0f);
  } else if (max_val == g) {
    hue_f = 60 * (((b - r) / delta) + 2);
  } else {
    hue_f = 60 * (((r - g) / delta) + 4);
  }

  if (hue_f < 0)
    hue_f += 360;

  // Map 0-360 -> 0-65535
  uint16_t hue_api = (uint16_t)((hue_f / 360.0f) * 65535);

  // Saturation
  float sat_f = (max_val == 0) ? 0 : (delta / max_val);
  uint8_t sat_api = (uint8_t)(sat_f * 254);

  // Brightness
  uint8_t bri_api = (uint8_t)(max_val * 254);

  // Ensure "on":true is sent. If Black, turn off?
  // User logic said: {"on":true, ...}
  // If color is black (0,0,0), maybe turn off?
  // Let's stick to user request for structure but handle OFF if bri is 0.

  send_request(hue_api, sat_api, bri_api);
}

void HueClient::send_request(uint16_t hue, uint8_t sat, uint8_t bri) {
  struct tcp_pcb *pcb = tcp_new();
  if (!pcb) {
    printf("Hue: Failed to create PCB\n");
    request_in_progress = false;
    return;
  }

  char *payload = (char *)malloc(512);
  if (!payload) {
    tcp_close(pcb);
    request_in_progress = false;
    return;
  }

  // Construct JSON Body
  char body[128];
  if (bri == 0) {
    sprintf(body, "{\"on\":false}");
  } else {
    sprintf(body, "{\"on\":true, \"sat\":%d, \"bri\":%d, \"hue\":%d}", sat, bri,
            hue);
  }

  // Construct HTTP Request
  sprintf(payload,
          "PUT /api/%s/groups/%s/action HTTP/1.1\r\n"
          "Host: %s\r\n"
          "Content-Type: application/json\r\n"
          "Content-Length: %d\r\n"
          "\r\n"
          "%s",
          HUE_USER, HUE_GROUP, HUE_IP, (int)strlen(body), body);

  printf("[Hue] Target: %s URL: /api/%s/groups/%s/action\n", HUE_IP, HUE_USER,
         HUE_GROUP);
  printf("[Hue] Body: %s\n", body);

  tcp_arg(pcb, payload); // Pass payload as arg
  tcp_err(pcb, hue_err);

  ip_addr_t ip;
  if (!ipaddr_aton(HUE_IP, &ip)) {
    printf("Hue: Invalid IP Address string: '%s'\n", HUE_IP);
    free(payload);
    tcp_close(pcb);
    request_in_progress = false;
    return;
  }

  // printf("Hue: Connecting to %s...\n", ipaddr_ntoa(&ip)); // Optional debug

  tcp_connect(pcb, &ip, 80, hue_connected);
}

// Callbacks
static err_t hue_recv(void *arg, struct tcp_pcb *tpcb, struct pbuf *p,
                      err_t err) {
  if (!p) {
    // Server closed connection
    tcp_close(tpcb);
    if (g_hue_client)
      g_hue_client->on_request_complete();
    return ERR_OK;
  }

  // Reading response (limited by pbuf structure, but good enough for status)
  if (p->payload) {
    char buffer[128];
    uint16_t len = p->len > 127 ? 127 : p->len;
    memcpy(buffer, p->payload, len);
    buffer[len] = 0;
    printf("[Hue] Rx: %s\n", buffer);
  }

  pbuf_free(p);
  tcp_close(tpcb); // Close after first response
  if (g_hue_client)
    g_hue_client->on_request_complete();

  return ERR_OK;
}

static err_t hue_connected(void *arg, struct tcp_pcb *tpcb, err_t err) {
  if (err != ERR_OK) {
    printf("Hue Connect Error: %d\n", err);
    if (arg)
      free(arg);
    if (g_hue_client)
      g_hue_client->on_request_complete();
    return err;
  }

  char *payload = (char *)arg;
  tcp_write(tpcb, payload, strlen(payload), TCP_WRITE_FLAG_COPY);
  tcp_output(tpcb);

  free(payload);

  // Do NOT close immediately. Wait for response.
  tcp_recv(tpcb, hue_recv);

  return ERR_OK;
}

static void hue_err(void *arg, err_t err) {
  if (arg)
    free(arg);
  if (g_hue_client)
    g_hue_client->on_request_complete();
  // printf("Hue Error: %d\n", err);
}
