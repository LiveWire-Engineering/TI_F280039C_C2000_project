//###########################################################################
//
// FILE:   can_driver.h
//
// TITLE:  CAN Driver for TI F280039C DCAN Module (CANA)
//
//! \addtogroup can_driver
//! <h1> CAN Driver </h1>
//!
//! This module provides initialization, transmit, and receive functions for
//! the DCAN peripheral (CANA) on the TI F280039C.  The physical interface
//! is exposed on connector J14 (CAN_HI / CAN_LO) through an external CAN
//! transceiver.
//!
//! GPIO mapping (active by default):
//!  - GPIO4  -> CANTXA  (CAN Transmit, mux position 6)
//!  - GPIO5  -> CANRXA  (CAN Receive,  mux position 6)
//!
//! The driver uses the DCAN interface registers (IF1 for TX, IF2 for RX)
//! and supports up to 32 message objects.
//!
//! Message definitions (IDs, DLCs, pack/unpack helpers) are auto-generated
//! from a DBC file by tools/dbc_to_c.py and live in can_message.h/.c.
//!
//! \b External \b Connections \n
//!  - J14 CAN_HI and CAN_LO (via CAN transceiver)
//!
//
//###########################################################################
//
// $Copyright:
// Copyright (C) 2025 Texas Instruments Incorporated - http://www.ti.com/
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
//
//   Redistributions of source code must retain the above copyright
//   notice, this list of conditions and the following disclaimer.
//
//   Redistributions in binary form must reproduce the above copyright
//   notice, this list of conditions and the following disclaimer in the
//   documentation and/or other materials provided with the
//   distribution.
//
//   Neither the name of Texas Instruments Incorporated nor the names of
//   its contributors may be used to endorse or promote products derived
//   from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
// $
//###########################################################################

#ifndef CAN_DRIVER_H_
#define CAN_DRIVER_H_

#include <stdint.h>

//
// CAN bit-rate selection (bit/s).  The driver computes BTR values assuming
// a 200 MHz SYSCLK (default for F280039C) and a CAN module clock of
// SYSCLK/2 = 100 MHz.
//
#define CAN_BITRATE_500K    500000UL
#define CAN_BITRATE_250K    250000UL
#define CAN_BITRATE_125K    125000UL
#define CAN_BITRATE_1M      1000000UL

//
// GPIO pins for CAN on J14 connector
//
#define CAN_GPIO_TX         4U      // GPIO4 = CANTXA
#define CAN_GPIO_RX         5U      // GPIO5 = CANRXA
#define CAN_GPIO_MUX_VAL    6U      // Mux position for CANA function

//
// DCAN message object numbers (1-32).
// Objects 1..CAN_TX_MBOX_COUNT are reserved for transmit.
// Objects (CAN_TX_MBOX_COUNT+1)..32 are available for receive filters.
//
#define CAN_TX_MBOX_START   1U
#define CAN_TX_MBOX_COUNT   8U
#define CAN_RX_MBOX_START   (CAN_TX_MBOX_START + CAN_TX_MBOX_COUNT)
#define CAN_RX_MBOX_COUNT   24U

//
// Return codes
//
#define CAN_OK              0
#define CAN_ERR_BUSY        (-1)
#define CAN_ERR_INVALID     (-2)
#define CAN_ERR_NO_MSG      (-3)

//
// CAN_Init - Initialize the DCAN module (CANA)
//
// This function:
//  1. Configures GPIO4/GPIO5 for CANTXA/CANRXA (J14 CAN_HI / CAN_LO)
//  2. Places the DCAN module into initialization mode
//  3. Sets the bit-timing register for the requested bit rate
//  4. Invalidates all 32 message objects
//  5. Exits initialization mode and waits for the module to synchronize
//
// Parameters:
//   bitrate - One of the CAN_BITRATE_* defines (e.g. CAN_BITRATE_500K)
//
void CAN_Init(uint32_t bitrate);

//
// CAN_ConfigTxMailbox - Configure a message object for transmission
//
// Parameters:
//   mboxNum - Message object number (1-32)
//   msgId   - Standard 11-bit CAN identifier (0x000-0x7FF)
//   dlc     - Data length code (0-8 bytes)
//
// Returns:
//   CAN_OK on success, CAN_ERR_INVALID if parameters are out of range
//
int16_t CAN_ConfigTxMailbox(uint16_t mboxNum, uint32_t msgId, uint16_t dlc);

//
// CAN_ConfigRxMailbox - Configure a message object for reception
//
// The acceptance mask is applied so that only messages matching msgId
// (after masking) are stored in this mailbox.  Set mask = 0x7FF for
// exact-match filtering.
//
// Parameters:
//   mboxNum - Message object number (1-32)
//   msgId   - Standard 11-bit CAN identifier to accept
//   mask    - Acceptance mask (0x7FF = exact match)
//
// Returns:
//   CAN_OK on success, CAN_ERR_INVALID if parameters are out of range
//
int16_t CAN_ConfigRxMailbox(uint16_t mboxNum, uint32_t msgId, uint32_t mask);

//
// CAN_Transmit - Send a CAN message
//
// Copies up to 8 data bytes into the message object and triggers
// a transmission request via IF1.
//
// Parameters:
//   mboxNum - TX message object number that was previously configured
//   msgId   - Standard 11-bit CAN identifier
//   data    - Pointer to data bytes (array of uint16_t, one byte per element)
//   dlc     - Number of data bytes to send (0-8)
//
// Returns:
//   CAN_OK on success, CAN_ERR_BUSY if the IF1 register is still busy
//
int16_t CAN_Transmit(uint16_t mboxNum, uint32_t msgId,
                     const uint16_t *data, uint16_t dlc);

//
// CAN_Receive - Read a received CAN message from a mailbox
//
// If a new message is pending in the specified mailbox, the data is
// copied out and the NewDat flag is cleared.
//
// Parameters:
//   mboxNum - RX message object number that was previously configured
//   msgId   - Pointer to store the received message ID
//   data    - Pointer to buffer for received data bytes (min 8 elements)
//   dlc     - Pointer to store the received DLC
//
// Returns:
//   CAN_OK if a new message was read, CAN_ERR_NO_MSG if no new data
//
int16_t CAN_Receive(uint16_t mboxNum, uint32_t *msgId,
                    uint16_t *data, uint16_t *dlc);

//
// CAN_SetupInterrupt - Enable CAN interrupts in PIE
//
// Enables CANA interrupt line 0 (PIE group 9, channel 5) and
// registers the provided ISR function pointer.
//
// Parameters:
//   None (uses default CANA0 ISR location in PIE)
//
void CAN_SetupInterrupt(void);

//
// CAN_GetErrorStatus - Read the CAN error / status register
//
// Returns the raw value of the CAN_ES register which contains:
//  - Bus-off status
//  - Error warning status
//  - Last error code
//  - TX/RX error counters (via CAN_ERRC)
//
uint32_t CAN_GetErrorStatus(void);

#endif // CAN_DRIVER_H_
