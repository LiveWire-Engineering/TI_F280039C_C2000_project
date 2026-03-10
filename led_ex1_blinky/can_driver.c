//###########################################################################
//
// FILE:   can_driver.c
//
// TITLE:  CAN Driver for TI F280039C DCAN Module (CANA)
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

//
// Included Files
//
#include "f28x_project.h"
#include "can_driver.h"

//
// DCAN register-level constants
// The F280039C DCAN module uses two interface register sets (IF1 and IF2)
// to access the 32 message objects through an indirect interface.
//

//
// CAN_CTL register bits
//
#define CTL_INIT        0x0001U  // Initialization mode
#define CTL_IE0         0x0002U  // Interrupt line 0 enable
#define CTL_SIE         0x0004U  // Status-change interrupt enable
#define CTL_EIE         0x0008U  // Error interrupt enable
#define CTL_DAR         0x0020U  // Disable automatic retransmission
#define CTL_CCE         0x0040U  // Configuration change enable
#define CTL_TEST        0x0080U  // Test mode enable

//
// CAN_ES register bits
//
#define ES_LEC_MASK     0x0007U  // Last Error Code
#define ES_TXOK         0x0008U  // TX success
#define ES_RXOK         0x0010U  // RX success
#define ES_EPASS        0x0020U  // Error passive
#define ES_EWARN        0x0040U  // Error warning
#define ES_BOFF         0x0080U  // Bus-off

//
// CAN_IFxCMD register bits
//
#define IFCMD_MSG_NUM_M 0x003FU  // Message number mask (bits 0-5)
#define IFCMD_BUSY      0x8000U  // IF busy flag (bit 15)
#define IFCMD_DATA_B    0x0001U  // Access data bytes 4-7 (bit 16 in 32b)
#define IFCMD_DATA_A    0x0002U  // Access data bytes 0-3 (bit 17)
#define IFCMD_TXRQST    0x0004U  // Set TxRqst/NewDat    (bit 18)
#define IFCMD_CLRINTPND 0x0008U  // Clear IntPnd          (bit 19)
#define IFCMD_CONTROL   0x0010U  // Access MsgCtrl        (bit 20)
#define IFCMD_ARB       0x0020U  // Access Arbitration    (bit 21)
#define IFCMD_MASK      0x0040U  // Access Mask           (bit 22)
#define IFCMD_DIR_WR    0x0080U  // Write (1) / Read (0)  (bit 23)

//
// CAN_IFxARB register bits
//
#define IFARB_DIR_TX    0x2000U  // Direction: 1 = TX (bit 29 in 32b word)
#define IFARB_MSGVAL    0x8000U  // Message valid (bit 31 in 32b)
// Standard ID sits in bits [28:18] of the 32-bit ARB register.
// In the upper 16-bit half that maps to bits [12:2].
#define IFARB_STD_ID_SHIFT  2U

//
// CAN_IFxMCTL register bits
//
#define IFMCTL_DLC_M    0x000FU  // DLC mask
#define IFMCTL_EOB      0x0080U  // End of block
#define IFMCTL_TXRQST   0x0100U  // TX request
#define IFMCTL_RMTEN    0x0200U  // Remote enable
#define IFMCTL_RXIE     0x0400U  // RX interrupt enable
#define IFMCTL_TXIE     0x0800U  // TX interrupt enable
#define IFMCTL_UMASK    0x1000U  // Use acceptance mask
#define IFMCTL_INTPND   0x2000U  // Interrupt pending
#define IFMCTL_MSGLST   0x4000U  // Message lost
#define IFMCTL_NEWDAT   0x8000U  // New data

//
// CAN_IFxMSK register bits
// Standard-ID mask sits in bits [28:18] of the 32-bit MASK register.
//
#define IFMSK_STD_SHIFT     18U
#define IFMSK_MDIR          0x40000000UL  // Mask message direction
#define IFMSK_MXTD          0x80000000UL  // Mask extended identifier

//
// CAN module clock: SYSCLK / 2 = 100 MHz (for 200 MHz SYSCLK default)
//
#define CAN_MODULE_FREQ     100000000UL

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

//
// WaitForIF1Ready - Spin until IF1 command register is not busy
//
static void
WaitForIF1Ready(void)
{
    while (CanaRegs.CAN_IF1CMD.all & ((uint32_t)IFCMD_BUSY << 16U))
    {
        // wait
    }
}

//
// WaitForIF2Ready - Spin until IF2 command register is not busy
//
static void
WaitForIF2Ready(void)
{
    while (CanaRegs.CAN_IF2CMD.all & ((uint32_t)IFCMD_BUSY << 16U))
    {
        // wait
    }
}

//
// InvalidateAllMsgObjects - Clear all 32 message objects
//
static void
InvalidateAllMsgObjects(void)
{
    uint16_t i;

    for (i = 1U; i <= 32U; i++)
    {
        WaitForIF1Ready();

        //
        // Clear arbitration (MsgVal = 0) and control registers
        //
        CanaRegs.CAN_IF1ARB.all = 0x00000000UL;
        CanaRegs.CAN_IF1MCTL.all = 0x00000000UL;

        //
        // Write ARB + CONTROL to message object i
        //
        CanaRegs.CAN_IF1CMD.all =
            ((uint32_t)(IFCMD_DIR_WR | IFCMD_ARB | IFCMD_CONTROL) << 16U)
            | (uint32_t)i;
    }

    WaitForIF1Ready();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

//
// CAN_Init - Initialize DCAN module (CANA) and GPIO for J14
//
void
CAN_Init(uint32_t bitrate)
{
    uint32_t brp;

    //
    // 1. Configure GPIO pins for CAN function
    //    GPIO4 = CANTXA, GPIO5 = CANRXA  (mux position 6)
    //    These route to J14 CAN_HI / CAN_LO through the board transceiver.
    //
    EALLOW;
    GPIO_SetupPinMux(CAN_GPIO_TX, GPIO_MUX_CPU1, CAN_GPIO_MUX_VAL);
    GPIO_SetupPinOptions(CAN_GPIO_TX, GPIO_OUTPUT, GPIO_ASYNC);
    GPIO_SetupPinMux(CAN_GPIO_RX, GPIO_MUX_CPU1, CAN_GPIO_MUX_VAL);
    GPIO_SetupPinOptions(CAN_GPIO_RX, GPIO_INPUT, GPIO_ASYNC);
    EDIS;

    //
    // 2. Enter initialization mode
    //
    EALLOW;
    CanaRegs.CAN_CTL.all |= CTL_INIT;

    //
    // Enable configuration change
    //
    CanaRegs.CAN_CTL.all |= CTL_CCE;

    //
    // 3. Set bit-timing register
    //    Assumptions:
    //      - CAN module clock = SYSCLK/2 = 100 MHz
    //      - Desired time quanta per bit = 10
    //        (Sync=1, TSeg1=6, TSeg2=3 -> sample point at 70%)
    //      - BRP = (CAN_MODULE_FREQ / (bitrate * Tq_per_bit)) - 1
    //
    //    CAN_BTR layout (32-bit):
    //      [5:0]   BRP   (Baud Rate Prescaler)
    //      [7:6]   SJW   (Synchronization Jump Width, 0-based)
    //      [11:8]  TSeg1  (0-based, so 6-1=5)
    //      [14:12] TSeg2  (0-based, so 3-1=2)
    //
    {
        uint32_t tq_per_bit = 10UL;
        uint32_t sjw  = 0UL;   // SJW = 1 Tq (0-based)
        uint32_t tseg1 = 5UL;  // TSeg1 = 6 Tq (0-based)
        uint32_t tseg2 = 2UL;  // TSeg2 = 3 Tq (0-based)

        brp = (CAN_MODULE_FREQ / (bitrate * tq_per_bit)) - 1UL;

        CanaRegs.CAN_BTR.all = (brp & 0x3FUL)
                              | ((sjw & 0x03UL) << 6U)
                              | ((tseg1 & 0x0FUL) << 8U)
                              | ((tseg2 & 0x07UL) << 12U);
    }

    //
    // 4. Invalidate all message objects
    //
    InvalidateAllMsgObjects();

    //
    // 5. Leave initialization mode
    //
    CanaRegs.CAN_CTL.all &= ~(CTL_CCE | CTL_INIT);
    EDIS;

    //
    // Wait until the module exits Init mode (Init bit clears)
    //
    while (CanaRegs.CAN_CTL.all & CTL_INIT)
    {
        // wait
    }
}

//
// CAN_ConfigTxMailbox - Set up a message object for transmitting
//
int16_t
CAN_ConfigTxMailbox(uint16_t mboxNum, uint32_t msgId, uint16_t dlc)
{
    if ((mboxNum < 1U) || (mboxNum > 32U) || (dlc > 8U))
    {
        return CAN_ERR_INVALID;
    }

    WaitForIF1Ready();

    //
    // Arbitration: MsgVal=1, Dir=TX, Standard ID
    //
    CanaRegs.CAN_IF1ARB.all =
        ((uint32_t)IFARB_MSGVAL << 16U)
        | ((uint32_t)IFARB_DIR_TX << 16U)
        | ((msgId & 0x7FFUL) << (IFARB_STD_ID_SHIFT + 16U));

    //
    // Message control: DLC, end-of-block, TX interrupt enable
    //
    CanaRegs.CAN_IF1MCTL.all = (dlc & IFMCTL_DLC_M)
                               | IFMCTL_EOB
                               | IFMCTL_TXIE;

    //
    // Write ARB + CONTROL to the message object
    //
    CanaRegs.CAN_IF1CMD.all =
        ((uint32_t)(IFCMD_DIR_WR | IFCMD_ARB | IFCMD_CONTROL) << 16U)
        | (uint32_t)mboxNum;

    WaitForIF1Ready();
    return CAN_OK;
}

//
// CAN_ConfigRxMailbox - Set up a message object for receiving
//
int16_t
CAN_ConfigRxMailbox(uint16_t mboxNum, uint32_t msgId, uint32_t mask)
{
    if ((mboxNum < 1U) || (mboxNum > 32U))
    {
        return CAN_ERR_INVALID;
    }

    WaitForIF2Ready();

    //
    // Acceptance mask: standard ID portion, plus mask direction & XTD bits
    //
    CanaRegs.CAN_IF2MSK.all = ((mask & 0x7FFUL) << IFMSK_STD_SHIFT)
                              | IFMSK_MDIR
                              | IFMSK_MXTD;

    //
    // Arbitration: MsgVal=1, Dir=RX (Dir bit = 0), Standard ID
    //
    CanaRegs.CAN_IF2ARB.all =
        ((uint32_t)IFARB_MSGVAL << 16U)
        | ((msgId & 0x7FFUL) << (IFARB_STD_ID_SHIFT + 16U));

    //
    // Message control: DLC=8, EOB, use mask, RX interrupt enable
    //
    CanaRegs.CAN_IF2MCTL.all = 8U  // Accept up to 8 bytes
                               | IFMCTL_EOB
                               | IFMCTL_UMASK
                               | IFMCTL_RXIE;

    //
    // Write MASK + ARB + CONTROL to the message object
    //
    CanaRegs.CAN_IF2CMD.all =
        ((uint32_t)(IFCMD_DIR_WR | IFCMD_MASK | IFCMD_ARB
                    | IFCMD_CONTROL) << 16U)
        | (uint32_t)mboxNum;

    WaitForIF2Ready();
    return CAN_OK;
}

//
// CAN_Transmit - Write data and request transmission
//
int16_t
CAN_Transmit(uint16_t mboxNum, uint32_t msgId,
             const uint16_t *data, uint16_t dlc)
{
    uint32_t dataA, dataB;

    if ((mboxNum < 1U) || (mboxNum > 32U) || (dlc > 8U) || (data == 0))
    {
        return CAN_ERR_INVALID;
    }

    //
    // Check that IF1 is not busy
    //
    if (CanaRegs.CAN_IF1CMD.all & ((uint32_t)IFCMD_BUSY << 16U))
    {
        return CAN_ERR_BUSY;
    }

    //
    // Pack data bytes into 32-bit words for IF1DATA / IF1DATB
    // CAN data is byte-oriented; on C2000 each uint16_t holds one byte.
    // IF1DATA = byte0 | (byte1<<8) | (byte2<<16) | (byte3<<24)
    // IF1DATB = byte4 | (byte5<<8) | (byte6<<16) | (byte7<<24)
    //
    dataA = 0UL;
    dataB = 0UL;

    if (dlc >= 1U) { dataA |= ((uint32_t)data[0] & 0xFFUL); }
    if (dlc >= 2U) { dataA |= ((uint32_t)data[1] & 0xFFUL) << 8U; }
    if (dlc >= 3U) { dataA |= ((uint32_t)data[2] & 0xFFUL) << 16U; }
    if (dlc >= 4U) { dataA |= ((uint32_t)data[3] & 0xFFUL) << 24U; }
    if (dlc >= 5U) { dataB |= ((uint32_t)data[4] & 0xFFUL); }
    if (dlc >= 6U) { dataB |= ((uint32_t)data[5] & 0xFFUL) << 8U; }
    if (dlc >= 7U) { dataB |= ((uint32_t)data[6] & 0xFFUL) << 16U; }
    if (dlc >= 8U) { dataB |= ((uint32_t)data[7] & 0xFFUL) << 24U; }

    CanaRegs.CAN_IF1DATA.all = dataA;
    CanaRegs.CAN_IF1DATB.all = dataB;

    //
    // Arbitration: MsgVal=1, Dir=TX, Standard ID
    //
    CanaRegs.CAN_IF1ARB.all =
        ((uint32_t)IFARB_MSGVAL << 16U)
        | ((uint32_t)IFARB_DIR_TX << 16U)
        | ((msgId & 0x7FFUL) << (IFARB_STD_ID_SHIFT + 16U));

    //
    // Message control: DLC, EOB, set TxRqst to trigger send
    //
    CanaRegs.CAN_IF1MCTL.all = (dlc & IFMCTL_DLC_M)
                               | IFMCTL_EOB
                               | IFMCTL_TXRQST
                               | IFMCTL_TXIE;

    //
    // Transfer everything (ARB + CTRL + DATA_A + DATA_B + TXRQST)
    // to the message object and request transmission
    //
    CanaRegs.CAN_IF1CMD.all =
        ((uint32_t)(IFCMD_DIR_WR | IFCMD_ARB | IFCMD_CONTROL
                    | IFCMD_DATA_A | IFCMD_DATA_B | IFCMD_TXRQST) << 16U)
        | (uint32_t)mboxNum;

    return CAN_OK;
}

//
// CAN_Receive - Read a received message from a mailbox
//
int16_t
CAN_Receive(uint16_t mboxNum, uint32_t *msgId,
            uint16_t *data, uint16_t *dlc)
{
    uint32_t arbVal, mctlVal, dataA, dataB;

    if ((mboxNum < 1U) || (mboxNum > 32U))
    {
        return CAN_ERR_INVALID;
    }

    //
    // Check NewDat via CAN_NDAT registers (one bit per message object).
    // Objects 1-16 are in CAN_NDAT_21, objects 17-32 in CAN_NDAT_22.
    //
    {
        uint32_t ndatVal;

        if (mboxNum <= 16U)
        {
            ndatVal = CanaRegs.CAN_NDAT_21.all;
        }
        else
        {
            ndatVal = CanaRegs.CAN_NDAT_22.all;
        }

        if ((ndatVal & (1UL << ((mboxNum - 1U) % 16U))) == 0UL)
        {
            return CAN_ERR_NO_MSG;
        }
    }

    WaitForIF2Ready();

    //
    // Request read of ARB + CONTROL + DATA_A + DATA_B from the msg object.
    // Clear IntPnd and NewDat on read.
    //
    CanaRegs.CAN_IF2CMD.all =
        ((uint32_t)(IFCMD_ARB | IFCMD_CONTROL | IFCMD_DATA_A
                    | IFCMD_DATA_B | IFCMD_CLRINTPND | IFCMD_TXRQST) << 16U)
        | (uint32_t)mboxNum;

    WaitForIF2Ready();

    //
    // Extract message ID (standard, bits [28:18] of ARB register)
    //
    arbVal = CanaRegs.CAN_IF2ARB.all;
    if (msgId != 0)
    {
        *msgId = (arbVal >> 18U) & 0x7FFUL;
    }

    //
    // Extract DLC
    //
    mctlVal = CanaRegs.CAN_IF2MCTL.all;
    if (dlc != 0)
    {
        *dlc = (uint16_t)(mctlVal & IFMCTL_DLC_M);
    }

    //
    // Unpack data bytes
    //
    dataA = CanaRegs.CAN_IF2DATA.all;
    dataB = CanaRegs.CAN_IF2DATB.all;

    if (data != 0)
    {
        data[0] = (uint16_t)(dataA & 0xFFUL);
        data[1] = (uint16_t)((dataA >> 8U) & 0xFFUL);
        data[2] = (uint16_t)((dataA >> 16U) & 0xFFUL);
        data[3] = (uint16_t)((dataA >> 24U) & 0xFFUL);
        data[4] = (uint16_t)(dataB & 0xFFUL);
        data[5] = (uint16_t)((dataB >> 8U) & 0xFFUL);
        data[6] = (uint16_t)((dataB >> 16U) & 0xFFUL);
        data[7] = (uint16_t)((dataB >> 24U) & 0xFFUL);
    }

    return CAN_OK;
}

//
// CAN_SetupInterrupt - Enable CANA interrupt line 0 in PIE
//
void
CAN_SetupInterrupt(void)
{
    EALLOW;

    //
    // Enable interrupt line 0 in DCAN module + status/error interrupts
    //
    CanaRegs.CAN_CTL.all |= (CTL_IE0 | CTL_SIE | CTL_EIE);

    //
    // Enable PIE group 9 (CAN interrupts are in group 9)
    //
    PieCtrlRegs.PIEIER9.bit.INTx5 = 1;   // CANA0 = PIE 9.5

    //
    // Enable CPU interrupt group 9
    //
    IER |= M_INT9;

    EDIS;
}

//
// CAN_GetErrorStatus - Return the CAN_ES register value
//
uint32_t
CAN_GetErrorStatus(void)
{
    return CanaRegs.CAN_ES.all;
}

//
// End of File
//
