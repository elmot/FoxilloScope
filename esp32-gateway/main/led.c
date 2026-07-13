#include "gateway.h"
#include "driver/rmt_tx.h"
#define LED_GPIO CONFIG_LED_GPIO
#define LED_RESOLUTION_HZ 10000000
#define LED_T0H 4
#define LED_T0L 8
#define LED_T1H 7
#define LED_T1L 5
#define LED_RESET 3000

static rmt_channel_handle_t s_led_chan = NULL;
static rmt_encoder_handle_t s_led_encoder = NULL;

static void led_set_rgb(uint8_t r, uint8_t g, uint8_t b)
{
    if (!s_led_chan || !s_led_encoder) return;
    rmt_symbol_word_t sym[25];
    const uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;
    for (int i = 0; i < 24; i++) {
        const bool bit = (grb >> (23 - i)) & 1;
        sym[i] = (rmt_symbol_word_t){
            .duration0 = bit ? LED_T1H : LED_T0H,
            .level0 = 1,
            .duration1 = bit ? LED_T1L : LED_T0L,
            .level1 = 0,
        };
    }
    sym[24] = (rmt_symbol_word_t){
        .duration0 = LED_RESET, .level0 = 0,
        .duration1 = 0, .level1 = 0,
    };
    const rmt_transmit_config_t t = { .loop_count = 0 };
    rmt_transmit(s_led_chan, s_led_encoder, sym, sizeof(sym), &t);
}

void led_refresh(void)
{
    if (!s_led_chan) return;
    if (ws_any_connected()) { led_set_rgb(0, 32, 0); return; }
    if (xEventGroupGetBits(s_wifi_event_group) & WIFI_FAIL_BIT) { led_set_rgb(32, 24, 0); return; }
    if (s_sta_connected) { led_set_rgb(0, 6, 0); return; }
    led_set_rgb(20,10,10);
}

void led_init(void)
{
    const rmt_tx_channel_config_t c = {
        .gpio_num = LED_GPIO,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = LED_RESOLUTION_HZ,
        .mem_block_symbols = 64,
        .trans_queue_depth = 1,
    };
    if (rmt_new_tx_channel(&c, &s_led_chan) != ESP_OK) return;
    rmt_copy_encoder_config_t ec = {};
    if (rmt_new_copy_encoder(&ec, &s_led_encoder) != ESP_OK) return;
    rmt_enable(s_led_chan);
    led_set_rgb(40,20,20);
}

