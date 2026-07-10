//
// Created by elmot on 09/07/2026.
//

#ifndef ESP32_GATEWAY_GATEWAY_H
#define ESP32_GATEWAY_GATEWAY_H
#include "freertos/FreeRTOS.h"
#include "esp_http_server.h"

#define WIFI_FAIL_BIT BIT1


extern void led_init(void);
extern void led_refresh(void);

void uart_init(void);
void uart_write_str(const char *str);
void broadcast_text(const char *text);

extern bool ws_any_connected(void);
extern EventGroupHandle_t s_wifi_event_group;

volatile extern bool s_sta_connected;

void register_http_static_resources(const httpd_handle_t hd);

#endif //ESP32_GATEWAY_GATEWAY_H
