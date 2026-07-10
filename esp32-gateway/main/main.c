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
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include "mdns.h"
#include "gateway.h"
#include "cJSON.h"

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define STA_CONNECT_TIMEOUT_MS  10000

static const char *TAG = "gateway";

static TaskHandle_t s_reconnect_task = NULL;
static SemaphoreHandle_t s_reconnect_sem = NULL;
static volatile int s_reconnect_delay;

EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;
static httpd_handle_t s_server = NULL;

static SemaphoreHandle_t s_ws_mutex;

static char s_sta_ssid[32];
static char s_sta_password[64];
static char s_sta_ip[16];
static volatile int s_sta_rssi;
volatile bool s_sta_connected;

static const char *NVS_NS = "wifi";

volatile int currentClientId = 0;
volatile int currentClientFd = -1;
typedef struct {
    int fd;
    char *data;
    int len;
    int clentId;
} async_send_arg_t;

static void ws_async_send(void* arg)
{
    async_send_arg_t* a = arg;
    if (currentClientId == a->clentId && currentClientFd >=0)
    {
        httpd_ws_frame_t pkt = {
            .payload = (uint8_t*)a->data,
            .len = a->len,
            .type = HTTPD_WS_TYPE_TEXT
        };
        const esp_err_t err = httpd_ws_send_frame_async(s_server, a->fd, &pkt);
        if (err != ESP_OK)
        {
            ESP_LOGW(TAG, "send fd=%d err=%s", a->fd, esp_err_to_name(err));
            xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
            currentClientFd = -1;
            xSemaphoreGive(s_ws_mutex);
            led_refresh();
        }
    }
    free(a->data);
    free(a);
}

void broadcast_text(const char *text)
{
    int len = strlen(text);
    xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
    if (currentClientFd >= 0)
    {
        async_send_arg_t* a = malloc(sizeof(*a));
        if (a)
        {
            a->fd = currentClientFd;
            a->clentId = currentClientId;
            a->data = strdup(text);
            a->len = len;
        }
        if (a && a->data)
        {
            const esp_err_t err = httpd_queue_work(s_server, ws_async_send, a);
            if (err != ESP_OK)
            {
                ESP_LOGW(TAG, "queue_work failed: %s", esp_err_to_name(err));
                free(a->data);
                free(a);
            }
        } else
        {
            ESP_LOGE(TAG, "Out Of Memory!!!");
        }
    }
    xSemaphoreGive(s_ws_mutex);
}

static void nvs_save_wifi_creds(const char *ssid, const char *password)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) {
        ESP_LOGE(TAG, "nvs_open failed while saving wifi creds");
        return;
    }
    esp_err_t err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "password", password);
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) ESP_LOGE(TAG, "nvs write failed: %s", esp_err_to_name(err));
    nvs_close(h);
}

static void nvs_load_wifi_creds(void)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) == ESP_OK) {
        size_t len = sizeof(s_sta_ssid);
        if (nvs_get_str(h, "ssid", s_sta_ssid, &len) != ESP_OK) {
            snprintf(s_sta_ssid, sizeof(s_sta_ssid), "%s", CONFIG_ESP_WIFI_REMOTE_AP_SSID);
        }
        len = sizeof(s_sta_password);
        if (nvs_get_str(h, "password", s_sta_password, &len) != ESP_OK) {
            snprintf(s_sta_password, sizeof(s_sta_password), "%s", CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
        }
        nvs_close(h);
    } else {
        snprintf(s_sta_ssid, sizeof(s_sta_ssid), "%s", CONFIG_ESP_WIFI_REMOTE_AP_SSID);
        snprintf(s_sta_password, sizeof(s_sta_password), "%s", CONFIG_ESP_WIFI_REMOTE_AP_PASSWORD);
    }
}

static void close_ws([[maybe_unused]] esp_err_t err, const int socket,[[maybe_unused]]  void *arg)
{
    httpd_sess_trigger_close(s_server, socket);
}

static void close_msg_ws(void *arg)
{
    // keep this in ROM memory
    static constexpr char message[] = "error: Another browser has taken over the session.";
    static const httpd_ws_frame_t frame = {
        .final = true,
        .fragmented = false,
        .type = HTTPD_WS_TYPE_TEXT,
        .payload = (uint8_t*)&message[0],
        .len = sizeof(message) - 1
    };
    const int fd = (int)arg;
    // The cast ditches *const* qualifier, it  is safe because httpd_ws_send_data_async does not modify the data
    const esp_err_t res = httpd_ws_send_data_async(s_server, fd, (httpd_ws_frame_t*)&frame, close_ws, nullptr);
    if (res!= ESP_OK)
    {
        ESP_LOGW(TAG, "Closing message fail: %d", res);
    }
}

static esp_err_t ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        const int newFd = httpd_req_to_sockfd(req);
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        if (currentClientFd >= 0)
        {
            httpd_queue_work(s_server, close_msg_ws, (void*)currentClientFd);
        }
        currentClientId++;
        currentClientFd = newFd;
        xSemaphoreGive(s_ws_mutex);
        ESP_LOGI(TAG, "WS connected fd=%d", currentClientFd);
        led_refresh();
        ESP_LOGI(TAG,
         "heap=%u min=%u internal=%u",
         esp_get_free_heap_size(),
         esp_get_minimum_free_heap_size(),
         heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
        return ESP_OK;
    }
    httpd_ws_frame_t pkt={0};
    uint8_t *buf = NULL;
    pkt.type = HTTPD_WS_TYPE_TEXT;
    esp_err_t ret = httpd_ws_recv_frame(req, &pkt, 0);
    if (ret != ESP_OK) return ret;
    if (pkt.type == HTTPD_WS_TYPE_CLOSE) {
        xSemaphoreTake(s_ws_mutex, portMAX_DELAY);
        currentClientFd = -1;
        led_refresh();
        xSemaphoreGive(s_ws_mutex);
        return ESP_OK;
    }
    if (pkt.len) {
        buf = calloc(1, pkt.len + 1);
        if (!buf) return ESP_ERR_NO_MEM;
        pkt.payload = buf;
        ret = httpd_ws_recv_frame(req, &pkt, pkt.len);
        if (ret != ESP_OK) { free(buf); return ret; }
        uart_write_str((const char *)buf);
        uart_write_str("\n");
        free(buf);
    }
    return ESP_OK;
}

static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    static char buf[1024];
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
    const int n = snprintf(buf, sizeof(buf),
        "{\"sta\":{\"status\":\"%s\",\"ssid\":\"%s\",\"ip\":\"%s\",\"rssi\":%d}}",
        status, s_sta_ssid, s_sta_ip, s_sta_rssi);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, buf, n);
    return ESP_OK;
}

// ReSharper disable once CppDFAConstantFunctionResult
static esp_err_t wifi_api_handler(httpd_req_t *req)
{
    char content[256];
    const int ret = httpd_req_recv(req, content, sizeof(content) - 1);
    if (ret <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No data");
        return ESP_FAIL;
    }
    content[ret] = '\0';

    cJSON *json = cJSON_Parse(content);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    char ssid[32] = {0};
    char password[64] = {0};
    snprintf(ssid, sizeof(ssid), "%s", ssid_item->valuestring);

    cJSON *pwd_item = cJSON_GetObjectItem(json, "password");
    if (cJSON_IsString(pwd_item)) {
        snprintf(password, sizeof(password), "%s", pwd_item->valuestring);
    }

    cJSON_Delete(json);

    nvs_save_wifi_creds(ssid, password);

    httpd_resp_set_type(req, "application/json");
    static constexpr char response_json[] = "{\"ok\":true}";
    httpd_resp_send(req, response_json, sizeof(response_json) -1);
    fflush(stdout);
    vTaskDelay(pdMS_TO_TICKS(500));
    esp_restart();
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 20;
    httpd_handle_t hd = NULL;

    if (httpd_start(&hd, &cfg) == ESP_OK) {
        register_http_static_resources(hd);
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
            .uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true
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

bool ws_any_connected()
{
    return currentClientFd >=0;
}

static void wifi_event_handler([[maybe_unused]] void *arg,const esp_event_base_t base,const  int32_t id,void * data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STACONNECTED) {
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_AP_STADISCONNECTED) {
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        s_sta_connected = false;
        led_refresh();
        esp_wifi_connect();
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *e = data;
        ESP_LOGI(TAG, "STA got IP: " IPSTR, IP2STR(&e->ip_info.ip));
        s_sta_connected = true;
        snprintf(s_sta_ip, sizeof(s_sta_ip), IPSTR, IP2STR(&e->ip_info.ip));
        int rssi;
        esp_wifi_sta_get_rssi(&rssi);
        s_sta_rssi = rssi;
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        const wifi_event_sta_disconnected_t *d = data;
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
            led_refresh();
            s_reconnect_delay = delay_ms;
            xSemaphoreGive(s_reconnect_sem);
        } else {
            ESP_LOGW(TAG, "Exhausted STA retries");
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
    }
    led_refresh();
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
            // ReSharper disable once CppDFAUnreachableCode
            .authmode = strlen(CONFIG_ESP_WIFI_AP_PASSWORD) ? WIFI_AUTH_WPA2_PSK : WIFI_AUTH_OPEN,
        }
    };
    wifi_config_t sta_cfg = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        }
    };
    snprintf((char *)sta_cfg.sta.ssid, sizeof(sta_cfg.sta.ssid), "%s", s_sta_ssid);
    snprintf((char *)sta_cfg.sta.password, sizeof(sta_cfg.sta.password), "%s", s_sta_password);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &ap_cfg));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "AP SSID: %s", CONFIG_ESP_WIFI_AP_SSID);
    ESP_LOGI(TAG, "STA connecting to: %s", CONFIG_ESP_WIFI_REMOTE_AP_SSID);
}

static void reconnect_task(void *arg)
{
    for (;;) {
        xSemaphoreTake(s_reconnect_sem, portMAX_DELAY);
        while (xSemaphoreTake(s_reconnect_sem, 0) == pdTRUE) {}
        vTaskDelay(pdMS_TO_TICKS(s_reconnect_delay));
        esp_wifi_connect();
    }
}

[[noreturn]] [[maybe_unused]] static void tx_power_task([[maybe_unused]] void *arg)
{
    int8_t last_power = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        if (!s_sta_connected) continue;
        int rssi;
        if (esp_wifi_sta_get_rssi(&rssi) != ESP_OK) continue;
        int8_t power;
        if (rssi > -50)      power = 48;
        else if (rssi > -65) power = 60;
        else if (rssi > -75) power = 72;
        else                 power = 80;
        if (power != last_power) {
            esp_wifi_set_max_tx_power(power);
            ESP_LOGI(TAG, "tx_power=%d (rssi=%d)", power / 4, rssi);
            last_power = power;
        }
    }
}

void app_main(void)
{
    led_init();
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

    s_reconnect_sem = xSemaphoreCreateBinary();
    xTaskCreate(reconnect_task, "reconnect", 2048, NULL, 5, &s_reconnect_task);

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, wifi_event_handler, NULL));

    nvs_load_wifi_creds();

    const wifi_init_config_t wcfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wcfg));
    wifi_init_apsta();

    xEventGroupWaitBits(s_wifi_event_group,
        WIFI_CONNECTED_BIT | WIFI_FAIL_BIT, pdFALSE, pdFALSE,
        pdMS_TO_TICKS(STA_CONNECT_TIMEOUT_MS));

    uart_init();
    s_server = start_webserver();

    ESP_ERROR_CHECK(mdns_init());
    const char *hostname = CONFIG_LWIP_LOCAL_HOSTNAME;
    const size_t h_len = strlen(hostname);
    if (h_len > 6 && strcmp(hostname + h_len - 6, ".local") == 0) {
        char *trimmed = strndup(hostname, h_len - 6);
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
#ifdef CONFIG_OSC_TX_POWER_TASK
    xTaskCreate(tx_power_task, "tx_pwr", 2048, NULL, 5, NULL);
#endif

}
