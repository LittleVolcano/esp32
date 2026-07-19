/*
 * Minimal ESP32-C3 SoftAP test, matching the Arduino WiFi.softAP() example.
 */
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define AP_SSID "ESP_AP"
#define AP_PASSWORD "123456789"
#define AP_CHANNEL 1
#define AP_MAX_CONNECTIONS 4

static const char *TAG = "softap_test";

static esp_err_t root_get_handler(httpd_req_t *request)
{
    static const char page[] = "<!doctype html><html><body>"
                               "<h1>ESP32 SoftAP</h1>"
                               "<p>Connected successfully.</p>"
                               "</body></html>";

    ESP_ERROR_CHECK(httpd_resp_set_type(request, "text/html; charset=utf-8"));
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t favicon_get_handler(httpd_req_t *request)
{
    ESP_ERROR_CHECK(httpd_resp_set_status(request, "204 No Content"));
    return httpd_resp_send(request, NULL, 0);
}

void app_main(void)
{
    esp_err_t result = nvs_flash_init();
    if (result == ESP_ERR_NVS_NO_FREE_PAGES || result == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        result = nvs_flash_init();
    }
    ESP_ERROR_CHECK(result);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_ap() == NULL ? ESP_FAIL : ESP_OK);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    wifi_config_t ap_config = {0};
    memcpy(ap_config.ap.ssid, AP_SSID, strlen(AP_SSID));
    memcpy(ap_config.ap.password, AP_PASSWORD, strlen(AP_PASSWORD));
    ap_config.ap.ssid_len = strlen(AP_SSID);
    ap_config.ap.channel = AP_CHANNEL;
    ap_config.ap.authmode = WIFI_AUTH_WPA2_PSK;
    ap_config.ap.max_connection = AP_MAX_CONNECTIONS;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_handle_t server = NULL;
    httpd_config_t server_config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &server_config));

    httpd_uri_t root_route = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));

    httpd_uri_t favicon_route = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = favicon_get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &favicon_route));

    ESP_LOGI(TAG, "SoftAP started: SSID=%s channel=%d password=%s", AP_SSID, AP_CHANNEL,
             AP_PASSWORD);
    ESP_LOGI(TAG, "HTTP server ready at http://192.168.4.1/");
}
