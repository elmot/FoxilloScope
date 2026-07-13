idf_component_register(SRCS "main.c" "serial.c" "led.c" "web-resources.c" "dns_server.c" "ble_uart.c"
                    INCLUDE_DIRS "."
                     PRIV_REQUIRES esp_driver_uart esp_http_server nvs_flash esp_wifi esp_event esp_netif mdns esp_driver_rmt bt
        EMBED_FILES ../../html/index.html
                     ../../html/wifi.html
                     ../../html/uPlot.iife.min.js
                     ../../html/uPlot.min.css
                     ../../html/favicon.png
                     ../../docs/wiring.png
)
