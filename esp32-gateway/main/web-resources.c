#include <stdint.h>

#include "gateway.h"

extern const uint8_t _binary_index_html_start[];
extern const uint8_t _binary_index_html_end[];

extern const uint8_t _binary_wifi_html_start[];
extern const uint8_t _binary_wifi_html_end[];

extern const uint8_t _binary_uPlot_iife_min_js_start[];
extern const uint8_t _binary_uPlot_iife_min_js_end[];

extern const uint8_t _binary_uPlot_min_css_start[];
extern const uint8_t _binary_uPlot_min_css_end[];

extern const uint8_t _binary_favicon_png_start[];
extern const uint8_t _binary_favicon_png_end[];

extern const uint8_t _binary_wiring_png_start[];
extern const uint8_t _binary_wiring_png_end[];

typedef struct
{
    const char * uri;
    const char * data_start;
    const char * data_end;
    const char * type;
} static_resource_t;

const static_resource_t static_resources[] = {
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

void register_http_static_resources(const httpd_handle_t hd)
{
    for (const static_resource_t* ptr = static_resources; ptr->uri != nullptr; ptr++)
    {
        httpd_register_uri_handler(hd, &(const httpd_uri_t){
                                       .uri = ptr->uri, .method = HTTP_GET, .handler = http_get_handler,
                                       .user_ctx = (void*)ptr
                                   });
    }
}
