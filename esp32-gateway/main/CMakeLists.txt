idf_component_register(SRCS "main.c" "serial.c" "led.c"
                    INCLUDE_DIRS "."
                     PRIV_REQUIRES esp_driver_uart esp_http_server nvs_flash esp_wifi esp_event esp_netif mdns esp_driver_rmt
        EMBED_FILES ../../html/index.html
                     ../../html/wifi.html)
