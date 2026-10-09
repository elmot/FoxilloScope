/**
  ******************************************************************************
  * @file    pin_defs.h
  * @brief   Pin and net descriptor types for the pin check firmware.
  *          The actual pin table lives in pin_defs.c.
  ******************************************************************************
  */
#ifndef PIN_DEFS_H
#define PIN_DEFS_H

#include <stdbool.h>
#include <stdint.h>
#include "stm32g4xx_hal.h" /* device registers and GPIO_PIN_x masks (headers only) */

#ifdef __cplusplus
extern "C" {
#endif

/** Descriptor of a single tested pin. */
typedef struct {
    const char   *name;       /**< MCU pin name, e.g. "PA3" */
    const char   *net_name;   /**< Net label from the CubeMX report, e.g. "_STAGE_A" */
    const char   *header_pin; /**< Physical board connector pin, e.g. "CN10-37" */
    GPIO_TypeDef *port;       /**< GPIO port */
    uint16_t      pin;        /**< GPIO pin mask (GPIO_PIN_x) */
    uint8_t       net_id;     /**< Pins sharing the same net_id are expected to be connected */
} PinDef_t;

/** Descriptor of an expected connection between two pins of PINS[]. */
typedef struct {
    const char *net_name;     /**< Net name */
    bool        is_internal;  /**< true if the net label starts with '_' (no external connector) */
    uint8_t     pin_idx_a;    /**< Index of the first pin in PINS[] */
    uint8_t     pin_idx_b;    /**< Index of the second pin in PINS[] */
} ExpectedNet_t;

/* Table sizes. Must match the tables in pin_defs.c (checked there by static assertions). */
#define PIN_COUNT           17U
#define EXPECTED_NET_COUNT  7U

extern const PinDef_t        PINS[PIN_COUNT];
extern const ExpectedNet_t   EXPECTED_NETS[EXPECTED_NET_COUNT];

#ifdef __cplusplus
}
#endif

#endif /* PIN_DEFS_H */
