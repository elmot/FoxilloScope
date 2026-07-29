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
void uart_prepare_for_flashing(void);

httpd_handle_t start_webserver(void);
esp_err_t ws_handler(httpd_req_t *req);
void nvs_save_wifi_creds(const char *ssid, const char *password);

void uart_write_str(const char *str);

extern bool ws_any_connected(void);
extern bool ble_any_connected(void);
void ble_disconnect_client(void);

extern EventGroupHandle_t s_wifi_event_group;

volatile extern bool s_sta_connected;

void register_http_static_resources(httpd_handle_t hd);

void start_dns_server(void);

void scheduleTxMessage(const char* payload, int len, bool isKey);

extern char s_sta_ssid[32];
extern char s_sta_ip[16];
extern char s_mac_suffix[8];
extern volatile int s_sta_rssi;

void ble_uart_init(void);
void ble_transmit(const char *text, int len);

void kick_out_ws_client();

[[noreturn]] void led_blink_pink_loop(void);

#endif //ESP32_GATEWAY_GATEWAY_H
