/******************************************************************************
 * @file    pwm_driver.h
 * @brief   ePWM driver for TMS320F280039C
 *
 * Target:  TMS320F280039C
 * Pins:    Configurable — ePWMxA / ePWMxB (e.g., GPIO 0/1 for ePWM1)
 *
 * Dependencies:
 *   - f28003x_device.h (register definitions)
 *   - InitSysCtrl() must be called before pwm_driver_init()
 *
 * TODO: Implement ePWM time-base, counter-compare, action-qualifier,
 *       dead-band, and trip-zone configuration.
 *****************************************************************************/
#ifndef PWM_DRIVER_H
#define PWM_DRIVER_H

#include "f28003x_device.h"

// ---------- Configuration ----------
#define PWM_CLK_DIV_1   0
#define PWM_CLK_DIV_2   1
#define PWM_CLK_DIV_4   2

// ---------- Types ----------
typedef enum {
    PWM_MODULE_1 = 1,
    PWM_MODULE_2 = 2,
    PWM_MODULE_3 = 3,
    PWM_MODULE_4 = 4,
    PWM_MODULE_5 = 5,
    PWM_MODULE_6 = 6,
    PWM_MODULE_7 = 7,
    PWM_MODULE_8 = 8
} pwm_module_t;

typedef enum {
    PWM_COUNT_UP      = 0,
    PWM_COUNT_DOWN    = 1,
    PWM_COUNT_UP_DOWN = 2
} pwm_count_mode_t;

// ---------- Public API ----------

/**
 * @brief Initialize an ePWM module.
 * @param module      ePWM module number (1–8).
 * @param period      Time-base period (TBPRD register value).
 * @param count_mode  Up, down, or up-down counting.
 */
void pwm_driver_init(pwm_module_t module, uint16_t period,
                     pwm_count_mode_t count_mode);

/**
 * @brief Set the duty cycle for ePWMxA output.
 * @param module  ePWM module number.
 * @param duty    Compare value (CMPA) — duty = CMPA / TBPRD.
 */
void pwm_driver_set_duty_a(pwm_module_t module, uint16_t duty);

/**
 * @brief Set the duty cycle for ePWMxB output.
 * @param module  ePWM module number.
 * @param duty    Compare value (CMPB).
 */
void pwm_driver_set_duty_b(pwm_module_t module, uint16_t duty);

#endif // PWM_DRIVER_H
