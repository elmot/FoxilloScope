#ifndef STM32_FLASHER_H
#define STM32_FLASHER_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// Target chip ID for STM32G474
#define STM32G474_PID 0x0469

// Protocol responses
#define STM32_ACK  0x79
#define STM32_NACK 0x1F

// Progress callback signature
typedef void (*stm32_flash_progress_cb_t)(size_t bytes_written, size_t total_bytes, const char *status);

/**
 * @brief Flash binary payload to STM32 device over existing UART using ST AN3155 protocol.
 * 
 * @param data Pointer to firmware binary buffer.
 * @param length Size of binary firmware in bytes.
 * @param start_address Destination flash memory start address (typically 0x08000000).
 * @param progress_cb Optional callback for progress reporting (can be NULL).
 * @return esp_err_t ESP_OK on success, or appropriate error code.
 * 
 * @note Upon successful completion, this function executes a system reboot.
 */
esp_err_t stm32_flash_binary(const uint8_t *data, size_t length, uint32_t start_address, stm32_flash_progress_cb_t progress_cb);

#ifdef __cplusplus
}
#endif

#endif // STM32_FLASHER_H
