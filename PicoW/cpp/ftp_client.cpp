#include "ftp_client.hpp"
#include "lwip/dns.h"
#include "lwip/tcp.h"
#include "pico/cyw43_arch.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {

constexpr const char *FTP_HOST = "astill.mobi";
constexpr const char *FTP_PATH = "/PredictionLeague/jaftp.json";
constexpr uint32_t TIMEOUT_MS = 5000;

volatile bool dns_done;
volatile bool dns_ok;
ip_addr_t resolved_ip;

void dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg) {
  (void)name;
  (void)arg;
  if (ipaddr) {
    resolved_ip = *ipaddr;
    dns_ok = true;
  }
  dns_done = true;
}

volatile bool tcp_done;
char response_buf[256];
size_t response_len;

err_t recv_cb(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) {
  (void)arg;
  (void)err;
  if (!p) {
    tcp_close(tpcb);
    tcp_done = true;
    return ERR_OK;
  }

  size_t copy_len = p->tot_len;
  if (response_len + copy_len >= sizeof(response_buf))
    copy_len = sizeof(response_buf) - 1 - response_len;
  if (copy_len > 0) {
    pbuf_copy_partial(p, response_buf + response_len, copy_len, 0);
    response_len += copy_len;
  }

  tcp_recved(tpcb, p->tot_len);
  pbuf_free(p);
  return ERR_OK;
}

void err_cb(void *arg, err_t err) {
  (void)arg;
  (void)err;
  tcp_done = true;
}

err_t connected_cb(void *arg, struct tcp_pcb *tpcb, err_t err) {
  (void)arg;
  if (err != ERR_OK) {
    tcp_done = true;
    return err;
  }

  char request[160];
  snprintf(request, sizeof(request),
           "GET %s HTTP/1.1\r\n"
           "Host: %s\r\n"
           "Connection: close\r\n"
           "\r\n",
           FTP_PATH, FTP_HOST);

  tcp_write(tpcb, request, strlen(request), TCP_WRITE_FLAG_COPY);
  tcp_output(tpcb);
  tcp_recv(tpcb, recv_cb);
  return ERR_OK;
}

} // namespace

bool fetch_remote_ftp(uint16_t &out_ftp) {
  printf("[FTP] Fetching http://%s%s...\n", FTP_HOST, FTP_PATH);

  dns_done = false;
  dns_ok = false;

  cyw43_arch_lwip_begin();
  err_t err = dns_gethostbyname(FTP_HOST, &resolved_ip, dns_found_cb, nullptr);
  cyw43_arch_lwip_end();

  if (err == ERR_OK) {
    // Already cached, callback will not fire.
    dns_ok = true;
    dns_done = true;
  } else if (err != ERR_INPROGRESS) {
    printf("[FTP] DNS lookup failed to start: %d\n", err);
    return false;
  }

  uint32_t start = to_ms_since_boot(get_absolute_time());
  while (!dns_done) {
    if (to_ms_since_boot(get_absolute_time()) - start > TIMEOUT_MS) {
      printf("[FTP] DNS lookup timed out\n");
      return false;
    }
    cyw43_arch_poll();
    sleep_ms(10);
  }

  if (!dns_ok) {
    printf("[FTP] DNS lookup failed for %s\n", FTP_HOST);
    return false;
  }

  response_len = 0;
  memset(response_buf, 0, sizeof(response_buf));
  tcp_done = false;

  cyw43_arch_lwip_begin();
  struct tcp_pcb *pcb = tcp_new();
  if (!pcb) {
    cyw43_arch_lwip_end();
    printf("[FTP] Failed to create PCB\n");
    return false;
  }
  tcp_err(pcb, err_cb);
  err = tcp_connect(pcb, &resolved_ip, 80, connected_cb);
  cyw43_arch_lwip_end();

  if (err != ERR_OK) {
    printf("[FTP] tcp_connect failed: %d\n", err);
    return false;
  }

  start = to_ms_since_boot(get_absolute_time());
  while (!tcp_done) {
    if (to_ms_since_boot(get_absolute_time()) - start > TIMEOUT_MS) {
      printf("[FTP] Request timed out\n");
      cyw43_arch_lwip_begin();
      tcp_abort(pcb);
      cyw43_arch_lwip_end();
      return false;
    }
    cyw43_arch_poll();
    sleep_ms(10);
  }

  char *body = strstr(response_buf, "\r\n\r\n");
  if (!body) {
    printf("[FTP] No HTTP body found in response\n");
    return false;
  }
  body += 4;

  char *key = strstr(body, "\"FTP\"");
  if (!key) {
    printf("[FTP] \"FTP\" key not found in response body\n");
    return false;
  }

  char *colon = strchr(key, ':');
  if (!colon) {
    printf("[FTP] Malformed JSON (no colon after key)\n");
    return false;
  }

  long value = strtol(colon + 1, nullptr, 10);
  if (value <= 0 || value > 8000) {
    printf("[FTP] Parsed FTP value out of range: %ld\n", value);
    return false;
  }

  out_ftp = (uint16_t)value;
  printf("[FTP] Remote FTP: %u\n", out_ftp);
  return true;
}
