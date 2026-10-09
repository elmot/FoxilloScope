/**
  ******************************************************************************
  * @file    pin_defs.c
  * @brief   Pin and expected-net tables derived from the CubeMX pin report.
  *
  * Rules (see docs/wiring_template.html):
  * - Pins with identical labels are connected together.
  * - Labels starting with '_' are internal connections (no external connector).
  * - Labels without leading '_' have external connectors.
  ******************************************************************************
  */
#include <assert.h>
#include "pin_defs.h"

const PinDef_t PINS[PIN_COUNT] = {
    // Net 0: _STAGE.A (Internal: PA3 <-> PB12)
    {"PA3",  "_STAGE.A",    "CN10-37", GPIOA, GPIO_PIN_3,  0},
    {"PB12", "_STAGE.A",    "CN10-16", GPIOB, GPIO_PIN_12, 0},

    // Net 1: _BIAS.A (Internal: PA4 <-> PB10)
    {"PA4",  "_BIAS.A",     "CN7-32",  GPIOA, GPIO_PIN_4,  1},
    {"PB10", "_BIAS.A",     "CN10-25", GPIOB, GPIO_PIN_10, 1},

    // Net 2: _BIAS.B (Internal: PA5 <-> PB15)
    {"PA5",  "_BIAS.B",     "CN10-11", GPIOA, GPIO_PIN_5,  2},
    {"PB15", "_BIAS.B",     "CN10-26", GPIOB, GPIO_PIN_15, 2},

    // Net 3: _SIGNAL.A (Internal: PA2 <-> PA7) [PA7 has label _SIGNAL_A in CubeMX]
    {"PA2",  "_SIGNAL.A",   "CN10-35", GPIOA, GPIO_PIN_2,  3},
    {"PA7",  "_SIGNAL.A",   "CN10-15", GPIOA, GPIO_PIN_7,  3},

    // Net 4: _SIGNAL.B (Internal: PB1 <-> PB14)
    {"PB1",  "_SIGNAL.B",   "CN10-24", GPIOB, GPIO_PIN_1,  4},
    {"PB14", "_SIGNAL.B",   "CN10-28", GPIOB, GPIO_PIN_14, 4},

    // Net 5: _STAGE.B (Internal: PB2 <-> PA8)
    {"PB2",  "_STAGE.B",    "CN10-22", GPIOB, GPIO_PIN_2,  5},
    {"PA8",  "_STAGE.B",    "CN10-23", GPIOA, GPIO_PIN_8,  5},

    // Net 6: VGND (External: PA1 <-> PB11)
    {"PA1",  "VGND",        "CN7-30",  GPIOA, GPIO_PIN_1,  6},
    {"PB11", "VGND",        "CN10-18", GPIOB, GPIO_PIN_11, 6},

    // Net 7: IN.A (External single pin: PB13)
    {"PB13", "IN.A",        "CN10-30", GPIOB, GPIO_PIN_13, 7},

    // Net 8: IN.B (External single pin: PC3)
    {"PC3",  "IN.B",        "CN7-37",  GPIOC, GPIO_PIN_3,  8},

    // Net 9: TEST.SIGNAL (External single pin: PA6)
    {"PA6",  "TEST.SIGNAL", "CN10-13", GPIOA, GPIO_PIN_6,  9},
};

/* Nets with >= 2 pins: indices refer to PINS[] */
const ExpectedNet_t EXPECTED_NETS[EXPECTED_NET_COUNT] = {
    {"_STAGE.A",  true,   0,  1},
    {"_BIAS.A",   true,   2,  3},
    {"_BIAS.B",   true,   4,  5},
    {"_SIGNAL.A", true,   6,  7},
    {"_SIGNAL.B", true,   8,  9},
    {"_STAGE.B",  true,  10, 11},
    {"VGND",      false, 12, 13},
};

/* Detect tables that are out of sync with the count macros in pin_defs.h */
static_assert(sizeof(PINS) / sizeof(PINS[0]) == PIN_COUNT, "PIN_COUNT mismatch");
static_assert(sizeof(EXPECTED_NETS) / sizeof(EXPECTED_NETS[0]) == EXPECTED_NET_COUNT, "EXPECTED_NET_COUNT mismatch");
