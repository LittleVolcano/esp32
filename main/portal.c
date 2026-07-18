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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

#define AP_SSID CONFIG_PORTAL_WIFI_SSID
#define AP_IP_ADDRESS CONFIG_PORTAL_AP_IP_ADDRESS
#define AP_MAX_CONNECTIONS 4
#define AP_CHANNEL 6
#define AP_BEACON_INTERVAL_MS 100
#define AP_STATUS_INTERVAL_MS 5000
#define AP_MAX_TX_POWER_QUARTER_DBM 80

static const char *TAG = "portal";

static void log_ap_status(const char *reason)
{
    wifi_mode_t mode = WIFI_MODE_NULL;
    wifi_config_t active_config = {0};
    wifi_sta_list_t stations = {0};
    int8_t tx_power = 0;
    char ssid[sizeof(active_config.ap.ssid) + 1] = {0};

    const esp_err_t mode_result = esp_wifi_get_mode(&mode);
    const esp_err_t config_result = esp_wifi_get_config(WIFI_IF_AP, &active_config);
    const esp_err_t stations_result = esp_wifi_ap_get_sta_list(&stations);
    const esp_err_t power_result = esp_wifi_get_max_tx_power(&tx_power);
    if (mode_result != ESP_OK || config_result != ESP_OK ||
        stations_result != ESP_OK || power_result != ESP_OK) {
        ESP_LOGW(TAG, "%s: AP status query failed mode=%s config=%s stations=%s power=%s",
                 reason, esp_err_to_name(mode_result), esp_err_to_name(config_result),
                 esp_err_to_name(stations_result), esp_err_to_name(power_result));
        return;
    }

    memcpy(ssid, active_config.ap.ssid, sizeof(active_config.ap.ssid));
    ESP_LOGI(TAG,
             "%s: mode=%d ssid=%s channel=%u hidden=%u beacon=%u ms clients=%u tx=%d.%02d dBm",
             reason, mode, ssid, active_config.ap.channel, active_config.ap.ssid_hidden,
             active_config.ap.beacon_interval, stations.num, tx_power / 4,
             (tx_power % 4) * 25);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base != WIFI_EVENT) {
        return;
    }

    switch (event_id) {
    case WIFI_EVENT_AP_START:
        ESP_LOGI(TAG, "WIFI_EVENT_AP_START");
        log_ap_status("AP started");
        break;
    case WIFI_EVENT_AP_STOP:
        ESP_LOGE(TAG, "WIFI_EVENT_AP_STOP");
        break;
    case WIFI_EVENT_AP_STACONNECTED: {
        const wifi_event_ap_staconnected_t *event = event_data;
        ESP_LOGI(TAG, "station connected: %02X:%02X:%02X:%02X:%02X:%02X, aid=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3],
                 event->mac[4], event->mac[5], event->aid);
        break;
    }
    case WIFI_EVENT_AP_STADISCONNECTED: {
        const wifi_event_ap_stadisconnected_t *event = event_data;
        ESP_LOGI(TAG, "station disconnected: %02X:%02X:%02X:%02X:%02X:%02X, aid=%d",
                 event->mac[0], event->mac[1], event->mac[2], event->mac[3],
                 event->mac[4], event->mac[5], event->aid);
        break;
    }
    default:
        break;
    }
}

static void ap_status_task(void *arg)
{
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(AP_STATUS_INTERVAL_MS));
        log_ap_status("AP heartbeat");
    }
}

static void configure_ap_network(esp_netif_t *ap_netif)
{
    esp_netif_ip_info_t ip_info = {0};
    ip4_addr_t parsed_ip = {0};
    ip4_addr_t netmask = {0};

    ESP_ERROR_CHECK(ap_netif == NULL ? ESP_ERR_NO_MEM : ESP_OK);
    ESP_ERROR_CHECK(ip4addr_aton(AP_IP_ADDRESS, &parsed_ip) ? ESP_OK : ESP_ERR_INVALID_ARG);
    ip_info.ip.addr = parsed_ip.addr;
    ip_info.gw.addr = parsed_ip.addr;
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    ip_info.netmask.addr = netmask.addr;

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
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                         &wifi_event_handler, NULL, NULL));

    wifi_config_t ap_config = {0};
    size_t ap_ssid_len = strlen(AP_SSID);
    ESP_ERROR_CHECK(ap_ssid_len <= sizeof(ap_config.ap.ssid) ? ESP_OK : ESP_ERR_INVALID_ARG);

    memcpy(ap_config.ap.ssid, AP_SSID, ap_ssid_len);
    ap_config.ap.ssid_len = ap_ssid_len;
    ap_config.ap.channel = AP_CHANNEL;
    ap_config.ap.authmode = WIFI_AUTH_OPEN;
    ap_config.ap.ssid_hidden = 0;
    ap_config.ap.max_connection = AP_MAX_CONNECTIONS;
    ap_config.ap.beacon_interval = AP_BEACON_INTERVAL_MS;
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_max_tx_power(AP_MAX_TX_POWER_QUARTER_DBM));
    log_ap_status("AP configured");

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    ESP_ERROR_CHECK(httpd_start(&server, &config));

    httpd_uri_t root_route = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = root_get_handler,
    };
    ESP_ERROR_CHECK(httpd_register_uri_handler(server, &root_route));
    ESP_LOGI(TAG, "SoftAP %s ready at http://%s/", AP_SSID, AP_IP_ADDRESS);
    xTaskCreate(ap_status_task, "ap_status", 3072, NULL, 4, NULL);
}
