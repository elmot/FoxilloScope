/**
  ******************************************************************************
  * @file           : pin_check/main.c
  * @brief          : Pin check hardware diagnostic firmware.
  *
  * ARCHITECTURE & RUNTIME MODEL:
  * 1. Bare-Metal Execution: No CubeMX HAL/LL init, no interrupts (__disable_irq),
  *    no RTOS. The MCU boots on reset-default clock. Delays use polled SysTick COUNTFLAG.
  * 2. Arm Semihosting (--specs=rdimon.specs):
  *    - File I/O: fopen("pin_report.json", "w") writes report to host via SYS_OPEN/WRITE.
  *    - Console: printf() streams machine-readable JSON followed by human report.
  * 3. Hardware-Safe 3-Phase Testing:
  *    - Phase 1 (Rail Shorts): Weak internal pull-up detects GND shorts; weak pull-down
  *      detects VDD shorts (current < 100 uA, safe against dead shorts).
  *    - Phase 2 (Continuity): Bidirectional drive & sense (HIGH & LOW) on expected nets.
  *    - Phase 3 (Isolation): Drives each pin, senses all other pins across differing nets
  *      to detect unexpected solder bridges or shorts.
  *    - Resting state: All tested pins placed in high-impedance GPIO_MODE_ANALOG.
  * 4. Tables: Pin/net definitions are defined separately in pin_defs.c.
  ******************************************************************************
  */

#include "pin_defs.h"
#include <stdio.h>
#include <stdbool.h>

/* Provided by the semihosting C library (rdimon) */
extern void initialise_monitor_handles(void);

static void run_pin_check(uint32_t cycle_count);

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}

/* ----------------------------------------------------------------------------
 * Interrupt-less SysTick delay (polls COUNTFLAG, TICKINT stays cleared)
 * ---------------------------------------------------------------------------- */
static void delay_ticks(uint32_t ticks)
{
    while (ticks)
    {
        const uint32_t chunk = (ticks > SysTick_LOAD_RELOAD_Msk) ? SysTick_LOAD_RELOAD_Msk : ticks;
        SysTick->CTRL = 0;
        SysTick->LOAD = chunk - 1U;
        SysTick->VAL  = 0;
        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_ENABLE_Msk; /* core clock, no IRQ */
        while (!(SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk)) {}
        ticks -= chunk;
    }
    SysTick->CTRL = 0;
}

static void delay_us(uint32_t us)
{
    delay_ticks(us * (SystemCoreClock / 1000000U));
}

static void delay_ms(uint32_t ms)
{
    delay_us(ms * 1000U);
}

/* Configure all tested pins as analog (high impedance / safe state) */
static void set_all_pins_analog(void)
{
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        GPIO_InitTypeDef init = {0};
        init.Pin = PINS[i].pin;
        init.Mode = GPIO_MODE_ANALOG;
        init.Pull = GPIO_NOPULL;
        HAL_GPIO_Init(PINS[i].port, &init);
    }
}

/* Configure all tested pins as input with specified pull */
static void set_all_pins_input(uint32_t pull)
{
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        GPIO_InitTypeDef init = {0};
        init.Pin = PINS[i].pin;
        init.Mode = GPIO_MODE_INPUT;
        init.Pull = pull;
        HAL_GPIO_Init(PINS[i].port, &init);
    }
}

/* Configure specific pin as output push-pull with initial state */
static void set_pin_output(size_t idx, GPIO_PinState state)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = PINS[idx].pin;
    init.Mode = GPIO_MODE_OUTPUT_PP;
    init.Pull = GPIO_NOPULL;
    init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_WritePin(PINS[idx].port, PINS[idx].pin, state);
    HAL_GPIO_Init(PINS[idx].port, &init);
}

/* Configure specific pin as input with specified pull */
static void set_pin_input(size_t idx, uint32_t pull)
{
    GPIO_InitTypeDef init = {0};
    init.Pin = PINS[idx].pin;
    init.Mode = GPIO_MODE_INPUT;
    init.Pull = pull;
    HAL_GPIO_Init(PINS[idx].port, &init);
}

/* Output machine-readable JSON report to the given stream */
static void write_json_report(FILE *stream, uint32_t cycle_count,
                              const uint8_t *rail_faults, uint32_t rail_errors,
                              const bool *net_passed, uint32_t continuity_errors,
                              const bool short_matrix[PIN_COUNT][PIN_COUNT], uint32_t isolation_errors,
                              uint32_t total_errors)
{
    fprintf(stream, "{\n");
    fprintf(stream, "  \"cycle\": %lu,\n", (unsigned long)cycle_count);
    fprintf(stream, "  \"status\": \"%s\",\n", (total_errors == 0) ? "PASS" : "FAIL");
    fprintf(stream, "  \"summary\": {\n");
    fprintf(stream, "    \"total_errors\": %lu,\n", (unsigned long)total_errors);
    fprintf(stream, "    \"rail_errors\": %lu,\n", (unsigned long)rail_errors);
    fprintf(stream, "    \"continuity_errors\": %lu,\n", (unsigned long)continuity_errors);
    fprintf(stream, "    \"isolation_errors\": %lu,\n", (unsigned long)isolation_errors);
    fprintf(stream, "    \"pins_checked\": %u,\n", (unsigned int)PIN_COUNT);
    fprintf(stream, "    \"nets_checked\": %u\n", (unsigned int)EXPECTED_NET_COUNT);
    fprintf(stream, "  },\n");

    /* Power rail errors */
    fprintf(stream, "  \"rail_errors\": [");
    bool first = true;
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        if (rail_faults[i] != 0)
        {
            if (!first)
            {
                fprintf(stream, ",");
            }
            first = false;
            fprintf(stream, "\n    {\n");
            fprintf(stream, "      \"pin\": \"%s\",\n", PINS[i].name);
            fprintf(stream, "      \"header\": \"%s\",\n", PINS[i].header_pin);
            fprintf(stream, "      \"net\": \"%s\",\n", PINS[i].net_name);
            fprintf(stream, "      \"error\": \"%s\"\n", (rail_faults[i] == 1) ? "SHORT_TO_GND" : "SHORT_TO_VDD");
            fprintf(stream, "    }");
        }
    }
    if (!first)
    {
        fprintf(stream, "\n  ");
    }
    fprintf(stream, "],\n");

    /* Expected net continuity results */
    fprintf(stream, "  \"expected_nets\": [");
    for (size_t n = 0; n < EXPECTED_NET_COUNT; n++)
    {
        const size_t ia = EXPECTED_NETS[n].pin_idx_a;
        const size_t ib = EXPECTED_NETS[n].pin_idx_b;
        if (n > 0)
        {
            fprintf(stream, ",");
        }
        fprintf(stream, "\n    {\n");
        fprintf(stream, "      \"net\": \"%s\",\n", EXPECTED_NETS[n].net_name);
        fprintf(stream, "      \"type\": \"%s\",\n", EXPECTED_NETS[n].is_internal ? "internal" : "external");
        fprintf(stream, "      \"pin_a\": \"%s\",\n", PINS[ia].name);
        fprintf(stream, "      \"header_a\": \"%s\",\n", PINS[ia].header_pin);
        fprintf(stream, "      \"pin_b\": \"%s\",\n", PINS[ib].name);
        fprintf(stream, "      \"header_b\": \"%s\",\n", PINS[ib].header_pin);
        fprintf(stream, "      \"status\": \"%s\"\n", net_passed[n] ? "PASS" : "FAIL");
        fprintf(stream, "    }");
    }
    if (EXPECTED_NET_COUNT > 0)
    {
        fprintf(stream, "\n  ");
    }
    fprintf(stream, "],\n");

    /* Unexpected short pairs */
    fprintf(stream, "  \"unexpected_shorts\": [");
    first = true;
    for (size_t p1 = 0; p1 < PIN_COUNT; p1++)
    {
        for (size_t p2 = p1 + 1; p2 < PIN_COUNT; p2++)
        {
            if (short_matrix[p1][p2])
            {
                if (!first)
                {
                    fprintf(stream, ",");
                }
                first = false;
                fprintf(stream, "\n    {\n");
                fprintf(stream, "      \"pin_a\": \"%s\",\n", PINS[p1].name);
                fprintf(stream, "      \"header_a\": \"%s\",\n", PINS[p1].header_pin);
                fprintf(stream, "      \"net_a\": \"%s\",\n", PINS[p1].net_name);
                fprintf(stream, "      \"pin_b\": \"%s\",\n", PINS[p2].name);
                fprintf(stream, "      \"header_b\": \"%s\",\n", PINS[p2].header_pin);
                fprintf(stream, "      \"net_b\": \"%s\"\n", PINS[p2].net_name);
                fprintf(stream, "    }");
            }
        }
    }
    if (!first)
    {
        fprintf(stream, "\n  ");
    }
    fprintf(stream, "]\n");

    fprintf(stream, "}\n");
}

/* Run one full pin-checking test suite */
static void run_pin_check(uint32_t cycle_count)
{
    uint32_t total_errors = 0;
    uint32_t rail_errors = 0;
    uint32_t continuity_errors = 0;
    uint32_t isolation_errors = 0;

    /* Result buffers: collect all diagnostics before printing */
    uint8_t rail_faults[PIN_COUNT] = {0}; /* 0: OK, 1: GND short, 2: VDD short */
    bool net_passed[EXPECTED_NET_COUNT] = {false};
    bool short_matrix[PIN_COUNT][PIN_COUNT] = {{false}};

    /* -------------------------------------------------------------
     * Phase 1: Power Rail Checks (Shorts to GND and VDD)
     * ------------------------------------------------------------- */
    // 1A: Pull-up test -> detects pins tied to GND
    set_all_pins_input(GPIO_PULLUP);
    delay_us(50);
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        if (HAL_GPIO_ReadPin(PINS[i].port, PINS[i].pin) == GPIO_PIN_RESET)
        {
            rail_faults[i] = 1;
            rail_errors++;
            total_errors++;
        }
    }

    // 1B: Pull-down test -> detects pins tied to VDD (3.3V)
    set_all_pins_input(GPIO_PULLDOWN);
    delay_us(50);
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        if (HAL_GPIO_ReadPin(PINS[i].port, PINS[i].pin) == GPIO_PIN_SET)
        {
            rail_faults[i] = 2;
            rail_errors++;
            total_errors++;
        }
    }

    /* -------------------------------------------------------------
     * Phase 2: Expected Connections Continuity Checks
     * ------------------------------------------------------------- */
    for (size_t n = 0; n < EXPECTED_NET_COUNT; n++)
    {
        const size_t ia = EXPECTED_NETS[n].pin_idx_a;
        const size_t ib = EXPECTED_NETS[n].pin_idx_b;
        bool net_ok = true;

        // Drive ia HIGH against PULLDOWN on all other pins
        set_all_pins_input(GPIO_PULLDOWN);
        set_pin_output(ia, GPIO_PIN_SET);
        delay_us(200);
        if (HAL_GPIO_ReadPin(PINS[ib].port, PINS[ib].pin) != GPIO_PIN_SET)
        {
            net_ok = false;
        }
        set_pin_input(ia, GPIO_PULLDOWN);

        // Drive ia LOW against PULLUP on all other pins
        set_all_pins_input(GPIO_PULLUP);
        set_pin_output(ia, GPIO_PIN_RESET);
        delay_us(200);
        if (HAL_GPIO_ReadPin(PINS[ib].port, PINS[ib].pin) != GPIO_PIN_RESET)
        {
            net_ok = false;
        }
        set_pin_input(ia, GPIO_PULLUP);

        // Reverse check: Drive ib HIGH against PULLDOWN
        set_all_pins_input(GPIO_PULLDOWN);
        set_pin_output(ib, GPIO_PIN_SET);
        delay_us(200);
        if (HAL_GPIO_ReadPin(PINS[ia].port, PINS[ia].pin) != GPIO_PIN_SET)
        {
            net_ok = false;
        }
        set_pin_input(ib, GPIO_PULLDOWN);

        // Reverse check: Drive ib LOW against PULLUP
        set_all_pins_input(GPIO_PULLUP);
        set_pin_output(ib, GPIO_PIN_RESET);
        delay_us(200);
        if (HAL_GPIO_ReadPin(PINS[ia].port, PINS[ia].pin) != GPIO_PIN_RESET)
        {
            net_ok = false;
        }
        set_pin_input(ib, GPIO_PULLUP);

        net_passed[n] = net_ok;
        if (!net_ok)
        {
            continuity_errors++;
            total_errors++;
        }
    }

    /* -------------------------------------------------------------
     * Phase 3: Unexpected Interconnections (Isolation Checks)
     * ------------------------------------------------------------- */
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        // 3A: Drive pin i HIGH against PULLDOWN
        set_all_pins_input(GPIO_PULLDOWN);
        set_pin_output(i, GPIO_PIN_SET);
        delay_us(200);

        for (size_t j = 0; j < PIN_COUNT; j++)
        {
            if (i == j) continue;
            if (PINS[i].net_id != PINS[j].net_id)
            {
                if (HAL_GPIO_ReadPin(PINS[j].port, PINS[j].pin) == GPIO_PIN_SET)
                {
                    const size_t p1 = (i < j) ? i : j;
                    const size_t p2 = (i < j) ? j : i;
                    if (!short_matrix[p1][p2])
                    {
                        short_matrix[p1][p2] = true;
                        isolation_errors++;
                        total_errors++;
                    }
                }
            }
        }
        set_pin_input(i, GPIO_PULLDOWN);

        // 3B: Drive pin i LOW against PULLUP
        set_all_pins_input(GPIO_PULLUP);
        set_pin_output(i, GPIO_PIN_RESET);
        delay_us(20);

        for (size_t j = 0; j < PIN_COUNT; j++)
        {
            if (i == j) continue;
            if (PINS[i].net_id != PINS[j].net_id)
            {
                if (HAL_GPIO_ReadPin(PINS[j].port, PINS[j].pin) == GPIO_PIN_RESET)
                {
                    const size_t p1 = (i < j) ? i : j;
                    const size_t p2 = (i < j) ? j : i;
                    if (!short_matrix[p1][p2])
                    {
                        short_matrix[p1][p2] = true;
                        isolation_errors++;
                        total_errors++;
                    }
                }
            }
        }
        set_pin_input(i, GPIO_PULLUP);
    }

    /* Safe resting state: return all pins to analog high impedance */
    set_all_pins_analog();

    /* -------------------------------------------------------------
     * Machine-Readable Report Output:
     * 1. Write JSON file to host via ARM semihosting (SYS_OPEN/WRITE)
     * 2. Print JSON block to semihosting console before human report
     * ------------------------------------------------------------- */
    FILE *fp = fopen("pin_report.json", "w");
    if (fp != NULL)
    {
        write_json_report(fp, cycle_count,
                          rail_faults, rail_errors,
                          net_passed, continuity_errors,
                          short_matrix, isolation_errors,
                          total_errors);
        fclose(fp);
    }

    printf("--- BEGIN JSON ---\n");
    write_json_report(stdout, cycle_count,
                      rail_faults, rail_errors,
                      net_passed, continuity_errors,
                      short_matrix, isolation_errors,
                      total_errors);
    printf("--- END JSON ---\n\n");

    /* -------------------------------------------------------------
     * Human-Readable Report Output
     * ------------------------------------------------------------- */
    printf("============================================================\n");
    printf(" Hardware Pin Check (Test Cycle #%lu)\n", (unsigned long)cycle_count);
    printf(" Target MCU: STM32G474RET6\n");
    if (fp != NULL)
    {
        printf(" Machine-readable file: written to pin_report.json\n");
    }
    printf("============================================================\n");

    /* Phase 1: Rail Checks */
    printf("\n--- Phase 1: Power Rail Checks (GND / VDD Shorts) ---\n");
    if (rail_errors == 0)
    {
        printf("  [PASS] All %u pins free of power rail shorts (GND / VDD).\n", (unsigned int)PIN_COUNT);
    }
    else
    {
        for (size_t i = 0; i < PIN_COUNT; i++)
        {
            if (rail_faults[i] == 1)
            {
                printf("  [FAIL] SHORT TO GND: Pin %-4s (%-7s, Net: %s)\n",
                       PINS[i].name, PINS[i].header_pin, PINS[i].net_name);
            }
            else if (rail_faults[i] == 2)
            {
                printf("  [FAIL] SHORT TO VDD: Pin %-4s (%-7s, Net: %s)\n",
                       PINS[i].name, PINS[i].header_pin, PINS[i].net_name);
            }
        }
    }

    /* Phase 2: Expected Connections */
    printf("\n--- Phase 2: Net Continuity Checks (Expected Connections) ---\n");
    for (size_t n = 0; n < EXPECTED_NET_COUNT; n++)
    {
        const size_t ia = EXPECTED_NETS[n].pin_idx_a;
        const size_t ib = EXPECTED_NETS[n].pin_idx_b;
        if (net_passed[n])
        {
            printf("  [PASS] Net %-10s : %s (%s) <-> %s (%s) connected [%s]\n",
                   EXPECTED_NETS[n].net_name,
                   PINS[ia].name, PINS[ia].header_pin,
                   PINS[ib].name, PINS[ib].header_pin,
                   EXPECTED_NETS[n].is_internal ? "Internal" : "External");
        }
        else
        {
            printf("  [FAIL] Net %-10s : %s (%s) <-> %s (%s) DISCONNECTED! [%s]\n",
                   EXPECTED_NETS[n].net_name,
                   PINS[ia].name, PINS[ia].header_pin,
                   PINS[ib].name, PINS[ib].header_pin,
                   EXPECTED_NETS[n].is_internal ? "Internal" : "External");
        }
    }

    /* Phase 3: Unexpected Shorts */
    printf("\n--- Phase 3: Net Isolation Checks (Unexpected Shorts) ---\n");
    if (isolation_errors == 0)
    {
        printf("  [PASS] All %u pin pairs verified: NO unexpected interconnections detected.\n",
               (unsigned int)(PIN_COUNT * (PIN_COUNT - 1U) / 2U));
    }
    else
    {
        for (size_t p1 = 0; p1 < PIN_COUNT; p1++)
        {
            for (size_t p2 = p1 + 1; p2 < PIN_COUNT; p2++)
            {
                if (short_matrix[p1][p2])
                {
                    printf("  [FAIL] SHORT: %s (%s, Net: %s) <-> %s (%s, Net: %s) are SHORTED!\n",
                           PINS[p1].name, PINS[p1].header_pin, PINS[p1].net_name,
                           PINS[p2].name, PINS[p2].header_pin, PINS[p2].net_name);
                }
            }
        }
    }

    /* Summary */
    printf("\n------------------------------------------------------------\n");
    if (total_errors == 0)
    {
        printf(" >>> RESULT: ALL PIN CHECKS PASSED (0 errors) <<<\n");
        printf(" - All %u expected nets connected properly.\n", (unsigned int)EXPECTED_NET_COUNT);
        printf(" - All %u pins isolated without unexpected shorts.\n", (unsigned int)PIN_COUNT);
        printf(" - Hardware wiring matches CubeMX specification.\n");
    }
    else
    {
        printf(" >>> RESULT: TEST FAILED (%lu error%s detected) <<<\n",
               (unsigned long)total_errors, (total_errors == 1) ? "" : "s");
        if (rail_errors > 0)
        {
            printf("   * Power rail errors (GND/VDD shorts): %lu\n", (unsigned long)rail_errors);
        }
        if (continuity_errors > 0)
        {
            printf("   * Missing connections (Open circuits): %lu\n", (unsigned long)continuity_errors);
        }
        if (isolation_errors > 0)
        {
            printf("   * Unexpected shorts (Interconnections): %lu\n", (unsigned long)isolation_errors);
        }
    }
    printf("============================================================\n");
}

/**
  * @brief  Configure all external pins as outputs and continuously blink them
  *         with distinct duty cycles so each external net can be identified on an LED.
  *         Pins sharing the same external net receive the identical duty cycle
  *         to avoid electrical contention / short circuits between them.
  *         Internal pins (label starting with '_') are kept in high-Z analog mode.
  */
static void run_external_pins_signal_generator(void)
{
    /* Keep internal pins safe in high-impedance analog mode */
    set_all_pins_analog();

    /* Find all unique external nets */
    uint8_t ext_net_ids[PIN_COUNT];
    size_t ext_net_count = 0;

    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        if (PINS[i].net_name[0] != '_')
        {
            bool already_added = false;
            for (size_t j = 0; j < ext_net_count; j++)
            {
                if (ext_net_ids[j] == PINS[i].net_id)
                {
                    already_added = true;
                    break;
                }
            }
            if (!already_added)
            {
                ext_net_ids[ext_net_count++] = PINS[i].net_id;
            }
        }
    }

    if (ext_net_count == 0)
    {
        printf("\nNo external pins defined. Halting.\n");
        while (1) {}
    }

    /* Assign on-time (ms) out of 1000 ms period for each unique external net */
    #define CYCLE_PERIOD_MS 1000U
    uint32_t on_time_ms[PIN_COUNT] = {0};
    for (size_t j = 0; j < ext_net_count; j++)
    {
        /* Linearly distribute duty cycle: (j + 1) / (ext_net_count + 1) */
        on_time_ms[j] = ((uint32_t)(j + 1) * CYCLE_PERIOD_MS) / (uint32_t)(ext_net_count + 1);
    }

    /* Configure all external pins as GPIO push-pull output LOW */
    for (size_t i = 0; i < PIN_COUNT; i++)
    {
        if (PINS[i].net_name[0] != '_')
        {
            set_pin_output(i, GPIO_PIN_RESET);
        }
    }

    printf("\n");
    printf("============================================================\n");
    printf(" External Pins LED Signal Generator Active\n");
    printf(" Period: %u ms. Internal pins remain in Analog High-Z mode.\n", (unsigned int)CYCLE_PERIOD_MS);
    printf("------------------------------------------------------------\n");

    for (size_t j = 0; j < ext_net_count; j++)
    {
        uint32_t duty_pct = (on_time_ms[j] * 100U) / CYCLE_PERIOD_MS;
        printf(" Net %u (Duty: %lu%%, %lums ON / %lums OFF):\n",
               (unsigned int)ext_net_ids[j],
               (unsigned long)duty_pct,
               (unsigned long)on_time_ms[j],
               (unsigned long)(CYCLE_PERIOD_MS - on_time_ms[j]));

        for (size_t i = 0; i < PIN_COUNT; i++)
        {
            if (PINS[i].net_id == ext_net_ids[j] && PINS[i].net_name[0] != '_')
            {
                printf("   - %s (%s, %s)\n", PINS[i].name, PINS[i].header_pin, PINS[i].net_name);
            }
        }
    }
    printf("============================================================\n");

    /* Infinite generation loop */
    while (1)
    {
        /* Turn all external pins ON */
        for (size_t i = 0; i < PIN_COUNT; i++)
        {
            if (PINS[i].net_name[0] != '_')
            {
                HAL_GPIO_WritePin(PINS[i].port, PINS[i].pin, GPIO_PIN_SET);
            }
        }

        /* Sequentially turn each net OFF when its on_time expires */
        uint32_t elapsed_ms = 0;
        for (size_t j = 0; j < ext_net_count; j++)
        {
            if (on_time_ms[j] > elapsed_ms)
            {
                delay_ms(on_time_ms[j] - elapsed_ms);
                elapsed_ms = on_time_ms[j];
            }

            for (size_t i = 0; i < PIN_COUNT; i++)
            {
                if (PINS[i].net_id == ext_net_ids[j] && PINS[i].net_name[0] != '_')
                {
                    HAL_GPIO_WritePin(PINS[i].port, PINS[i].pin, GPIO_PIN_RESET);
                }
            }
        }

        /* Wait remainder of cycle */
        if (CYCLE_PERIOD_MS > elapsed_ms)
        {
            delay_ms(CYCLE_PERIOD_MS - elapsed_ms);
        }
    }
}

/**
  * @brief  The application entry point for pin_check.
  *         No generated init code is used: the MCU runs from the reset-default
  *         clock, interrupts are disabled, console output goes via semihosting.
  * @retval int
  */
int main(void)
{
    /* No interrupts at all: SysTick is polled, nothing else is enabled */
    __disable_irq();

    initialise_monitor_handles();
#ifdef __HAL_RCC_GPIOA_CLK_ENABLE
    __HAL_RCC_GPIOA_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOB_CLK_ENABLE
    __HAL_RCC_GPIOB_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOC_CLK_ENABLE
    __HAL_RCC_GPIOC_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOD_CLK_ENABLE
    __HAL_RCC_GPIOD_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOE_CLK_ENABLE
    __HAL_RCC_GPIOE_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOF_CLK_ENABLE
    __HAL_RCC_GPIOF_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOH_CLK_ENABLE
    __HAL_RCC_GPIOH_CLK_ENABLE();
#endif
#ifdef __HAL_RCC_GPIOI_CLK_ENABLE
    __HAL_RCC_GPIOI_CLK_ENABLE();
#endif

    printf("\n");
    printf("############################################################\n");
    printf("# Hardware Pin Check Diagnostic Tool                       #\n");
    printf("############################################################\n");

    /* Run pin diagnostic and report generation */
    run_pin_check(1);

    /* Switch external pins to output and generate distinguishable duty cycle signals */
    run_external_pins_signal_generator();
}

