#pragma once

#include "config.h"

// Blocking fetch of the rider's FTP from a remote JSON endpoint
// (http://astill.mobi/PredictionLeague/jaftp.json -> {"FTP":227}).
// Must be called after WiFi is connected and before the BTstack run loop
// starts (uses cyw43_arch_poll() to pump lwIP synchronously, like
// HueClient::check_reachable()).
// Returns true and sets out_ftp on success; returns false (out_ftp left
// untouched) on any DNS/connection/parse failure so the caller can keep
// DEFAULT_FTP.
bool fetch_remote_ftp(uint16_t &out_ftp);
