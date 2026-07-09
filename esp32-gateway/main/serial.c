#include "freertos/FreeRTOS.h"
#include "gateway.h"
#include "driver/uart.h"
#include "soc/gpio_num.h"

#define UART_PORT UART_NUM_1
#define UART_BAUD 460800
#define UART_TX_PIN GPIO_NUM_6
#define UART_RX_PIN GPIO_NUM_7

#define UART_BUF_SIZE 10240

#define LINE_BUF_SIZE 2000
#define MAX_FRAME_LINES 128

static char s_line_buf[LINE_BUF_SIZE];
static int s_line_pos = 0;
static char *s_frame_lines[MAX_FRAME_LINES];
static int s_frame_count = 0;
static QueueHandle_t s_uart_queue = NULL;

void uart_write_str(const char *str)
{
    uart_write_bytes(UART_PORT, str, strlen(str));
}

// ReSharper disable once CppDFAConstantParameter
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

__noreturn static void uart_event_task(__unused void *arg)
{
    uart_event_t event;
    static char data[UART_BUF_SIZE];

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
}

void uart_init(void)
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
    ESP_ERROR_CHECK(uart_driver_install(UART_PORT, 256, 256, 20, &s_uart_queue, 0));
    xTaskCreate(uart_event_task, "uart_evt", 4096, NULL, 10, NULL);
}
