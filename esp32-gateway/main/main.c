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
#include "mdns.h"

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

static char s_sta_ssid[32];
static char s_sta_password[64];
static char s_sta_ip[16];
static int s_sta_rssi;
static bool s_sta_connected;

static const char *NVS_NS = "wifi";

static char s_line_buf[LINE_BUF_SIZE];
static int s_line_pos = 0;
static char *s_frame_lines[MAX_FRAME_LINES];
static int s_frame_count = 0;

extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];
extern const uint8_t _binary_wifi_html_start[];
extern const uint8_t _binary_wifi_html_end[];

struct async_send_arg {
    int fd;
    char *data;
    int len;
};

static void ws_async_send(void* arg)
{
    struct async_send_arg* a = arg;
    if (httpd_ws_get_fd_info(s_server, a->fd) == HTTPD_WS_CLIENT_WEBSOCKET)
    {
        httpd_ws_frame_t pkt = {
            .payload = (uint8_t*)a->data,
            .len = a->len,
            .type = HTTPD_WS_TYPE_TEXT
        };
        esp_err_t err = httpd_ws_send_frame_async(s_server, a->fd, &pkt) != ESP_OK;
        if (err)
        {
            ESP_LOGW(TAG, "send fd=%d err=%s", a->fd, esp_err_to_name(err));
            xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
            for (int j = 0; j < MAX_WS_CLIENTS; j++)
            {
                if (s_ws_fds[j] == a->fd)
                {
                    s_ws_fds[j] = -1;
                    break;
                }
            }
            xSemaphoreGive(s_ws_mutex);
        }
    }
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
                esp_err_t err = httpd_queue_work(s_server, ws_async_send, a);
                if (err != ESP_OK)
                {
                    ESP_LOGW(TAG, "queue_work failed: %s", esp_err_to_name(err));
                    free(a->data);
                    free(a);
                }
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

static void nvs_save_wifi_creds(const char *ssid, const char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_str(h, "ssid", ssid);
        nvs_set_str(h, "password", password);
        nvs_commit(h);
        nvs_close(h);
    }
}

static void nvs_load_wifi_creds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_sta_ssid);
        if (nvs_get_str(h, "ssid", s_sta_ssid, &len) != ESP_OK) {
            strncpy(s_sta_ssid, CONFIG_ESP_WIFI_REMOTE_AP_SSID, sizeof(s_sta_ssid));
        }
        len = sizeof(s_sta_password);
        if (nvs_get_str(h, "password", s_sta_password, &len) != ESP_OK) {
            strncpy(s_sta_password, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD, sizeof(s_sta_password));
        }
        nvs_close(h);
    } else {
        strncpy(s_sta_ssid, CONFIG_ESP_WIFI_REMOTE_AP_SSID, sizeof(s_sta_ssid));
        strncpy(s_sta_password, CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD, sizeof(s_sta_password));
    }
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
        ESP_LOGI(TAG,
         "heap=%u min=%u internal=%u",
         esp_get_free_heap_size(),
         esp_get_minimum_free_heap_size(),
         heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
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

static esp_err_t wifi_config_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)_binary_wifi_html_start,
                    _binary_wifi_html_end - _binary_wifi_html_start);
    return ESP_OK;
}

static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    char buf[256];
    const char *status;
    if (s_sta_connected) {
        status = "connected";
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            s_sta_rssi = ap.rssi;
        }
    } else {
        status = "disconnected";
    }
    int n = snprintf(buf, sizeof(buf),
        "{\"sta\":{\"status\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}}",
        status, s_sta_ssid, s_sta_ip, s_sta_rssi);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, n);
    return ESP_OK;
}

static esp_err_t wifi_api_handler(httpd_req_t *req)
{
    char content[256];
    int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    char ssid[32] = {0};
    char password[64] = {0};
    char *p = content;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
        if (strncmp(p, "\"ssid\":", 7) == 0) {
            p += 7;
            while (*p && *p != '"') p++;
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(ssid) - 1) ssid[i++] = *p++;
            }
        } else if (strncmp(p, "\"password\":", 11) == 0) {
            p += 11;
            while (*p && *p != '"') p++;
            if (*p == '"') {
                p++;
                int i = 0;
                while (*p && *p != '"' && i < (int)sizeof(password) - 1) password[i++] = *p++;
            }
        } else {
            p++;
        }
    }

    if (ssid[0] == '\0') {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    nvs_save_wifi_creds(ssid, password);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"ok\":true}", 0);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
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
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/wifi", .method = HTTP_GET, .handler = wifi_config_handler
        });
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/api/wifi/status", .method = HTTP_GET, .handler = wifi_status_handler
        });
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/api/wifi", .method = HTTP_POST, .handler = wifi_api_handler
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
        s_sta_connected = false;
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_sta_connected = true;
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        esp_wifi_sta_get_rssi(&s_sta_rssi);
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        wifi_event_sta_disconnected_t *d = data;
        s_sta_connected = false;
        s_sta_ip[0] = '\0';
        ESP_LOGI(TAG, "STA disconnected, reason=%d", d->reason);
        if (d->reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
            d->reason == WIFI_REASON_MIC_FAILURE ||
            d->reason == WIFI_REASON_HANDSHAKE_TIMEOUT ||
            d->reason == WIFI_REASON_AUTH_EXPIRE ||
            d->reason == WIFI_REASON_NO_AP_FOUND) {
            ESP_LOGW(TAG, "Fatal disconnect, stopping auto-reconnect");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        } else if (s_retry_num < CONFIG_ESP_MAXIMUM_STA_RETRY) {
            s_retry_num++;
            int delay_ms = 15000 * (1 << (s_retry_num - 1));
            if (delay_ms > 300000) delay_ms = 300000;
            ESP_LOGI(TAG, "Reconnect in %d ms (attempt %d/%d)",
                     delay_ms, s_retry_num, CONFIG_ESP_MAXIMUM_STA_RETRY);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
            esp_wifi_connect();
        } else {
            ESP_LOGW(TAG, "Exhausted STA retries");
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
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        }
    };
    strncpy((char *)sta_cfg.sta.ssid, s_sta_ssid, sizeof(sta_cfg.sta.ssid));
    strncpy((char *)sta_cfg.sta.password, s_sta_password, sizeof(sta_cfg.sta.password));

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

    nvs_load_wifi_creds();

    wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    wifi_init_apsta();

    xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE, 10000 / portTICK_PERIOD_MS);

    uart_init();
    s_server = start_webserver();

    ESP_ERROR_CHECK(mdns_init());
    const char *hostname = CONFIG_LWIP_LOCAL_HOSTNAME;
    const size_t hlen = strlen(hostname);
    if (hlen > 6 && strcmp(hostname + hlen - 6, ".local") == 0) {
        char *trimmed = strndup(hostname, hlen - 6);
        ESP_ERROR_CHECK(mdns_hostname_set(trimmed));
        free(trimmed);
    } else {
        ESP_ERROR_CHECK(mdns_hostname_set(hostname));
    }
    ESP_ERROR_CHECK(mdns_instance_name_set("ESP32 Elmot Oscilloscope(" CONFIG_LWIP_LOCAL_HOSTNAME ")"));
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str), MACSTR, MAC2STR(mac));
    mdns_service_txt_item_set("_http", "_tcp", "mac", mac_str);
    mdns_service_txt_item_set("_http", "_tcp", "model", "v1.0");
    ESP_LOGI(TAG, "mDNS advertising as " CONFIG_LWIP_LOCAL_HOSTNAME);
}
