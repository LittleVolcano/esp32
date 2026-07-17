idf_component_register(SRCS "portal.c"
                    PRIV_REQUIRES esp_event esp_http_server esp_netif esp_wifi nvs_flash
                    INCLUDE_DIRS ".")
