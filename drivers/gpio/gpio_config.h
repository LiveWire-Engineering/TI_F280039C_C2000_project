/******************************************************************************
 * @file    gpio_config.h
 * @brief   GPIO configuration helpers for TMS320F280039C
 *
 * Target:  TMS320F280039C
 * Pins:    Any GPIO (0–59 on this device)
 *
 * Dependencies:
 *   - f28003x_device.h (register definitions)
 *   - InitSysCtrl() must be called before gpio_config_init()
 *
 * This module provides a higher-level API over the low-level GPIO register
 * functions in f28003x_gpio.c. Use it to configure pins for specific
 * peripheral functions (CAN, SPI, SCI, etc.) or as digital I/O.
 *
 * TODO: Implement pin configuration, read, write, and toggle helpers.
 *****************************************************************************/
#ifndef GPIO_CONFIG_H
#define GPIO_CONFIG_H

#include "f28003x_device.h"

// ---------- Types ----------
typedef enum {
    GPIO_DIR_INPUT  = 0,
    GPIO_DIR_OUTPUT = 1
} gpio_dir_t;

typedef enum {
    GPIO_PULLUP_DISABLE = 0,
    GPIO_PULLUP_ENABLE  = 1
} gpio_pullup_t;

// ---------- Public API ----------

/**
 * @brief Initialize all GPIO to a safe default state.
 *        Typically called once at startup before individual pin configuration.
 */
void gpio_config_init(void);

/**
 * @brief Configure a single GPIO pin.
 * @param pin      GPIO number (0–59).
 * @param dir      Input or output.
 * @param pullup   Enable or disable internal pull-up.
 * @param mux_val  Peripheral mux value (0 = GPIO, 1–3 = peripheral function).
 */
void gpio_config_pin(uint16_t pin, gpio_dir_t dir, gpio_pullup_t pullup,
                     uint16_t mux_val);

/**
 * @brief Write a value to a GPIO output pin.
 * @param pin    GPIO number.
 * @param value  0 = low, 1 = high.
 */
void gpio_config_write(uint16_t pin, uint16_t value);

/**
 * @brief Read the current state of a GPIO pin.
 * @param pin  GPIO number.
 * @return 0 or 1.
 */
uint16_t gpio_config_read(uint16_t pin);

/**
 * @brief Toggle a GPIO output pin.
 * @param pin  GPIO number.
 */
void gpio_config_toggle(uint16_t pin);

#endif // GPIO_CONFIG_H
