# CAN Driver – DBC Integration Guide

## Overview

This document describes how CAN messages are defined, generated, and
transmitted/received on the TI F280039C ECU through connector **J14**
(`CAN_HI` / `CAN_LO`).

The workflow has two stages:

1. **Design-time** – A standard **Vector DBC file** describes every CAN
   message and signal on the bus.  A Python code-generator converts the
   DBC into C source files.
2. **Run-time** – The generated message structs and pack/unpack helpers
   are used together with a low-level **CAN driver** that talks to the
   DCAN peripheral on the F280039C.

```
┌──────────────┐      ┌──────────────┐      ┌──────────────────────┐
│  .dbc file   │─────>│ dbc_to_c.py  │─────>│ can_message.h/.c     │
│ (bus design) │      │ (generator)  │      │ (IDs, structs,       │
└──────────────┘      └──────────────┘      │  pack/unpack)        │
                                            └──────────┬───────────┘
                                                       │ #include
                                            ┌──────────▼───────────┐
                                            │ can_driver.h/.c      │
                                            │ (HW init, TX, RX)    │
                                            └──────────┬───────────┘
                                                       │ CanaRegs
                                            ┌──────────▼───────────┐
                                            │  DCAN peripheral     │
                                            │  GPIO4/5 → J14      │
                                            │  CAN_HI / CAN_LO    │
                                            └──────────────────────┘
```

---

## 1  DBC File Format

The **DBC file** (`dbc/ecu_example.dbc`) uses the industry-standard
Vector DBC format.  Key sections:

| DBC keyword | Purpose | Example |
|---|---|---|
| `BU_` | Bus nodes (ECUs) | `BU_: ECU Tester` |
| `BO_` | Message definition | `BO_ 256 EngineData: 8 ECU` |
| `SG_` | Signal inside a message | `SG_ EngineSpeed : 0\|16@1+ (1,0) [0\|8000] "rpm" Tester` |
| `CM_` | Comment / description | `CM_ BO_ 256 "Engine data";` |
| `BA_` | Attribute (e.g. cycle time) | `BA_ "GenMsgCycleTime" BO_ 256 100;` |

### Signal encoding

```
SG_ <name> : <start_bit>|<length>@<byte_order><sign> (<factor>,<offset>) [<min>|<max>] "<unit>" <receivers>
```

- **byte_order**: `1` = little-endian (Intel), `0` = big-endian (Motorola)
- **sign**: `+` = unsigned, `-` = signed
- **Physical value** = raw × factor + offset

---

## 2  Code Generation

### Running the generator

```bash
python3 tools/dbc_to_c.py dbc/ecu_example.dbc \
        --outdir led_ex1_blinky \
        --node ECU
```

`--node ECU` tells the tool which DBC node *this* firmware represents.
Messages whose **sender** equals the node are classified as **TX**;
all others are **RX**.

### Generated output

| File | Contents |
|---|---|
| `can_message.h` | `#define` for every message ID, DLC, and cycle time; `typedef struct` for each message's signals; prototypes for `CAN_Pack_*` and `CAN_Unpack_*` functions |
| `can_message.c` | Bit-level pack (struct → raw bytes) and unpack (raw bytes → struct) implementations for every message |

**Re-run the generator every time the DBC file changes.** The header
comment in each generated file includes a warning not to edit manually.

### How pack/unpack work

For a **little-endian** signal the generator emits code that shifts and
masks each signal into/out of a `uint16_t data[8]` array (one byte per
element, matching the CAN driver interface):

```c
// Pack EngineSpeed (start=0, len=16) into data[]
data[0] |= ((uint16_t)msg->EngineSpeed & 0xFF) << 0;
data[1] |= ((uint16_t)msg->EngineSpeed >> 8)    & 0xFF;
```

The physical-value conversion (factor/offset) is **not** baked into the
generated code.  Apply it in application logic:

```c
float speed_rpm = (float)engineData.EngineSpeed * 1.0f + 0.0f;
```

---

## 3  CAN Driver (Hardware Layer)

### Files

| File | Purpose |
|---|---|
| `can_driver.h` | Public API and configuration constants |
| `can_driver.c` | DCAN register-level implementation |

### GPIO / J14 mapping

| GPIO | Function | Mux | Connector |
|---|---|---|---|
| GPIO4 | CANTXA (transmit) | 6 | J14 CAN_HI/CAN_LO via transceiver |
| GPIO5 | CANRXA (receive)  | 6 | J14 CAN_HI/CAN_LO via transceiver |

### Initialisation

```c
CAN_Init(CAN_BITRATE_500K);
```

This:
1. Muxes GPIO4/5 to the CANA function
2. Puts DCAN into Init mode and sets bit timing (500 kbit/s, 70 % sample point)
3. Invalidates all 32 message objects
4. Exits Init mode

### Configuring mailboxes

```c
// TX mailbox 1 for EngineData (ID 0x100, 8 bytes)
CAN_ConfigTxMailbox(1, CAN_ID_ENGINE_DATA, CAN_DLC_ENGINE_DATA);

// RX mailbox 9 for TesterCommand (ID 0x400, exact match)
CAN_ConfigRxMailbox(9, CAN_ID_TESTER_COMMAND, 0x7FF);
```

### Sending a message

```c
CAN_MSG_ENGINE_DATA_t engineMsg;
uint16_t raw[8];

engineMsg.EngineSpeed  = 3500;
engineMsg.EngineTemp   = 90 + 40;   // offset = -40 in DBC
engineMsg.ThrottlePos  = 128;       // ~50 %
engineMsg.EngineStatus = 2;         // Running

CAN_Pack_EngineData(raw, &engineMsg);
CAN_Transmit(1, CAN_ID_ENGINE_DATA, raw, CAN_DLC_ENGINE_DATA);
```

### Receiving a message

```c
uint32_t rxId;
uint16_t rxData[8];
uint16_t rxDlc;

if (CAN_Receive(9, &rxId, rxData, &rxDlc) == CAN_OK)
{
    CAN_MSG_TESTER_COMMAND_t cmd;
    CAN_Unpack_TesterCommand(&cmd, rxData);
    // process cmd.CommandType, cmd.CommandData, ...
}
```

### Interrupts

Call `CAN_SetupInterrupt()` after `CAN_Init()` to enable CANA interrupt
line 0 (PIE 9.5).  Implement your ISR by replacing the default
`CANA0_ISR` stub in `f28003x_defaultisr.c` or by re-mapping the PIE
vector:

```c
PieVectTable.CANA0_INT = &MyCAN_ISR;
```

---

## 4  Adding or Changing Messages

1. Edit `dbc/ecu_example.dbc` (or replace it with your project DBC).
2. Re-run the code generator:
   ```bash
   python3 tools/dbc_to_c.py dbc/ecu_example.dbc --outdir led_ex1_blinky --node ECU
   ```
3. Update mailbox configuration in your application to match the new
   message set.
4. Rebuild the firmware in Code Composer Studio.

---

## 5  File Summary

```
dbc/
  ecu_example.dbc          ← DBC bus definition (edit this)

tools/
  dbc_to_c.py              ← DBC → C code generator

led_ex1_blinky/
  can_driver.h             ← CAN driver public API
  can_driver.c             ← CAN driver implementation (DCAN + GPIO)
  can_message.h            ← [generated] message IDs, structs, prototypes
  can_message.c            ← [generated] pack / unpack functions

docs/
  CAN_DBC_INTEGRATION.md   ← this document
```
