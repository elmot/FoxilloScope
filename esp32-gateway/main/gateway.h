//
// Created by elmot on 09/07/2026.
//

#ifndef ESP32_GATEWAY_GATEWAY_H
#define ESP32_GATEWAY_GATEWAY_H

void uart_init(void);
void uart_write_str(const char *str);
void broadcast_text(const char *text);

#endif //ESP32_GATEWAY_GATEWAY_H
