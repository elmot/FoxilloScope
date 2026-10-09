# Forming `pin_defs.c` — Hardware Pin Check Guide

This document is a short, definitive guide on how to create and maintain `pin_defs.c` for **any STM32 microcontroller and custom board**.

All diagnostic logic lives in `main.c`; `pin_defs.c` is the **only file you create or modify** to define your board's wiring.

---

## 1. Extracting Nets from STM32CubeMX Pin Report

Export the pin report from STM32CubeMX (`<project_name>.txt`). Each pin configuration line contains the MCU pin name, GPIO port/pin, and optional user label in square brackets:

1. **Shared Net Labels**: Pins sharing the exact same label `[NET_NAME]` must be electrically connected together. Assign them the same numeric `net_id`.
2. **Internal Nets (`_NET_NAME`)**: Labels starting with an underscore (e.g. `_STAGE_A`, `_BIAS_REF`) designate internal MCU-to-MCU traces or jumpers (no external connector). Set `is_internal = true`.
3. **External Nets (`NET_NAME`)**: Labels without a leading underscore (e.g. `VGND`, `PROBE_IN`) connect to board headers or external connectors. Set `is_internal = false`.
4. **Isolated Signal Pins**: Pins with unique labels representing inputs, outputs, or test points get unique `net_id` values. They are tested for power rail faults (GND/VDD) and isolation against all other nets, but are not listed in `EXPECTED_NETS[]`.
5. **Debug SWD Pins**: Do **not** include SWCLK or SWDIO in `PINS[]`, as driving them interrupts the debug connection and semihosting output.

---

## 2. Descriptor Structures (`pin_defs.h`)

Declared in `pin_defs.h` and shared with `main.c`:

```c
typedef struct {
    const char   *name;       /* MCU pin name, e.g. "PA3" */
    const char   *net_name;   /* CubeMX label, e.g. "_STAGE_A" */
    const char   *header_pin; /* Board connector coordinate, e.g. "CN10-37" */
    GPIO_TypeDef *port;       /* GPIO port pointer, e.g. GPIOA */
    uint16_t      pin;        /* GPIO pin mask, e.g. GPIO_PIN_3 */
    uint8_t       net_id;     /* Shared ID grouping pins of the same net */
} PinDef_t;

typedef struct {
    const char *net_name;     /* Net label */
    bool        is_internal;  /* true if label starts with '_', false if external */
    uint8_t     pin_idx_a;    /* Index of first pin in PINS[] */
    uint8_t     pin_idx_b;    /* Index of second pin in PINS[] */
} ExpectedNet_t;

#define PIN_COUNT           17U
#define EXPECTED_NET_COUNT  7U

extern const PinDef_t      PINS[PIN_COUNT];
extern const ExpectedNet_t EXPECTED_NETS[EXPECTED_NET_COUNT];
```

---

## 3. Implementation Template (`pin_defs.c`)

```c
#include <assert.h>
#include "pin_defs.h"

const PinDef_t PINS[PIN_COUNT] = {
    /* Net 0: _STAGE_A (Internal: PA3 <-> PB12) */
    {"PA3",  "_STAGE_A", "CN10-37", GPIOA, GPIO_PIN_3,  0},
    {"PB12", "_STAGE_A", "CN10-16", GPIOB, GPIO_PIN_12, 0},

    /* Net 1: VGND (External: PA1 <-> PB11) */
    {"PA1",  "VGND",     "CN7-30",  GPIOA, GPIO_PIN_1,  1},
    {"PB11", "VGND",     "CN10-18", GPIOB, GPIO_PIN_11, 1},

    /* Net 2: IN_A (Isolated external signal: PB13) */
    {"PB13", "IN_A",     "CN10-30", GPIOB, GPIO_PIN_13, 2},
};

/* Multi-pin nets to verify for bidirectional continuity: indices refer to PINS[] */
const ExpectedNet_t EXPECTED_NETS[EXPECTED_NET_COUNT] = {
    {"_STAGE_A", true,  0, 1},
    {"VGND",     false, 2, 3},
};

/* Compile-time verification that sizes match header declarations */
static_assert(sizeof(PINS) / sizeof(PINS[0]) == PIN_COUNT, "PIN_COUNT mismatch");
static_assert(sizeof(EXPECTED_NETS) / sizeof(EXPECTED_NETS[0]) == EXPECTED_NET_COUNT, "EXPECTED_NET_COUNT mismatch");
```

---

## 4. Building and Running

1. **Build firmware**:
   ```bash
   cmake --build --preset Debug --target pin_check
   ```
2. **Execute automated test with semihosting capture**:
   ```bash
   cmake --build --preset Debug --target run_pin_check
   ```
3. **Artifacts generated**:
   - `pin_report.json`: Machine-readable test results written directly by the MCU via Arm semihosting.
   - `pin_check_output.log`: Complete semihosting session log.
