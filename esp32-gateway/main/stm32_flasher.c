#include "stm32_flasher.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "gateway.h"

static const char *TAG = "stm32_flasher";
#define UART_PORT UART_NUM_1

#define CMD_GET_ID       0x02
#define CMD_WRITE_MEMORY 0x31
#define CMD_EXT_ERASE    0x44

static esp_err_t send_byte(uint8_t b)
{
    int written = uart_write_bytes(UART_PORT, (const char *)&b, 1);
    return written == 1 ? ESP_OK : ESP_FAIL;
}

static esp_err_t read_byte(uint8_t *b, uint32_t timeout_ms)
{
    int read = uart_read_bytes(UART_PORT, b, 1, pdMS_TO_TICKS(timeout_ms));
    return read == 1 ? ESP_OK : ESP_ERR_TIMEOUT;
}

static esp_err_t wait_ack(uint32_t timeout_ms)
{
    uint8_t resp = 0;
    esp_err_t err = read_byte(&resp, timeout_ms);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Timeout waiting for ACK/NACK (%u ms)", (unsigned int)timeout_ms);
        return err;
    }
    if (resp == STM32_ACK) {
        return ESP_OK;
    }
    if (resp == STM32_NACK) {
        ESP_LOGE(TAG, "Received NACK (0x1F) from bootloader");
        return ESP_FAIL;
    }
    ESP_LOGE(TAG, "Unexpected bootloader response: 0x%02X", resp);
    return ESP_FAIL;
}

static esp_err_t send_cmd(uint8_t cmd)
{
    send_byte(cmd);
    send_byte(cmd ^ 0xFF);
    return wait_ack(1000);
}

static esp_err_t stm32_sync(void)
{
    ESP_LOGI(TAG, "Syncing with STM32 bootloader (sending 0x7F)...");
    uart_flush_input(UART_PORT);
    for (int retry = 0; retry < 10000; retry++) {
        send_byte(0x7F);
        if (wait_ack(200) == ESP_OK) {
            ESP_LOGI(TAG, "Sync successful!");
            return ESP_OK;
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    ESP_LOGE(TAG, "Failed to sync with bootloader");
    return ESP_FAIL;
}

static esp_err_t stm32_cmd_get_id(uint16_t *chip_id)
{
    if (send_cmd(CMD_GET_ID) != ESP_OK) {
        return ESP_FAIL;
    }
    uint8_t num_bytes = 0;
    if (read_byte(&num_bytes, 1000) != ESP_OK) {
        return ESP_FAIL;
    }
    uint8_t id_buf[4] = {0};
    int count = num_bytes + 1;
    for (int i = 0; i < count; i++) {
        if (read_byte(&id_buf[i], 1000) != ESP_OK) {
            return ESP_FAIL;
        }
    }
    if (wait_ack(1000) != ESP_OK) {
        return ESP_FAIL;
    }
    *chip_id = (id_buf[0] << 8) | id_buf[1];
    ESP_LOGI(TAG, "Detected STM32 Chip ID: 0x%04X", *chip_id);
    return ESP_OK;
}

static esp_err_t stm32_cmd_mass_erase(void)
{
    ESP_LOGI(TAG, "Performing Extended Mass Erase...");
    if (send_cmd(CMD_EXT_ERASE) != ESP_OK) {
        return ESP_FAIL;
    }
    // Mass erase code for AN3155 extended erase: 0xFF 0xFF, XOR checksum = 0x00
    send_byte(0xFF);
    send_byte(0xFF);
    send_byte(0x00);
    
    // Mass erase can take several seconds on G4 flash
    if (wait_ack(10000) != ESP_OK) {
        ESP_LOGE(TAG, "Extended mass erase failed");
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "Mass erase completed successfully");
    return ESP_OK;
}

static esp_err_t stm32_cmd_write_chunk(uint32_t address, const uint8_t *buf, size_t chunk_len)
{
    if (chunk_len == 0 || chunk_len > 256) {
        return ESP_ERR_INVALID_ARG;
    }
    if (send_cmd(CMD_WRITE_MEMORY) != ESP_OK) {
        return ESP_FAIL;
    }
    // Send 4-byte address + checksum
    uint8_t a3 = (address >> 24) & 0xFF;
    uint8_t a2 = (address >> 16) & 0xFF;
    uint8_t a1 = (address >> 8) & 0xFF;
    uint8_t a0 = address & 0xFF;
    uint8_t addr_chk = a3 ^ a2 ^ a1 ^ a0;

    send_byte(a3);
    send_byte(a2);
    send_byte(a1);
    send_byte(a0);
    send_byte(addr_chk);

    if (wait_ack(1000) != ESP_OK) {
        return ESP_FAIL;
    }

    // Send length byte N-1, data bytes, checksum
    uint8_t n = (uint8_t)(chunk_len - 1);
    send_byte(n);
    
    uint8_t checksum = n;
    for (size_t i = 0; i < chunk_len; i++) {
        send_byte(buf[i]);
        checksum ^= buf[i];
    }
    send_byte(checksum);

    if (wait_ack(2000) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write chunk at 0x%08X", (unsigned int)address);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t stm32_flash_binary(const uint8_t *data, size_t length, uint32_t start_address, stm32_flash_progress_cb_t progress_cb)
{
    if (!data || length == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Preparing to flash %u bytes to 0x%08X", (unsigned int)length, (unsigned int)start_address);

    uart_prepare_for_flashing();

    if (stm32_sync() != ESP_OK) {
        ESP_LOGE(TAG, "Sync failed.");
        return ESP_FAIL;
    }

    uint16_t chip_id = 0;
    if (stm32_cmd_get_id(&chip_id) != ESP_OK) {
        ESP_LOGE(TAG, "Get ID failed.");
        return ESP_FAIL;
    }

    if (chip_id != STM32G474_PID) {
        ESP_LOGW(TAG, "Chip ID 0x%04X does not match expected 0x%04X", chip_id, STM32G474_PID);
    }

    if (stm32_cmd_mass_erase() != ESP_OK) {
        ESP_LOGE(TAG, "Mass erase failed.");
        return ESP_FAIL;
    }

    size_t offset = 0;
    while (offset < length) {
        size_t chunk_len = length - offset;
        if (chunk_len > 256) {
            chunk_len = 256;
        }
        
        esp_err_t err = stm32_cmd_write_chunk(start_address + offset, data + offset, chunk_len);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Writing chunk at offset %u failed.", (unsigned int)offset);
            return err;
        }

        offset += chunk_len;

        if (progress_cb) {
            progress_cb(offset, length, "Flashing...");
        }
    }

    ESP_LOGI(TAG, "Flashing completed successfully!");
    return ESP_OK;
}
