# Contributing

Guidelines for adding features to this C2000 firmware project. Written for both
human contributors and AI agents.

## Quick Start

1. Read [`ARCHITECTURE.md`](ARCHITECTURE.md) to understand the layer model and
   register access patterns.
2. Create your driver under `drivers/<module>/` with a `.h` and `.c` file.
3. Follow the naming and style conventions below.
4. Update the feature table in the root [`README.md`](../README.md).
5. Test on the LAUNCHXL-F280039C dev kit in **CPU1_RAM** mode first.

## Coding Standards

### Naming

| Element | Convention | Example |
|---|---|---|
| Functions | `snake_case` | `can_driver_init()` |
| Local variables | `snake_case` | `uint16_t msg_count` |
| Global variables | `g_` prefix + `snake_case` | `volatile uint16_t g_adc_result` |
| Constants / macros | `UPPER_CASE` | `#define CAN_BAUD_500K 500000U` |
| Type definitions | `snake_case_t` | `typedef struct { ... } can_msg_t;` |
| Header guards | `<MODULE>_DRIVER_H` | `#ifndef CAN_DRIVER_H` |

### Style

* **Indentation**: 4 spaces (no tabs).
* **Braces**: opening brace on the same line as the statement.
* **Line length**: 100 characters maximum.
* **Comments**: use `//` for single-line, `/* */` for block comments.
  Add a file-header comment block to every new file describing its purpose,
  author context, and pin usage.

### File Header Template

```c
/******************************************************************************
 * @file    <name>_driver.c
 * @brief   <one-line description>
 *
 * Target:  TMS320F280039C
 * Pins:    <list GPIO pins used, e.g., GPIO 32 (CANTXA), GPIO 33 (CANRXA)>
 *
 * Dependencies:
 *   - f28003x_device.h (register definitions)
 *   - InitSysCtrl() must be called before driver init
 *****************************************************************************/
```

## Adding a New Driver

### Step-by-Step

1. **Create the directory**: `drivers/<name>/`

2. **Create the header** (`<name>_driver.h`):
   * Include `f28003x_device.h`.
   * Define configuration constants.
   * Declare the public API: at minimum an `<name>_driver_init()` function.
   * Document pin assignments in the header comment block.

3. **Create the source** (`<name>_driver.c`):
   * Include only the driver's own header.
   * Wrap all register writes in `EALLOW` / `EDIS`.
   * Enable the peripheral clock if not already done in `InitPeripheralClocks()`.

4. **Integrate with the application**:
   * `#include` the driver header from the application source file.
   * Call `<name>_driver_init()` after `InitSysCtrl()` in `main()`.

5. **Update documentation**:
   * Add the driver's pin assignments to the pin table in `docs/ARCHITECTURE.md`.
   * Update the feature status table in `README.md`.

### Example: Adding a CAN Driver

```
drivers/can/
├── can_driver.h
└── can_driver.c
```

**can_driver.h** would declare:
```c
void can_driver_init(void);
void can_driver_send(uint32_t msg_id, const uint8_t *data, uint8_t len);
bool can_driver_receive(uint32_t *msg_id, uint8_t *data, uint8_t *len);
```

**In the application (main.c or led_ex1_blinky.c)**:
```c
#include "drivers/can/can_driver.h"

void main(void) {
    InitSysCtrl();
    can_driver_init();
    // ...
}
```

## Register Access Rules

1. **Always use `EALLOW` / `EDIS`** around writes to protected registers.
2. **Use the named bit-field structures** (e.g., `GpioCtrlRegs.GPADIR.bit.GPIO20`)
   rather than raw hex masks — this makes the code self-documenting.
3. **Do not modify registers owned by another driver** — if two drivers need
   the same register (e.g., `GpioCtrlRegs` for pin mux), coordinate through
   a shared GPIO configuration step.

## Interrupt Guidelines

* Register ISRs in the PIE vector table (`PieVectTable`).
* Keep ISRs short — set a flag and process in the main loop.
* Clear the PIE acknowledge register at the end of every ISR:
  ```c
  PieCtrlRegs.PIEACK.all = PIEACK_GROUPx;
  ```

## Testing

* **Bench test** every driver on the LAUNCHXL-F280039C LaunchPad before merging.
* **CPU1_RAM** mode allows rapid load-debug-fix cycles without wearing flash.
* Use CCS breakpoints and watch expressions to validate register state.
* For communication peripherals (CAN, SCI), use external tools
  (e.g., PCAN-USB, logic analyzer) to verify the bus.

## Pull Request Checklist

- [ ] New files follow the naming conventions.
- [ ] Header guard present and correct.
- [ ] File header comment block with pin list and dependencies.
- [ ] `EALLOW` / `EDIS` used for all protected register writes.
- [ ] No raw addresses — only named register structures.
- [ ] Pin assignments documented in `docs/ARCHITECTURE.md`.
- [ ] Feature table updated in `README.md`.
- [ ] Tested on LaunchPad hardware (or noted as untested).
