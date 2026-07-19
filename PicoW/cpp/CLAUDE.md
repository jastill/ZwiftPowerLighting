# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a Raspberry Pi Pico (W / 2 W) that reads live power output from a Wahoo KICKR CORE turbo trainer over BLE, maps power (relative to rider FTP) to a color zone, and drives that color out to both a local WS2812B LED strip and a Philips Hue light group over WiFi. A Pimoroni Pico Display (ST7789-based SPI screen) shows connection status, current power/FTP, and a scrolling log.

## Build

Two targets exist depending on the board:

```bash
./build_pico2w.sh   # Pico 2 W (RP2350) -> build_pico2w/ZwiftPowerLighting.uf2
./build_picow.sh    # Pico W (RP2040)   -> build_picow/ZwiftPowerLighting.uf2
```

Each script just does `mkdir -p <build_dir> && cd <build_dir> && cmake -DPICO_BOARD=<board> .. && make -j4`. There is no separate lint/test step — correctness is verified by flashing and observing serial output (`pico_enable_stdio_usb` is on, UART stdio is off).

Deploy by copying the `.uf2` to the board mounted in BOOTSEL mode, e.g. `/Volumes/RP2350/` for the Pico 2 W.

Requires the Raspberry Pi Pico SDK v2.0.0+ (the `pico-sdk/` submodule is vendored in this repo) and a `.env` file (see below).

## Configuration via `.env`

`CMakeLists.txt` parses `.env` at configure time and injects each key as a compile definition (`add_compile_definitions(KEY="VALUE")`), so `WIFI_SSID`, `WIFI_PASSWORD`, etc. become string literals directly usable in C++ (see `config.h` / `main.cpp` referencing `WIFI_SSID`). Copy `.env.example` to `.env` and fill in:

```
WIFI_SSID=...
WIFI_PASSWORD=...
HUE_IP=...
HUE_USER=...
HUE_GROUP=...
```

`.env` is gitignored — never commit real credentials. If `.env` is missing, CMake configure emits a warning and WiFi/Hue defines will be absent, which will fail compilation of anything referencing them.

## Architecture

Single-threaded, callback-driven firmware built on BTstack's run loop (no RTOS, no `while(true)` polling loop in `main()`). `main.cpp` wires everything together:

- **`BLEClient`** (`ble_client.cpp/hpp`) — scans for and connects to a BLE Cycling Power Service device matching `BLE_TARGET_NAME` (`config.h`), subscribes to the Power Measurement characteristic (UUID `...2a63`), and invokes a `PowerCallback` with raw watts on each notification. Exposes `check_watchdog()` for connection-loss detection.
- **`LEDController`** (`leds.cpp/hpp`) — drives the WS2812B strip via PIO (`ws2812.pio`, generated header via `pico_generate_pio_header`). `update_from_power()` maps power/FTP into a `PowerZone` (see `POWER_ZONES` in `config.h`) and returns the resulting `Color`. Strip is initialized `is_rgbw = false` (24-bit GRB) — do not switch to RGBW; a prior attempt caused a 4-LED repeating color pattern (end-of-strip yellowing is a voltage-drop symptom, not a bit-width bug).
- **`fetch_remote_ftp`** (`ftp_client.cpp/hpp`) — one-shot blocking HTTP GET (DNS + raw lwIP TCP, polled via `cyw43_arch_poll()` like `HueClient::check_reachable()`) against `http://astill.mobi/PredictionLeague/jaftp.json` (`{"FTP":227}`), called once at startup after WiFi connects. On success it overrides `current_ftp`; on any failure (DNS, connect, timeout, malformed JSON) it leaves `current_ftp` at `DEFAULT_FTP`.
- **`HueClient`** (`hue_client.cpp/hpp`) — pushes the same zone `Color` to a Hue bridge group via raw HTTP PUT (`/api/<user>/groups/<group>/action`) using lwIP, rate-limited by `HUE_UPDATE_INTERVAL_MS`. Tracks `hub_reachable` from an initial reachability check.
- **`Display`** (`display.cpp/hpp`) — bit-banged SPI driver for the ST7789 display plus manual framebuffer text/shape rendering (no external graphics library); shows connection/WiFi/Hue status, power, FTP, and a rolling log (`add_log_line`/`draw_logs`).
- **`main.cpp`** — owns global instances of all four, plus:
  - a 1 Hz `heartbeat_handler` timer for status logging, BLE watchdog checks, and a 60s auto-off timeout for the Hue light when power drops to 0
  - a 50 Hz `ui_handler` timer polling four buttons (A/B decrement/increment FTP with hold-to-repeat, X long-press toggles Hue on/off, Y toggles the FTP display) via a simple active-low `Button` struct
  - `on_power_update`, the BLE power callback: smooths raw watts over a 5-sample rolling window (`power_history`), derives the zone `Color`, and pushes it to both `LEDController`/`Display` (LED strip) and `HueClient` (only when `hue_enabled` and not in the auto-off state)

Shared constants/types (`Color`, `PowerZone`, `POWER_ZONES`, pin assignments, `FIRMWARE_VERSION`, `DEFAULT_FTP`) live in `config.h` and are included everywhere.

## Hardware notes

- LED strip: WS2812B RGB, 300 LEDs, data pin `LED_PIN` = GPIO 28 (`config.h`)
- Display: Pimoroni Pico Display, SPI0, pins defined in `config.h` (`PIN_MISO/CS/SCK/MOSI/DC/BL`)
- Buttons: A/B/X/Y on GPIO 12–15, active-low with internal pull-ups
- Target BLE device name is hardcoded as `BLE_TARGET_NAME` in `config.h` — update this when pairing with a different trainer
