idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "."
                     PRIV_REQUIRES esp_driver_uart esp_https_server esp_driver_rmt nvs_flash esp_wifi esp_event esp_netif mdns
                     EMBED_FILES ../../html/index.html
                     ../../html/wifi.html)
