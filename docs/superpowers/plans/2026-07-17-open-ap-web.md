# Open Access Point Web Page Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the Wi-Fi scanner with an unsecured ESP32-C3 access point that returns static HTML at `http://192.168.4.1/`.

**Architecture:** One firmware source starts the ESP-IDF SoftAP network stack and its HTTP server. One GET route (`/`) writes a static HTML response from memory.

**Tech Stack:** ESP-IDF, `esp_wifi`, `esp_netif`, `esp_event`, `esp_http_server`, `nvs_flash`, Python `unittest`.

## Global Constraints

- The SSID is exactly `ESP32-Portal`.
- The access point has no password and uses `WIFI_AUTH_OPEN`.
- The documented URL is exactly `http://192.168.4.1/`.
- Only GET `/` is served; captive-portal DNS is not implemented.
- Initialization failures use `ESP_ERROR_CHECK`.

---

### Task 1: Establish source-level regression coverage

**Files:**
- Create: `tests/test_portal_source.py`
- Test: `tests/test_portal_source.py`

**Interfaces:**
- Consumes: text of `main/portal.c`.
- Produces: an executable regression check for the required portal contract.

- [ ] **Step 1: Write the failing test**

```python
class PortalSourceTests(unittest.TestCase):
    def test_configures_open_portal_and_root_html_handler(self):
        source = (ROOT / "main" / "portal.c").read_text(encoding="utf-8")
        self.assertIn('#define AP_SSID "ESP32-Portal"', source)
        self.assertIn('.authmode = WIFI_AUTH_OPEN', source)
        self.assertIn('.uri = "/"', source)
        self.assertIn('"text/html; charset=utf-8"', source)
```

- [ ] **Step 2: Run the test to verify it fails**

Run: `python3 -m unittest tests/test_portal_source.py -v`  
Expected: FAIL with `FileNotFoundError` because `main/portal.c` does not yet exist.

- [ ] **Step 3: Commit with the implementation after it passes**

```bash
git add tests/test_portal_source.py main/portal.c main/CMakeLists.txt README.md
git rm main/fast_scan.c
git commit -m "feat: serve web page from open access point"
```

### Task 2: Implement SoftAP and root HTTP page

**Files:**
- Create: `main/portal.c`
- Modify: `main/CMakeLists.txt`
- Delete: `main/fast_scan.c`

**Interfaces:**
- Consumes: `esp_netif_create_default_wifi_ap`, `esp_wifi_set_config`, `httpd_start`, and `httpd_register_uri_handler`.
- Produces: `void app_main(void)` that starts `ESP32-Portal` and serves GET `/`.

- [ ] **Step 1: Implement root handler and access-point constants**

```c
#define AP_SSID "ESP32-Portal"
#define AP_MAX_CONNECTIONS 4

static esp_err_t root_get_handler(httpd_req_t *request)
{
    static const char page[] = "<!doctype html><html><body>"
                               "<h1>ESP32 Portal</h1>"
                               "<p>Connected successfully.</p>"
                               "</body></html>";
    ESP_ERROR_CHECK(httpd_resp_set_type(request, "text/html; charset=utf-8"));
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}
```

- [ ] **Step 2: Start an AP using open authentication**

```c
wifi_config_t ap_config = {
    .ap = {
        .ssid = AP_SSID,
        .ssid_len = strlen(AP_SSID),
        .channel = 1,
        .authmode = WIFI_AUTH_OPEN,
        .max_connection = AP_MAX_CONNECTIONS,
    },
};
ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
ESP_ERROR_CHECK(esp_wifi_start());
```

- [ ] **Step 3: Start HTTP server and register `/`**

```c
httpd_handle_t server = NULL;
httpd_config_t config = HTTPD_DEFAULT_CONFIG();
ESP_ERROR_CHECK(httpd_start(&server, &config));
httpd_uri_t root_route = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler,
};
ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));
```

- [ ] **Step 4: Update CMake component dependencies**

```cmake
idf_component_register(SRCS "portal.c"
                    PRIV_REQUIRES esp_event esp_http_server esp_netif esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
```

- [ ] **Step 5: Run the regression test to verify it passes**

Run: `python3 -m unittest tests/test_portal_source.py -v`  
Expected: PASS with one test and zero failures.

### Task 3: Document and build

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: AP SSID and route contract.
- Produces: accurate build, connection, and browsing instructions.

- [ ] **Step 1: Replace scanner documentation with portal instructions**

Document `ESP32-Portal`, no password, and `http://192.168.4.1/`.

- [ ] **Step 2: Run host regression test**

Run: `python3 -m unittest tests/test_portal_source.py -v`  
Expected: PASS with one test and zero failures.

- [ ] **Step 3: Build firmware when ESP-IDF is available**

Run: `idf.py build`  
Expected: successful ESP-IDF build. If unavailable, record the missing command and do not claim a firmware build passed.

- [ ] **Step 4: Commit implementation**

```bash
git add main/portal.c main/CMakeLists.txt tests/test_portal_source.py README.md
git rm main/fast_scan.c
git commit -m "feat: serve web page from open access point"
```

## Self-review

- Spec coverage: Task 2 implements SSID, open auth, AP network, root route, content type, and failure handling; Task 3 documents URL and build.
- Placeholder scan: no TBD/TODO entries.
- Type consistency: `root_get_handler` matches `httpd_uri_t.handler`; `app_main` is the ESP-IDF entry point.

