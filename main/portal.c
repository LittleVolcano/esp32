/*
 * ESP32 SoftAP with a minimal HTTP portal page.
 */
#include <string.h>

#include "lwip/ip4_addr.h"

#include "esp_err.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define AP_SSID CONFIG_PORTAL_WIFI_SSID
#define AP_IP_ADDRESS CONFIG_PORTAL_AP_IP_ADDRESS
#define AP_MAX_CONNECTIONS 4

static const char *TAG = "portal";

static void configure_ap_network(esp_netif_t *ap_netif)
{
    esp_netif_ip_info_t ip_info = {0};

    ESP_ERROR_CHECK(ap_netif == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(ip4addr_aton(AP_IP_ADDRESS, &ip_info.ip) ? ESP_OK : ESP_ERR_INVALID_ARG);
    ip_info.gw = ip_info.ip;
    IP4_ADDR(&ip_info.netmask, 255, 255, 255, 0);

    ESP_ERROR_CHECK(esp_netif_dhcps_stop(ap_netif));
    ESP_ERROR_CHECK(esp_netif_set_ip_info(ap_netif, &ip_info));
    ESP_ERROR_CHECK(esp_netif_dhcps_start(ap_netif));
}

static esp_err_t root_get_handler(httpd_req_t *request)
{
    static const char page[] = "<!doctype html><html><body>"
                               "<h1>ESP32 Portal</h1>"
                               "<p>Connected successfully.</p>"
                               "</body></html>";
    ESP_ERROR_CHECK(httpd_resp_set_type(request, "text/html; charset=utf-8"));
    return httpd_resp_send(request, page, HTTPD_RESP_USE_STRLEN);
}

void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    configure_ap_network(ap_netif);

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));

    wifi_config_t ap_config = {0};
    size_t ap_ssid_len = strlen(AP_SSID);
    ESP_ERROR_CHECK(ap_ssid_len <= sizeof(ap_config.ap.ssid) ? ESP_OK : ESP_ERR_INVALID_ARG);

    memcpy(ap_config.ap.ssid, AP_SSID, ap_ssid_len);
    ap_config.ap.ssid_len = ap_ssid_len;
    ap_config.ap.channel = 1;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.max_connection = AP_MAX_CONNECTIONS;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root_route = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));
    ESP_LOGI(TAG, "SoftAP " AP_SSID " ready at http://" AP_IP_ADDRESS "/");
}
