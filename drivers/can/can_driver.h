/******************************************************************************
 * @file    can_driver.h
 * @brief   CAN / MCAN bus driver for TMS320F280039C
 *
 * Target:  TMS320F280039C
 * Pins:    GPIO 32 (CANTXA), GPIO 33 (CANRXA)  [default LaunchPad assignment]
 *
 * Dependencies:
 *   - f28003x_device.h (register definitions)
 *   - InitSysCtrl() must be called before can_driver_init()
 *
 * TODO: Implement CAN 2.0B / CAN-FD initialization, TX, RX, and filtering.
 *****************************************************************************/
#ifndef CAN_DRIVER_H
#define CAN_DRIVER_H

#include "f28003x_device.h"

// ---------- Configuration ----------
#define CAN_BAUD_500K   500000U
#define CAN_BAUD_250K   250000U
#define CAN_BAUD_1M     1000000U

// ---------- Public API ----------

/**
 * @brief Initialize the CAN peripheral.
 *        Configures GPIO pins, bit timing, and enables the module.
 */
void can_driver_init(void);

/**
 * @brief Transmit a CAN message.
 * @param msg_id  Standard or extended message ID.
 * @param data    Pointer to payload bytes (up to 8 for CAN 2.0).
 * @param len     Number of payload bytes (0–8).
 */
void can_driver_send(uint32_t msg_id, const uint8_t *data, uint8_t len);

/**
 * @brief Check for and receive a CAN message.
 * @param msg_id  [out] Received message ID.
 * @param data    [out] Buffer for payload bytes (must be >= 8 bytes).
 * @param len     [out] Number of received payload bytes.
 * @return 1 if a message was received, 0 if no message available.
 */
uint16_t can_driver_receive(uint32_t *msg_id, uint8_t *data, uint8_t *len);

#endif // CAN_DRIVER_H
