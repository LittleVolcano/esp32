# Configurable SoftAP Portal Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox syntax for tracking.

**Goal:** Make the SoftAP SSID and IP address editable from the ESP-IDF SDK Configuration Editor.

**Architecture:** Kconfig supplies SSID and IPv4 string macros. Firmware validates them, applies the IPv4 address to the default AP netif and DHCP server, and uses the values in Wi-Fi setup and startup log.

**Tech Stack:** ESP-IDF Kconfig, esp_netif, lwIP IPv4 parsing, Python unittest.

## Global Constraints

- CONFIG_PORTAL_WIFI_SSID defaults to ESP32-Portal.
- CONFIG_PORTAL_AP_IP_ADDRESS defaults to 192.168.4.1.
- The hotspot remains open with WIFI_AUTH_OPEN.
- The portal is available at http://<CONFIG_PORTAL_AP_IP_ADDRESS>/.
- Invalid SSID and IP input fails before Wi-Fi startup.
- No password, DNS interception, or extra HTTP route is added.

---

### Task 1: Add a failing configuration contract test

**Files:**
- Modify: tests/test_portal_source.py

**Interfaces:**
- Consumes: main/Kconfig.projbuild and main/portal.c.
- Produces: a test that requires Kconfig defaults and source use of both settings.

- [ ] **Step 1: Write the failing test**

~~~python
def test_uses_sdkconfig_for_ssid_and_ap_address(self):
    kconfig = (ROOT / "main" / "Kconfig.projbuild").read_text(encoding="utf-8")
    source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
    self.assertIn('config PORTAL_WIFI_SSID', kconfig)
    self.assertIn('default "ESP32-Portal"', kconfig)
    self.assertIn('config PORTAL_AP_IP_ADDRESS', kconfig)
    self.assertIn('default "192.168.4.1"', kconfig)
    self.assertIn('CONFIG_PORTAL_WIFI_SSID', source)
    self.assertIn('CONFIG_PORTAL_AP_IP_ADDRESS', source)
~~~

- [ ] **Step 2: Verify RED**

Run: python3 -m unittest tests/test_portal_source.py -v.
Expected: FAIL because the current source hard-codes values and Kconfig contains scanner settings.

### Task 2: Implement Kconfig-driven SoftAP configuration

**Files:**
- Modify: main/Kconfig.projbuild
- Modify: main/portal.c
- Test: tests/test_portal_source.py

**Interfaces:**
- Consumes: CONFIG_PORTAL_WIFI_SSID and CONFIG_PORTAL_AP_IP_ADDRESS.
- Produces: app_main that starts the configured open AP and DHCP network.

- [ ] **Step 1: Replace the Kconfig menu**

~~~kconfig
menu "Portal Configuration"

    config PORTAL_WIFI_SSID
        string "Wi-Fi name"
        default "ESP32-Portal"

    config PORTAL_AP_IP_ADDRESS
        string "AP IP address"
        default "192.168.4.1"

endmenu
~~~

- [ ] **Step 2: Apply values to Wi-Fi and AP network**

~~~c
esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
esp_netif_ip_info_t ip_info = {0};
ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
~~~

Parse CONFIG_PORTAL_AP_IP_ADDRESS with lwIP before esp_netif_set_ip_info; set gateway equal to the configured address and netmask to 255.255.255.0. Reject SSIDs longer than 32 bytes and invalid IP strings with ESP_ERROR_CHECK.

- [ ] **Step 3: Verify GREEN**

Run: python3 -m unittest tests/test_portal_source.py -v.
Expected: PASS with all tests green.

### Task 3: Document configuration and validate

**Files:**
- Modify: README.md

**Interfaces:**
- Consumes: configured SSID and IP behavior.
- Produces: instructions for VS Code's SDK Configuration Editor.

- [ ] **Step 1: Document editor path and rebuild requirement**

State that the user changes Portal Configuration → Wi-Fi name and Portal Configuration → AP IP address, then builds and flashes firmware. State the browser URL derives from the configured IP.

- [ ] **Step 2: Run regression and build checks**

Run: python3 -m unittest tests/test_portal_source.py -v.
Expected: PASS.

Run: idf.py build.
Expected: successful build when ESP-IDF is available; otherwise record the unavailable command without claiming a build passed.

- [ ] **Step 3: Commit**

~~~bash
git add main/Kconfig.projbuild main/portal.c tests/test_portal_source.py README.md
git commit -m "feat: configure portal network in sdkconfig"
~~~

## Self-review

- Kconfig defaults, source use, runtime validation, DHCP configuration, and user instructions each have a task.
- No placeholder or extra portal behavior is included.

