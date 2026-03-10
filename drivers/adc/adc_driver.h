/******************************************************************************
 * @file    adc_driver.h
 * @brief   ADC sampling driver for TMS320F280039C
 *
 * Target:  TMS320F280039C
 * Pins:    Configurable — ADCINA0–A5, ADCINB0–B5, ADCINC0–C5
 *
 * Dependencies:
 *   - f28003x_device.h (register definitions)
 *   - InitSysCtrl() must be called before adc_driver_init()
 *
 * TODO: Implement multi-channel ADC configuration, SOC triggering,
 *       result reading, and optional calibration.
 *****************************************************************************/
#ifndef ADC_DRIVER_H
#define ADC_DRIVER_H

#include "f28003x_device.h"

// ---------- Configuration ----------
#define ADC_RESOLUTION_12BIT    0
#define ADC_SIGNAL_MODE_SINGLE  0

// ---------- Types ----------
typedef enum {
    ADC_MODULE_A = 0,
    ADC_MODULE_B = 1,
    ADC_MODULE_C = 2
} adc_module_t;

// ---------- Public API ----------

/**
 * @brief Initialize the ADC subsystem.
 *        Configures clock prescaler, resolution, and signal mode.
 * @param module  Which ADC module to initialize (A, B, or C).
 */
void adc_driver_init(adc_module_t module);

/**
 * @brief Configure a Start-of-Conversion (SOC) channel.
 * @param module   ADC module (A, B, or C).
 * @param soc_num  SOC number (0–15).
 * @param channel  Input channel number.
 * @param acq_window  Acquisition window duration in SYSCLK cycles.
 */
void adc_driver_configure_soc(adc_module_t module, uint16_t soc_num,
                               uint16_t channel, uint16_t acq_window);

/**
 * @brief Trigger a software-initiated conversion and return the result.
 * @param module   ADC module (A, B, or C).
 * @param soc_num  SOC number to trigger.
 * @return 12-bit conversion result.
 */
uint16_t adc_driver_read(adc_module_t module, uint16_t soc_num);

#endif // ADC_DRIVER_H
