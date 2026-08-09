# nora-hal-dspic33ak-gpio

**NORA-HAL** — *Native On-chip Resource Assistant*

Small, readable GPIO and PPS HAL for Microchip dsPIC33AK devices — part of
**NORA-HAL**, a HAL family whose public API is namespaced `nora_*` /
`NORA_*`.

> Want to run it on hardware first?
> Start with [dspic33ak-hal-starter](https://github.com/sulaolab/dspic33ak-hal-starter),
> which vendors validated snapshots of the NORA-HAL repositories and
> provides a ready-to-build MPLAB X project for the dsPIC33AK Curiosity board.

> **This repository is a published snapshot, not the development tree.** Every
> file under `src/` is byte-identical to its counterpart in
> `dspic33ak-hal-starter`, which is in turn byte-identical to the audio-board
> project that runs these sources on hardware. Fixes flow *into* here from that
> validated tree — see [docs/nora_migration.md](docs/nora_migration.md).
>
> **One exception, 2026-08-09.** Documentation and comment corrections under `src/`
> were made here first, ahead of the audio-board upstream. **No executable code
> changed.** The exact files and corrections are listed in
> [docs/nora_migration.md](docs/nora_migration.md).

This project is intended as a compact alternative to large generated driver code.
The goal is not to hide everything behind a framework, but to provide a simple
GPIO layer that is easy to read, test, modify, and adapt.

## Naming

The public API is `nora_*` / `NORA_*`. It replaces the `dspic33ak_*` /
`DSPIC33AK_*` namespace this repository used before 2026-08, and **there are no
compatibility aliases** — a consumer moving to this version renames its call
sites. The rename is purely textual: `dspic33ak_` → `nora_`,
`DSPIC33AK_` → `NORA_`.

The chip name survives in exactly two places, both deliberate:

* **Implementation file names** carry a backend tag: `nora_gpio_dspic33ak.c` is
  the dsPIC33AK backend of the processor-neutral `nora_gpio.h`. A second
  processor would add `nora_gpio_<tag>.c` beside it, not a second header.
* **Backend-private identifiers** inside those files (register-layer macros and
  statics), which no caller sees.

The tag is `_dspic33ak`, the device family this backend actually drives — not
`_dspic33a`, which is the *core* family name (dsPIC33A) and one level too
coarse. A dsPIC33CK backend would be tagged `_dspic33ck`: a different silicon
family (dsPIC33**C**), and never abbreviated to `_dspic33c`.

## Status

Current validation target:

* Device: dsPIC33AK512MPS512
* Compiler: XC-DSC v3.31.01
* DFP: Microchip dsPIC33AK-MP DFP 1.3.185 (or compatible)

The port register table is built from `#if defined(LATx)` presence tests, so it
adapts to whichever ports the selected device header defines, without
device-name conditionals.

This HAL is used in a larger board project; on the dsPIC33AK512MPS512 target it
has been validated for:

* LED output
* button input
* external SPI-flash control pins (chip-select / reset / write-protect)
* UART1 / UART2 pin pre-configuration
* ADC analog input pre-configuration
* TDM / I2S peripheral pin pre-configuration

Confirmed operations on the validation target:

* Per-pin direction (input / output)
* Per-pin pull-up / pull-down / none
* Per-pin analog / digital (ANSEL) select
* Per-pin open-drain enable
* One-shot `nora_gpio_config()` with a glitch-aware apply order
* Output write / set / clear / toggle
* Input read (PORT) and output-latch read-back (LAT), 3-state level result
* RP-first addressing (RPn) over the packed-pin core
* Optional PPS peripheral-pin routing (companion `nora_pps.h` / `nora_pps_dspic33ak.c`), exercised by
  the SPI / UART / TDM pin routing in the upstream audio board project
* Optional GPIO CN event dispatch layer, validated in `dspic33ak-hal-starter`

## Design policy

This HAL is intentionally small.

* A pin is addressed either by its Remappable-Pin number (RPn) through the
  **RP-first API** (preferred for normal board and application code) or by a
  packed `(port, bit)` handle through the **core packed-pin API** (for non-RP
  pins, low-level GPIO, or HAL internals). Both APIs reach the same GPIO
  register implementation; the RP-first functions are thin wrappers that convert
  RPn to a packed pin and delegate to the core.
* No XC-DSC / DFP bitfield structures (`LATxbits` / `TRISxbits` / ...) are
  exposed in the public API.
* Device-specific register symbols are isolated in a small per-port pointer
  table. The table is built from `#if defined(LATx)` presence tests, so it
  adapts to the device without device-name conditionals.
* The core GPIO layer (`nora_gpio.h` / `nora_gpio_dspic33ak.c`) owns only the GPIO attribute/data
  registers (`ANSEL` / `TRIS` / `LAT` / `PORT` / `CNPU` / `CNPD` / `ODC`). PPS
  signal routing is a separate, optional companion module (`nora_pps.h` / `nora_pps_dspic33ak.c`) in
  this same family; the board layer owns the policy — which signal maps to which
  RP pin.
* The core GPIO layer does not own interrupt vectors. The optional CN event
  layer only dispatches registered GPIO events when the application calls it
  from an app-owned vector.
* The accessors are plain read-modify-write and do NOT disable interrupts; if a
  port is updated from both main-line code and an ISR, the caller provides the
  mutual exclusion.

## Scope

In scope:

* Direction, pull, analog/digital, open-drain configuration
* Output write / set / clear / toggle
* Input read and output-latch read-back
* RP-first addressing (RPn) as a thin adapter over the packed-pin core
* Optional PPS (peripheral pin select) routing — companion `nora_pps.h` / `nora_pps_dspic33ak.c`
* Optional Change Notification (CN) event attach/detach/dispatch
* Any port present on the device (A..H as defined by the device header)

Out of scope (not handled here):

* HAL-owned interrupt vectors
* Debounce or new event semantics beyond the validated CN event layer
* Atomic set/clear via dedicated SFRs (accessors are read-modify-write)
* RTOS locking / cross-context mutual exclusion

## Files

Headers are processor-neutral and untagged; each `.c` is a backend and carries
the `_dspic33ak` tag.

```text
src/
  nora_gpio.h                  core GPIO API (attributes + data) + RP-first adapter
  nora_gpio_dspic33ak.c          dsPIC33AK backend
  nora_gpio_dspic33ak_reg.h      dsPIC33AK register layer (SFR pointers/masks)
  nora_pps.h                   optional PPS (peripheral pin select) routing
  nora_pps_dspic33ak.c           dsPIC33AK backend
  nora_gpio_event.h            optional Change Notification (CN) event layer
  nora_gpio_event_dspic33ak.c    dsPIC33AK backend
docs/
  gpio_event_design.md
  nora_migration.md            where these bytes come from, and what changed
```

`nora_gpio_dspic33ak_reg.h` is the only place that touches the raw GPIO SFRs as
32-bit pointers and bit masks; the driver body drives any port through a
per-port pointer table.

The two optional companions are compiled only when their feature is used:
compile `nora_gpio_event_dspic33ak.c` only when CN event support is needed, and
`nora_pps_dspic33ak.c` only when the board routes peripherals through PPS.

## Pin addressing

Two addressing styles are supported. Use RP-first for PPS-capable board and
application pins; use packed-pin for non-RP pins or low-level core usage.

**RP-first (preferred for board/application code with PPS):** a pin is
identified by its Remappable-Pin number `RPn` — the same number the PPS map
uses — so the GPIO call and the PPS route refer to the pin identically:

```c
#define BOARD_UART1_TX_RP  (114u)   /* U1TX -> RH1 */

nora_gpio_rp_config_digital_output(BOARD_UART1_TX_RP, true);
nora_pps_route_output(NORA_PPS_OUTPUT_U1TX, BOARD_UART1_TX_RP);
```

**Packed-pin (core API — for non-RP pins or HAL internals):** a pin is a
packed number `(port << 4) | bit`. Always build it with
`NORA_GPIO_PIN(port, bit)` and give it a board-level name:

```c
#include "nora_gpio.h"

#define BOARD_LED0      NORA_GPIO_PIN(NORA_GPIO_PORT_C, 8)
#define BOARD_FLASH_CS  NORA_GPIO_PIN(NORA_GPIO_PORT_D, 15)
```

Port codes are `NORA_GPIO_PORT_A` .. `NORA_GPIO_PORT_H`; `bit` is
`0..15`. Values outside this range are masked by the macro and should not be
used.

## Basic usage

One-shot configuration (recommended) applies attributes in a glitch-aware order
(analog/digital → pull → open-drain → initial output level → direction):

```c
const nora_gpio_config_t led_cfg = {
    .dir          = NORA_GPIO_DIR_OUTPUT,
    .pull         = NORA_GPIO_PULL_NONE,
    .analog       = false,
    .open_drain   = false,
    .initial_high = false,   /* LAT seeded before the pin becomes an output */
};
(void)nora_gpio_config(BOARD_LED0, &led_cfg);

nora_gpio_set(BOARD_LED0);      /* drive high            */
nora_gpio_clear(BOARD_LED0);    /* drive low             */
nora_gpio_toggle(BOARD_LED0);   /* flip the output latch */
```

Reading an input:

```c
const nora_gpio_config_t btn_cfg = {
    .dir = NORA_GPIO_DIR_INPUT, .pull = NORA_GPIO_PULL_UP,
    .analog = false, .open_drain = false, .initial_high = false,
};
(void)nora_gpio_config(BOARD_BUTTON, &btn_cfg);

/* read() returns a 3-state level (ERROR / LOW / HIGH) -- not a bool. Handle
 * NORA_GPIO_LEVEL_ERROR first, then compare against LOW / HIGH. */
bool pressed = (nora_gpio_read(BOARD_BUTTON) == NORA_GPIO_LEVEL_LOW);  /* active-low */
```

Individual attribute setters are also available when one-shot config is not
wanted:

```c
(void)nora_gpio_set_analog(BOARD_LED0, false);
(void)nora_gpio_set_pull(BOARD_LED0, NORA_GPIO_PULL_NONE);
(void)nora_gpio_set_open_drain(BOARD_LED0, false);
(void)nora_gpio_set_direction(BOARD_LED0, NORA_GPIO_DIR_OUTPUT);
```

GPIO CN event usage keeps the vector in the application:

```c
static volatile bool sw3_changed;

static void sw3_event_cb(nora_gpio_pin_t pin,
                         nora_gpio_event_edge_t edge,
                         void *user_data)
{
    (void)pin;
    (void)edge;
    (void)user_data;
    sw3_changed = true;
}

void __attribute__((__interrupt__, __no_auto_psv__)) _CNBInterrupt(void)
{
    nora_gpio_event_process_isr();
}
```

## PPS routing (peripheral pin select)

`nora_pps.h` / `nora_pps_dspic33ak.c` is an optional companion module that maps a peripheral signal
to or from a Remappable-Pin (RPn). It is compiled only when the board routes
peripherals through PPS. The board layer owns the policy — which signal maps to
which RP pin — and uses the *same* RPn for the GPIO attribute call and the PPS
route, so the two refer to the pin identically:

```c
#define BOARD_UART1_TX_RP  (114u)   /* U1TX -> RH1 */
#define BOARD_UART1_RX_RP  (50u)    /* U1RX <- RD1 */

nora_gpio_rp_config_digital_output(BOARD_UART1_TX_RP, true);   /* GPIO first */
nora_pps_route_output(NORA_PPS_OUTPUT_U1TX, BOARD_UART1_TX_RP);

nora_gpio_rp_config_digital_input(BOARD_UART1_RX_RP);
nora_pps_route_input(NORA_PPS_INPUT_U1RX, BOARD_UART1_RX_RP);
```

API:

* `nora_pps_route_output(signal, rp)` — drive a peripheral output onto RPn.
  Returns `false` (routing nothing) if the signal or the RP pin is not defined
  on the selected device.
* `nora_pps_route_input(signal, rp)` — feed a peripheral input from RPn.
  Rejects an `rp` that is not a physical pin on the device (returns `false`
  before writing).
* `nora_pps_unlock()` / `nora_pps_lock()` — the RPCON.IOLOCK gate. The
  `route_*` functions open and close it themselves; these are exposed only for
  code that writes PPS registers directly.

Because *every* PPS user has to do the two steps above in that order, the pair is
also available as one call:

* `nora_pinmux_route_output(signal, rp, initial_high)` — configure RPn as a
  digital output (seeding the latch first) and then route the signal.
* `nora_pinmux_route_input(signal, rp)` — configure RPn as a digital input and
  then route the signal.

Both apply the GPIO configuration first and return `false` without routing
anything if that step fails; otherwise they return the result of the route. They
add no register access of their own — they call the two APIs above unchanged.

```c
nora_pinmux_route_output(NORA_PPS_OUTPUT_U1TX, BOARD_UART1_TX_RP, true);
nora_pinmux_route_input(NORA_PPS_INPUT_U1RX, BOARD_UART1_RX_RP);
```

The signal enums (`nora_pps_output_t` / `nora_pps_input_t`) carry the
signals this codebase currently needs — UART (U1TX/U2TX, U1RX/U2RX), SPI1..SPI4
(SS/SCK/SDO out, SS/SCK/SDI in), CLC1-3 (CLCINA/B/C in), PWM (1H/2H/3H and
5H/5L..8H/8L), REFI1, CAN1 (TX/RX), and the Input Capture inputs ICM1..ICM9.
They are **not** every PPS-capable peripheral the device supports.

### Adding a signal

Extend the enum, then add the matching device-guarded case — no part-number
conditionals:

1. Add an enumerator to `nora_pps_output_t` (outputs) or
   `nora_pps_input_t` (inputs) in `nora_pps.h`.
2. Add the matching `#ifdef`-guarded case in `nora_pps_dspic33ak.c`:
   * output: `#ifdef _RPOUT_<sig>` → `case ...: *code = (uint8_t)_RPOUT_<sig>; return true;`
   * input:  `#ifdef _<sig>R`      → `case ...: _<sig>R = rp; break;`

Device adaptation is entirely by these `#ifdef`s on the XC-DSC header macros
(output function code `_RPOUT_<sig>`, output pin register `_RP<nn>R`,
input-select register `_<sig>R`). A signal or RP the selected device header does
not define is simply left out of the switch and the route call returns `false`.
(U2TX / U2RX were added exactly this way.)

## API summary

### Packed-pin core API

Configuration (glitch-aware apply order: analog → pull → open-drain → LAT → direction):

* `nora_gpio_config()` — one-shot via config struct
* `nora_gpio_config_digital_input()` — shortcut: digital input, no pull
* `nora_gpio_config_digital_output()` — shortcut: digital output, seeds LAT first
* `nora_gpio_set_direction()`
* `nora_gpio_set_pull()`
* `nora_gpio_set_analog()`
* `nora_gpio_set_open_drain()`

Output:

* `nora_gpio_write()` — drive a given level
* `nora_gpio_set()`   — drive high
* `nora_gpio_clear()` — drive low
* `nora_gpio_toggle()`

Input / read-back (return a 3-state `nora_gpio_level_t`, **not a bool**):

* `nora_gpio_read()`        — pin level from `PORT`
* `nora_gpio_read_output()` — driven latch from `LAT`

### RP-first API

Thin wrappers over the packed-pin core (convert RPn → packed pin, then call
the matching core function). Preferred for board and application code that uses
PPS-capable pins. Do not duplicate register-access logic.

Conversion:

* `nora_gpio_pin_from_rp()`
* `nora_gpio_rp_from_pin()`

Full configuration (via config struct):

* `nora_gpio_rp_config()`

Individual attribute setters:

* `nora_gpio_rp_set_direction()`
* `nora_gpio_rp_set_pull()`
* `nora_gpio_rp_set_analog()`
* `nora_gpio_rp_set_open_drain()`

Digital shortcuts (most common cases):

* `nora_gpio_rp_config_digital_input()`
* `nora_gpio_rp_config_digital_output()`

Output operations:

* `nora_gpio_rp_write()`
* `nora_gpio_rp_set()`
* `nora_gpio_rp_clear()`
* `nora_gpio_rp_toggle()`

Read operations (3-state `nora_gpio_level_t`, **not a bool**):

* `nora_gpio_rp_read()`
* `nora_gpio_rp_read_output()`

Optional PPS routing (`nora_pps.h` / `nora_pps_dspic33ak.c`, compiled only when used):

* `nora_pps_route_output()` — drive a peripheral output onto an RP pin
* `nora_pps_route_input()`  — feed a peripheral input from an RP pin
* `nora_pps_unlock()` / `_lock()` — IOLOCK gate for direct PPS writers
* `nora_pinmux_route_output()` / `nora_pinmux_route_input()` — GPIO digital
  configuration **and** the PPS route in one call, in the glitch-aware order

Optional event layer:

* `nora_gpio_event_attach()`      — register one packed-pin event callback
* `nora_gpio_event_detach()`      — unregister one packed-pin event
* `nora_gpio_event_irq_enable()` / `_disable()` — configure the CN port
  interrupt line for setup/teardown
* `nora_gpio_event_irq_is_enabled()` / `_set_enabled()` — read/write only
  the IEC enable bit without clearing a pending CN event
* `nora_gpio_event_rp_*()` — RP-first wrappers for CN attach/detach and
  CN interrupt helper operations
* `nora_gpio_event_process_isr()` — app-called CN event dispatcher

The setters take a packed pin from `NORA_GPIO_PIN()` (or use the RP-first
wrappers) and return `false` if the pin's port is not present on the device (no
register row), otherwise `true`. `nora_gpio_read()` / `read_output()`
return a 3-state `nora_gpio_level_t` — `NORA_GPIO_LEVEL_ERROR` (`-1`)
for a pin whose port is not present, else `..._LOW` (`0`) / `..._HIGH` (`1`).
Do not use the result directly as a bool (ERROR is truthy); compare against the
named constants and handle ERROR first.

## Glitch-aware order

`nora_gpio_config()` applies an output pin as: select digital, set pull and
open-drain, seed the output latch (`initial_high`), and only then switch the pin
to an output. Seeding `LAT` before flipping `TRIS` avoids driving an undefined
level for one cycle when a pin first becomes an output.

The input case is ordered the other way round: `TRIS` is released **first**, so a
pin that is currently a live output stops driving before its analog, pull and
open-drain attributes change underneath it. That makes a runtime
output-to-input transition safe, not just first-time configuration.

## Device mapping

The per-port register table in `nora_gpio_dspic33ak.c` is the only place that
references `LATA` / `TRISA` / `PORTA` / ... Each row is emitted only when the
device header defines that port's SFRs, so the table tracks the silicon
automatically and the driver body stays device-neutral.

## Notes

* The GPIO SFRs are 32-bit on the dsPIC33A core; the register layer uses 32-bit
  pointers to match the DFP device headers.
* The core `nora_gpio.h` / `nora_gpio_dspic33ak.c` layer drives GPIO attributes/data only; PPS signal
  routing lives in the optional companion `nora_pps.h` / `nora_pps_dspic33ak.c`. The board layer
  still owns the policy — which signal maps to which RP pin.
* The optional event layer does not change ANSEL, TRIS, CNPU/CNPD, PPS,
  interrupt priority, or IEC enable bits in `nora_gpio_event_attach()`.
  Optional IRQ helpers are available when the application wants the HAL to hide
  the scattered CNxIP/CNxIF/CNxIE symbols.
* CMSIS-Driver GPIO-style wrappers are intentionally kept in separate
  repositories, such as `dspic33ak-gpio-cmsis-driver`. That wrapper still expects
  the pre-NORA file and symbol names and needs updating before it can sync again
  — see [docs/nora_migration.md](docs/nora_migration.md).
* This HAL checks whether a GPIO port exists in the selected device header. It
  does not check whether a specific package exposes that pin. Package-level and
  board-level pin validity must be handled by the board layer.
* This repository does not include Microchip DFP header files.

## License

MIT No Attribution License (MIT-0). See [LICENSE](LICENSE).

Attribution is appreciated but not required.
