#include <stdint.h>

#include "esp_log.h"
#include "esp_wifi.h"
#include "gateway.h"
#include "cJSON.h"

extern const uint8_t _binary_index_html_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_index_html_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_scope_js_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_scope_js_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_wifi_html_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_wifi_html_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_uPlot_iife_min_js_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_uPlot_iife_min_js_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_uPlot_min_css_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_uPlot_min_css_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_favicon_png_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_favicon_png_end[]; // NOLINT(*-reserved-identifier)

extern const uint8_t _binary_wiring_png_start[]; // NOLINT(*-reserved-identifier)
extern const uint8_t _binary_wiring_png_end[]; // NOLINT(*-reserved-identifier)

typedef struct
{
    const char * uri;
    const char * data_start;
    const char * data_end;
    const char * type;
} static_resource_t;

static const char *TAG = "gateway";

const static_resource_t static_resources[] = { // NOLINT(*-interfaces-global-init)
    {
        .uri = "/", .data_start = (const char*)_binary_index_html_start,
        .data_end = (const char*)_binary_index_html_end, .type = "text/html"
    },
    {
        "/wifi", .data_start = (const char*)_binary_wifi_html_start,
        .data_end = (const char*)_binary_wifi_html_end, .type = "text/html"
    },

    {
        "/uPlot.min.css", .data_start = (const char*)_binary_uPlot_min_css_start,
        .data_end = (const char*)_binary_uPlot_min_css_end, .type = "text/css"
    },
    {
        "/scope.js", .data_start = (const char*)_binary_scope_js_start,
        .data_end = (const char*)_binary_scope_js_end, .type = "text/javascript"
    },
    {
        "/uPlot.iife.min.js", .data_start = (const char*)_binary_uPlot_iife_min_js_start,
        .data_end = (const char*)_binary_uPlot_iife_min_js_end, .type = "text/javascript"
    },
    {
        "/favicon.png", .data_start = (const char*)_binary_favicon_png_start,
        .data_end = (const char*)_binary_favicon_png_end, .type = "image/png"
    },
    {
        "/wiring.png", .data_start = (const char*)_binary_wiring_png_start,
        .data_end = (const char*)_binary_wiring_png_end, .type = "image/png"
    },

    {.uri = nullptr}
};

static esp_err_t http_get_handler(httpd_req_t *req)
{
    const static_resource_t*  res = req->user_ctx;
    httpd_resp_set_type(req, res->type);
    httpd_resp_send(req, res->data_start, res->data_end-res->data_start);
    return ESP_OK;
}

// ReSharper disable once CppParameterMayBeConst
void register_http_static_resources(httpd_handle_t hd)
{
    for (const static_resource_t* ptr = static_resources; ptr->uri != nullptr; ptr++)
    {
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
                                       .uri = ptr->uri, .method = HTTP_GET, .handler = http_get_handler,
                                       .user_ctx = (void*)ptr
                                   });
    }
}


static esp_err_t wifi_status_handler(httpd_req_t *req)
{
    static char buf[1024];
    const char *status;
    if (s_sta_connected) {
        status = "connected";
        wifi_ap_record_t ap;
        if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
            s_sta_rssi = (uint8_t)ap.rssi;
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

    const cJSON *ssid_item = cJSON_GetObjectItem(json, "ssid");
    if (!cJSON_IsString(ssid_item) || ssid_item->valuestring[0] == '\0') {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "SSID required");
        return ESP_FAIL;
    }

    char ssid[32] = {0};
    char password[64] = {0};
    snprintf(ssid, sizeof(ssid), "%s", ssid_item->valuestring);

    const cJSON *pwd_item = cJSON_GetObjectItem(json, "password");
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

static esp_err_t redirect_handler(httpd_req_t *req, [[maybe_unused]] httpd_err_code_t)
{
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "/wifi");
    httpd_resp_send(req, nullptr, 0);
    return ESP_OK;
}


httpd_handle_t start_webserver(void)
{
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.max_uri_handlers = 20;
    cfg.max_open_sockets = 20;
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
        httpd_register_err_handler(hd, HTTPD_404_NOT_FOUND, redirect_handler);
        ESP_LOGI(TAG, "Web server started on port %d", cfg.server_port);
    }
    return hd;
}

