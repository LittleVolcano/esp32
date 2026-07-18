/*
 * ESP32-C3 Wi-Fi scanner.
 * Scan all channels, sort access points by RSSI, and print the result.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"

static const char *TAG = "wifi_scan";
#define TARGET_SSID "ESP32-Portal"
#define SCAN_INTERVAL_MS 2000

static int compare_by_rssi(const void *left, const void *right)
{
    const wifi_ap_record_t *ap_left = left;
    const wifi_ap_record_t *ap_right = right;

    /* Stronger signal has a larger (less negative) RSSI value. */
    return (ap_right->rssi > ap_left->rssi) -
           (ap_right->rssi < ap_left->rssi);
}

static const char *authmode_name(wifi_auth_mode_t authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:         return "OPEN";
    case WIFI_AUTH_WEP:          return "WEP";
    case WIFI_AUTH_WPA_PSK:      return "WPA";
    case WIFI_AUTH_WPA2_PSK:     return "WPA2";
    case WIFI_AUTH_WPA_WPA2_PSK: return "WPA/WPA2";
    case WIFI_AUTH_WPA2_ENTERPRISE: return "WPA2-ENT";
    case WIFI_AUTH_WPA3_PSK:     return "WPA3";
    case WIFI_AUTH_WPA2_WPA3_PSK:return "WPA2/WPA3";
    case WIFI_AUTH_WAPI_PSK:     return "WAPI";
    default:                     return "UNKNOWN";
    }
}

static void scan_for_portal(void)
{
    wifi_scan_config_t scan_config = {
        .ssid = (uint8_t *)TARGET_SSID,
        .bssid = NULL,
        .channel = 0,              /* 0 means all channels */
        .show_hidden = false,
        .scan_type = WIFI_SCAN_TYPE_ACTIVE,
        .scan_time.active.min = 100,
        .scan_time.active.max = 300,
    };

    ESP_LOGI(TAG, "Scanning for %s...", TARGET_SSID);
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    if (ap_count == 0) {
        ESP_LOGI(TAG, "%s not found", TARGET_SSID);
        return;
    }

    wifi_ap_record_t *records = calloc(ap_count, sizeof(wifi_ap_record_t));
    if (records == NULL) {
        ESP_LOGE(TAG, "Not enough memory for scan results");
        return;
    }

    uint16_t record_count = ap_count;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&record_count, records));
    qsort(records, record_count, sizeof(wifi_ap_record_t), compare_by_rssi);

    bool found = false;
    for (uint16_t i = 0; i < record_count; ++i) {
        /* SSID is not necessarily NUL-terminated in the driver record. */
        char ssid[sizeof(records[i].ssid) + 1] = {0};
        memcpy(ssid, records[i].ssid, sizeof(records[i].ssid));
        if (strcmp(ssid, TARGET_SSID) == 0) {
            ESP_LOGI(TAG,
                     "Found %s: RSSI=%d dBm channel=%u auth=%s BSSID=%02X:%02X:%02X:%02X:%02X:%02X",
                     TARGET_SSID, records[i].rssi, records[i].primary,
                     authmode_name(records[i].authmode), records[i].bssid[0],
                     records[i].bssid[1], records[i].bssid[2], records[i].bssid[3],
                     records[i].bssid[4], records[i].bssid[5]);
            found = true;
        }
    }
    if (!found) {
        ESP_LOGI(TAG, "%s not found", TARGET_SSID);
    }

    free(records);
    ESP_ERROR_CHECK(esp_wifi_scan_stop());
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

    wifi_init_config_t init_config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&init_config));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    while (true) {
        scan_for_portal();
        vTaskDelay(pdMS_TO_TICKS(SCAN_INTERVAL_MS));
    }
}
