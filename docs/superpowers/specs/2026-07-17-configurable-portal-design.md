# Configurable SoftAP Portal Design

## Goal

Expose the SoftAP Wi-Fi name and AP IP address in ESP-IDF's SDK Configuration Editor, so a firmware build can choose the hotspot name and browser address without editing C source.

## Configuration

The project provides a Portal Configuration menu with two build-time values:

- CONFIG_PORTAL_WIFI_SSID: string, default ESP32-Portal.
- CONFIG_PORTAL_AP_IP_ADDRESS: IPv4 string, default 192.168.4.1.

The SSID remains an open network; no password configuration is added. The address always maps to the root page, so users browse http://<AP IP>/.

## Runtime behavior

The firmware validates the SSID is no more than 32 bytes and parses the configured IPv4 address before starting Wi-Fi. It stops the default AP DHCP server, sets the AP IP and gateway to the configured address with a /24 netmask, then restarts DHCP. Invalid input causes an explicit startup failure through ESP_ERROR_CHECK.

The root handler and route stay unchanged. The startup log uses the configured SSID and IP address.

## Files

- main/Kconfig.projbuild: replace stale scanner settings with the two portal settings.
- main/portal.c: consume Kconfig macros, configure the AP netif, and validate configuration.
- tests/test_portal_source.py: check Kconfig declarations and source consumption.
- README.md: document where to edit values and the rebuild/flash requirement.

## Verification

A source-level Python regression test covers configuration presence and use. idf.py build is run when the ESP-IDF CLI is available; this workspace currently does not provide it on PATH.

