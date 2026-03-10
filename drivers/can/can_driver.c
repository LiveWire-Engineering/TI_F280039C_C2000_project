/******************************************************************************
 * @file    can_driver.c
 * @brief   CAN / MCAN bus driver for TMS320F280039C — stub implementation
 *
 * Target:  TMS320F280039C
 * Pins:    GPIO 32 (CANTXA), GPIO 33 (CANRXA)
 *
 * Dependencies:
 *   - f28003x_device.h
 *   - InitSysCtrl() must be called before can_driver_init()
 *****************************************************************************/
#include "can_driver.h"

void can_driver_init(void)
{
    // TODO: Configure GPIO 32/33 for CAN TX/RX
    // TODO: Enable CAN peripheral clock
    // TODO: Set bit-timing registers for desired baud rate
    // TODO: Initialize mailboxes / message objects
    // TODO: Enable CAN module
}

void can_driver_send(uint32_t msg_id, const uint8_t *data, uint8_t len)
{
    // TODO: Load message object with ID, data, and DLC
    // TODO: Trigger transmission
    // TODO: Wait for TX complete or return status
    (void)msg_id;
    (void)data;
    (void)len;
}

uint16_t can_driver_receive(uint32_t *msg_id, uint8_t *data, uint8_t *len)
{
    // TODO: Check for new message in receive mailbox
    // TODO: Read message ID, data, and DLC
    // TODO: Clear new-data flag
    (void)msg_id;
    (void)data;
    (void)len;
    return 0;
}
