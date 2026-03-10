# Architecture

This document describes the firmware architecture for the TMS320F280039C
platform. It is intended as a reference for both human developers and AI agents
working on the codebase.

## System Overview

```
┌──────────────────────────────────────────────────────────┐
│                     Application Layer                     │
│  (led_ex1_blinky.c  ·  future main.c)                   │
├──────────────────────────────────────────────────────────┤
│                      Driver Layer                         │
│  drivers/gpio/  ·  drivers/adc/  ·  drivers/can/  · …   │
├──────────────────────────────────────────────────────────┤
│                 Device Support Layer                       │
│  f28003x_sysctrl.c  ·  f28003x_gpio.c  ·  f28003x_*.c  │
│  f28003x_headers_nonBIOS.cmd  (register map)             │
├──────────────────────────────────────────────────────────┤
│                       Hardware                            │
│  TMS320F280039C  (C28x core @ 120 MHz + CLA)            │
└──────────────────────────────────────────────────────────┘
```

### Layer Responsibilities

| Layer | Purpose |
|---|---|
| **Application** | Main loop, task scheduling, high-level logic. Calls driver init functions then enters a run loop. |
| **Driver** | Peripheral-specific configuration and runtime API. Each driver is a self-contained module under `drivers/`. |
| **Device Support** | Low-level register definitions, system clock init, interrupt vector setup. Provided by TI C2000Ware and committed to the repo for reproducibility. |

## Memory Map

The F280039C has two execution modes configured via linker command files:

| Mode | Linker File | Use Case |
|---|---|---|
| RAM | `28003x_generic_ram_lnk.cmd` | Development — fast load, no flash wear |
| Flash | `28003x_generic_flash_lnk.cmd` | Production — persistent across resets |

### Key Memory Regions

| Region | Address Range | Size | Usage |
|---|---|---|---|
| RAMM0 | 0x000122 – 0x0002FF | 478 B | Boot / codestart |
| RAMM1 | 0x000300 – 0x0003FF | 256 B | Stack |
| RAMLS0-4 | 0x008000 – 0x009FFF | 10 KB | .text (code) |
| RAMLS5 | 0x00A000 – 0x00A7FF | 2 KB | .bss / .data |
| RAMLS6-7 | 0x00A800 – 0x00BFFF | 6 KB | CLA / IQmath |
| FLASH BANK0 | 0x080000 – 0x09FFFF | 128 KB | Application code + constants |
| FLASH BANK1 | 0x0A0000 – 0x0BFFFF | 128 KB | Extended code |
| FLASH BANK2 | 0x0C0000 – 0x0DFFFF | 128 KB | Extended code |

## Peripheral Register Reference

All peripheral registers are memory-mapped structures defined in the device
headers installed by C2000Ware. The linker command file
`f28003x_headers_nonBIOS.cmd` maps each structure to its hardware address.

### Commonly Used Register Structures

| Structure | Header / Source | Description |
|---|---|---|
| `GpioCtrlRegs` | `f28003x_gpio.h` / `f28003x_gpio.c` | GPIO mux, direction, pull-up, qualification |
| `GpioDataRegs` | `f28003x_gpio.h` | GPIO set, clear, toggle, read |
| `AdcaRegs` / `AdcbRegs` / `AdccRegs` | `f28003x_adc.h` / `f28003x_adc.c` | ADC control, SOC config, results |
| `CanaRegs` | `f28003x_can.h` | Classic CAN controller registers |
| `EPwm1Regs` – `EPwm8Regs` | `f28003x_epwm.h` / `f28003x_epwm.c` | ePWM time-base, compare, action-qualifier |
| `CpuTimer0Regs` – `CpuTimer2Regs` | `f28003x_cputimers.h` / `f28003x_cputimers.c` | CPU timer period, prescale, interrupt |
| `PieCtrlRegs` / `PieVectTable` | `f28003x_piectrl.c` / `f28003x_pievect.c` | Interrupt controller |
| `ClkCfgRegs` / `CpuSysRegs` | `f28003x_sysctrl.c` | Clock config, PLL, peripheral clock gates |

### Register Access Pattern

All register modifications must be wrapped in `EALLOW` / `EDIS` to unlock
the protected register space:

```c
EALLOW;
GpioCtrlRegs.GPAMUX2.bit.GPIO20 = 0;   // GPIO mode
GpioCtrlRegs.GPADIR.bit.GPIO20  = 1;   // Output
EDIS;
```

## Interrupt Architecture

The F280039C uses a two-level **PIE** (Peripheral Interrupt Expansion) system:

1. **PIE Groups 1–12** — each group holds up to 16 interrupt sources.
2. **CPU Interrupt Lines INT1–INT12** — one per PIE group.

Interrupt flow: Peripheral → PIE → CPU → ISR (via `PieVectTable`).

To add a new ISR:

```c
// 1. Map ISR in the PIE vector table
PieVectTable.ADCA1_INT = &adca1_isr;

// 2. Enable the PIE interrupt (group, channel)
PieCtrlRegs.PIEIER1.bit.INTx1 = 1;

// 3. Enable the CPU interrupt line
IER |= M_INT1;

// 4. Global interrupt enable
EINT;
ERTM;
```

## Clock Tree

```
XTAL (10 MHz)
  │
  └─► PLL ──► PLLSYSCLK (120 MHz) ──► SYSCLK
                 │
                 ├─► CPU core
                 ├─► Peripheral clocks (gated per module)
                 └─► CLA clock
```

The PLL is configured in `InitSysCtrl()` → `InitSysPll()` inside
`f28003x_sysctrl.c`. Peripheral clocks are individually gated in
`InitPeripheralClocks()`.

## Driver Module Template

Each driver module follows this structure:

```
drivers/<name>/
├── <name>_driver.h    ← Public API (init, runtime functions, types)
└── <name>_driver.c    ← Implementation
```

### Header Template

```c
#ifndef <NAME>_DRIVER_H
#define <NAME>_DRIVER_H

#include "f28003x_device.h"

// ---------- Configuration ----------
// Define any compile-time constants here.

// ---------- Public API ----------
void <name>_driver_init(void);

#endif // <NAME>_DRIVER_H
```

### Source Template

```c
#include "<name>_driver.h"

void <name>_driver_init(void)
{
    EALLOW;
    // Peripheral-specific register configuration
    EDIS;
}
```

## Pin Assignments (LaunchPad)

| GPIO | Function | Direction | Notes |
|---|---|---|---|
| 20 | LED (D10) | Output | Active-low, toggled in blinky demo |

> **Future drivers must document their pin assignments** in the driver header
> and in this table to prevent pin conflicts.

## Build Configurations

| Config | Entry Point | Code Location | Use Case |
|---|---|---|---|
| CPU1_RAM | `code_start` (RAMM0) | All in SRAM | Fast development iteration |
| CPU1_FLASH | `code_start` (FLASH BEGIN) | Flash + RAM copy | Production / persistent |

Both configurations are defined in `.cproject` and use the TI C2000 compiler
v25.11.0 with FPU32 support.
