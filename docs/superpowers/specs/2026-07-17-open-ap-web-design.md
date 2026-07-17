# Open Access Point Web Page Design

## Goal

Convert the ESP32-C3 firmware from a Wi-Fi RSSI scanner into an open Wi-Fi access point that serves a web page at `http://192.168.4.1/`.

## User-facing behavior

- The device advertises the SSID `ESP32-Portal`.
- The hotspot has no password and uses `WIFI_AUTH_OPEN`.
- The access point uses the default ESP-IDF SoftAP network `192.168.4.1/24`.
- A connected client can request GET `/` at `http://192.168.4.1/` and receives a UTF-8 HTML success page.
- Serial logs identify the SSID and web address after startup.

## Architecture

`app_main` initializes NVS, ESP network interfaces, and the default event loop. It creates the default Wi-Fi AP interface, configures Wi-Fi for AP mode, applies the open SoftAP configuration, and starts it.

After Wi-Fi is started, the firmware starts ESP-IDF's built-in HTTP server and registers one GET handler for `/`. The handler sets `text/html; charset=utf-8` and writes a small static HTML document from firmware memory. No filesystem, DNS interception, captive-portal detection, station mode, or Wi-Fi scanning is included.

## Components

- `main/portal.c`: initialization, SoftAP configuration, HTTP server startup, and root-page handler.
- `main/CMakeLists.txt`: Wi-Fi, network interface, NVS, event loop, and HTTP server dependencies.
- `README.md`: connection and build instructions.
- `tests/test_portal_source.py`: host-runnable regression check for SSID, open authentication, route, and HTML content type.

## Error handling

Required ESP-IDF calls use `ESP_ERROR_CHECK`, so initialization failure stops with an explicit log. The root handler returns the error from `httpd_resp_send` when transmission fails.

## Verification

The source regression test must pass with system Python. Firmware must be built by `idf.py build` when ESP-IDF is available. At design time this workspace has no `idf.py` command on PATH; that limitation will be reported if it remains unavailable.

## Out of scope

- DNS hijacking and automatic captive-portal popups.
- Password protection, non-default DHCP ranges, and configuration UI.
- Continuing Wi-Fi RSSI scanning.
