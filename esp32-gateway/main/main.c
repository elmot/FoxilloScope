#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "esp_http_server.h"
#include "driver/uart.h"
#include "soc/gpio_num.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

#define UART_PORT UART_NUM_1
#define UART_BAUD 460800
#define UART_TX_PIN GPIO_NUM_6
#define UART_RX_PIN GPIO_NUM_7
#define UART_BUF_SIZE 256

#define LINE_BUF_SIZE 10240
#define MAX_FRAME_LINES 128
#define MAX_WS_CLIENTS 8

static const char *TAG = "gateway";

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static httpd_handle_t s_server = NULL;
static QueueHandle_t s_uart_queue = NULL;

static int s_ws_fds[MAX_WS_CLIENTS];
static SemaphoreHandle_t s_ws_mutex;

static char s_line_buf[LINE_BUF_SIZE];
static int s_line_pos = 0;
static char *s_frame_lines[MAX_FRAME_LINES];
static int s_frame_count = 0;

extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];

struct async_send_arg {
    int fd;
    char *data;
    int len;
};

static void ws_async_send(void *arg)
{
    struct async_send_arg *a = arg;
    httpd_ws_frame_t pkt = {
        .payload = (uint8_t *)a->data,
        .len = a->len,
        .type = HTTPD_WS_TYPE_TEXT
    };
    httpd_ws_send_frame_async(s_server, a->fd, &pkt);
    free(a->data);
    free(a);
}

static void broadcast_text(const char *text)
{
    int len = strlen(text);
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    for (int i = 0; i < MAX_WS_CLIENTS; i++) {
        if (s_ws_fds[i] >= 0) {
            struct async_send_arg *a = malloc(sizeof(*a));
            if (a) {
                a->fd = s_ws_fds[i];
                a->data = strdup(text);
                a->len = len;
                httpd_queue_work(s_server, ws_async_send, a);
            }
        }
    }
    xSemaphoreGive(s_ws_mutex);
}

static void broadcast_error(const char *msg)
{
    char buf[128];
    int n = snprintf(buf, sizeof(buf), "error: %s", msg);
    if (n > 0) broadcast_text(buf);
}

static void flush_frame(void)
{
    if (s_frame_count == 0) return;
    int total = 0;
    for (int i = 0; i < s_frame_count; i++) {
        total += strlen(s_frame_lines[i]) + 1;
    }
    char *block = malloc(total + 1);
    if (!block) return;
    int pos = 0;
    for (int i = 0; i < s_frame_count; i++) {
        int l = strlen(s_frame_lines[i]);
        memcpy(block + pos, s_frame_lines[i], l);
        pos += l;
        block[pos++] = '\n';
        free(s_frame_lines[i]);
        s_frame_lines[i] = NULL;
    }
    block[pos] = '\0';
    s_frame_count = 0;
    broadcast_text(block);
    free(block);
}

static void uart_event_task(void *arg)
{
    uart_event_t event;
    char *data = malloc(UART_BUF_SIZE);
    if (!data) { vTaskDelete(NULL); return; }

    for (;;) {
        if (xQueueReceive(s_uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA:
                uart_read_bytes(UART_PORT, data, event.size, 0);
                for (int i = 0; i < event.size; i++) {
                    char c = data[i];
                    if (c == '\n') {
                        s_line_buf[s_line_pos] = '\0';
                        if (strcmp(s_line_buf, "[frame]") == 0) {
                            flush_frame();
                        } else if (s_line_buf[0] != '\0') {
                            if (s_frame_count < MAX_FRAME_LINES) {
                                s_frame_lines[s_frame_count] = strdup(s_line_buf);
                                if (s_frame_lines[s_frame_count]) s_frame_count++;
                            }
                        }
                        s_line_pos = 0;
                    } else if (c != '\r') {
                        if (s_line_pos < LINE_BUF_SIZE - 1) s_line_buf[s_line_pos++] = c;
                    }
                }
                break;
            case UART_FRAME_ERR:
                broadcast_error("UART frame error");
                break;
            case UART_FIFO_OVF:
                uart_flush_input(UART_PORT);
                break;
            default:
                break;
            }
        }
    }
    free(data);
    vTaskDelete(NULL);
}

static void uart_init(void)
{
    uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE * 2, 0, 20, &s_uart_queue, 0));
    xTaskCreate(uart_event_task, "uart_evt", 4096, NULL, 10, NULL);
}

static void uart_write_str(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        int fd = httpd_req_to_sockfd(req);
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        for (int i = 0; i < MAX_WS_CLIENTS; i++) {
            if (s_ws_fds[i] < 0) { s_ws_fds[i] = fd; break; }
        }
        xSemaphoreGive(s_ws_mutex);
        ESP_LOGI(TAG, "WS connected fd=%d", fd);
        return ESP_OK;
    }
    httpd_ws_frame_t pkt;
    uint8_t *buf = NULL;
    memset(&pkt, 0, sizeof(pkt));
    pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &pkt, 0);
    if (ret != ESP_OK) return ret;
    if (pkt.len) {
        buf = calloc(1, pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &pkt, pkt.len);
        if (ret != ESP_OK) { free(buf); return ret; }
        uart_write_str((const char *)buf);
        uart_write_str("\n");
    }
    free(buf);
    return ESP_OK;
}

static esp_err_t index_handler(httpd_req_t *req)
{
    size_t len = _binary_index_html_end - _binary_index_html_start;
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)_binary_index_html_start, len);
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    httpd_handle_t hd = NULL;
    if (httpd_start(&hd, &cfg) == ESP_OK) {
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/", .method = HTTP_GET, .handler = index_handler
        });
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true
        });
        ESP_LOGI(TAG, "Web server started on port %d", cfg.server_port);
    }
    return hd;
}

static void wifi_event_handler(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < CONFIG_ESP_MAXIMUM_STA_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
}

static void wifi_init_apsta(void)
{
    esp_netif_create_default_wifi_ap();
    esp_netif_create_default_wifi_sta();

    wifi_config_t ap_cfg = {
        .ap = {
            .ssid = CONFIG_ESP_WIFI_AP_SSID,
            .ssid_len = 0,
            .channel = CONFIG_ESP_WIFI_AP_CHANNEL,
            .password = CONFIG_ESP_WIFI_AP_PASSWORD,
            .max_connection = CONFIG_ESP_MAX_STA_CONN_AP,
            .authmode = strlen(CONFIG_ESP_WIFI_AP_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
        }
    };
    wifi_config_t sta_cfg = {
        .sta = {
            .ssid = CONFIG_ESP_WIFI_REMOTE_AP_SSID,
            .password = CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        }
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP SSID: %s", CONFIG_ESP_WIFI_AP_SSID);
    ESP_LOGI(TAG, "STA connecting to: %s", CONFIG_ESP_WIFI_REMOTE_AP_SSID);
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

    s_wifi_event_group = xEventGroupCreate();
    s_ws_mutex = xSemaphoreCreateMutex();
    for (int i = 0; i < MAX_WS_CLIENTS; i++) s_ws_fds[i] = -1;

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    wifi_init_apsta();

    xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, 10000 / portTICK_PERIOD_MS);

    uart_init();
    s_server = start_webserver();
}
