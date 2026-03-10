# TI F280039C C2000 Embedded Platform

Firmware for the **TMS320F280039C** microcontroller, targeting the
[LAUNCHXL-F280039C](https://www.ti.com/tool/LAUNCHXL-F280039C) LaunchPad
development kit. The long-term goal is to develop production-ready automotive
peripheral drivers (CAN, ADC, GPIO, PWM, etc.) that can be ported to custom
hardware built around the same MCU.

## Hardware Overview

| Parameter | Value |
|---|---|
| MCU | TMS320F280039C (C28x + CLA core) |
| Clock | 120 MHz (PLL from 10 MHz XTAL) |
| Flash | 384 KB (3 banks) |
| RAM | 69 KB (SRAM) |
| ADC | 3× 12-bit, 4.6 MSPS |
| PWM | 8× ePWM modules |
| Comms | CAN / MCAN, SCI (UART), SPI, I²C, LIN |
| Debug | On-board XDS110 JTAG via USB |

## Repository Layout

```
├── README.md                   ← This file
├── .gitignore
├── docs/
│   ├── ARCHITECTURE.md         ← Module map and peripheral register reference
│   └── CONTRIBUTING.md         ← Coding standards and how to add a new driver
├── drivers/
│   ├── can/                    ← CAN / MCAN bus driver (planned)
│   │   ├── can_driver.h
│   │   └── can_driver.c
│   ├── adc/                    ← ADC sampling driver (planned)
│   │   ├── adc_driver.h
│   │   └── adc_driver.c
│   ├── gpio/                   ← GPIO configuration helpers (planned)
│   │   ├── gpio_config.h
│   │   └── gpio_config.c
│   └── pwm/                    ← ePWM driver (planned)
│       ├── pwm_driver.h
│       └── pwm_driver.c
└── led_ex1_blinky/             ← Current blink-LED reference application
    ├── led_ex1_blinky.c        ← Application entry point (main)
    ├── f28003x_*.c             ← Device support source files
    ├── 28003x_generic_*.cmd    ← Linker command files (RAM / Flash)
    ├── f28003x_headers_nonBIOS.cmd
    ├── targetConfigs/          ← JTAG probe target configuration
    └── .ccsproject / .cproject / .project  ← CCS IDE project files
```

## Getting Started

### Prerequisites

* **Code Composer Studio (CCS) ≥ 12** — download from
  [ti.com/tool/CCSTUDIO](https://www.ti.com/tool/CCSTUDIO).
* **C2000Ware ≥ 5.x** — install via CCS Resource Explorer or download from
  [ti.com/tool/C2000WARE](https://www.ti.com/tool/C2000WARE).
* **XDS110 USB drivers** — bundled with CCS.

### Build & Flash

1. Clone this repository.
2. Open CCS → **File ▸ Import ▸ CCS Projects** → browse to `led_ex1_blinky/`.
3. Select the build configuration:
   * **CPU1_RAM** — load directly into SRAM (faster iteration, lost on reset).
   * **CPU1_FLASH** — program into on-chip flash (persistent).
4. Click **Build** (🔨) then **Debug** (🐛) to flash and run.

The on-board LED on GPIO 20 should blink at ~1 Hz.

## Planned Features

The following drivers are planned for future development. Each will live in its
own directory under `drivers/` and expose a clean header interface.

| Module | Directory | Status | Description |
|---|---|---|---|
| GPIO Config | `drivers/gpio/` | 🔜 Planned | Pin mux, pull-up/down, direction helpers |
| ADC | `drivers/adc/` | 🔜 Planned | Multi-channel sampling, calibration |
| CAN / MCAN | `drivers/can/` | 🔜 Planned | Automotive CAN 2.0 / CAN-FD messaging |
| ePWM | `drivers/pwm/` | 🔜 Planned | Duty-cycle generation, dead-band, trip zones |

## AI Agent Guide

This project is structured so that **multiple AI agents can work in parallel**
on independent driver modules. Each `drivers/<module>/` directory is a
self-contained unit with its own header and source files.

### Key Conventions

* **One driver per directory** — add new peripherals under `drivers/<name>/`.
* **Public API in the header** — every driver exposes an `_init()` function and
  any runtime functions through its `.h` file.
* **No cross-driver dependencies** — drivers depend only on the device header
  files (e.g., `f28003x_device.h`, `f28003x_examples.h`) and the system
  init provided by the base project.
* **Register access** — use the TI-provided register structures
  (`CanaRegs`, `AdcaRegs`, `GpioCtrlRegs`, etc.) defined in the device
  headers. Do **not** use raw memory addresses.
* **Naming** — use `snake_case` for functions and variables, `UPPER_CASE` for
  constants and register macros, matching TI conventions.

### How to Add a New Driver

1. Create `drivers/<name>/<name>_driver.h` and `drivers/<name>/<name>_driver.c`.
2. Define an init function (e.g., `can_driver_init()`) and any runtime API.
3. Include the driver header from the application (e.g., `led_ex1_blinky.c` or
   a future `main.c`).
4. Document the driver's purpose and pin assignments in a comment block at the
   top of the header.
5. Update the feature table in this README.

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for the full module map and
register reference, and [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md) for
coding standards.

## License

See repository license file. All TI device-support source files are provided
under Texas Instruments' standard terms — see file headers for details.
