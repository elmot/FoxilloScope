#include <string.h>
#include "freertos/FreeRTOS.h"
#include "gateway.h"
#include "driver/uart.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "soc/gpio_num.h"

#define UART_PORT UART_NUM_1
#define UART_BAUD 460800
#define UART_TX_PIN CONFIG_OSC_UART_TX
#define UART_RX_PIN CONFIG_OSC_UART_RX

#define BUF_SIZE 20000

static QueueHandle_t s_uart_queue = nullptr;

void uart_write_str(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

// ReSharper disable once CppDFAConstantParameter
static void transmit_error(const char *msg)
{
    char buf[128];
    const int n = snprintf(buf, sizeof(buf), "error: %s", msg);
    if (n > 0) ws_transmit(buf, n);
}

[[noreturn]] static void uart_event_task([[maybe_unused]] void *arg)
{
    static char buf[BUF_SIZE];
    static int len = 0;
    static bool discard = true;
    uart_event_t event;

    for (;;) {
        if (xQueueReceive(s_uart_queue, &event, portMAX_DELAY)) {
            switch (event.type) {
            case UART_DATA: {
                const int room = BUF_SIZE - 1 - len;
                const int n = event.size < room ? (int)event.size : room;
                uart_read_bytes(UART_PORT, buf + len, n, 0);
                len += n;

                if (discard) {
                    const char *hash = memchr(buf, '#', len);
                    if (hash) {
                        discard = false;
                        const int after = len - (hash - buf) - 1;
                        memmove(buf, hash + 1, after);
                        len = after;
                    } else {
                        len = 0;
                    }
                }

                if (!discard) {
                    char *hash;
                    while ((hash = memchr(buf, '#', len)) != NULL) {
                        const int idx = hash - buf;
                        *hash = '\0';
                        if (idx > 0) ws_transmit(buf, idx);
                        const int after = len - idx - 1;
                        memmove(buf, hash + 1, after);
                        len = after;
                    }
                }
                buf[len] = '\0';
                break;
            }
            case UART_FRAME_ERR:
                transmit_error("UART frame error");
                break;
            case UART_FIFO_OVF:
                uart_flush_input(UART_PORT);
                break;
            default:
                break;
            }
        }
    }
}

void uart_init(void)
{
    static const uart_config_t cfg = {
        .baud_rate = UART_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_param_config(UART_PORT, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 4096, 256, 20, &s_uart_queue, 0));
    xTaskCreate(uart_event_task, "uart_evt", 4096, nullptr, 10, nullptr);
}
