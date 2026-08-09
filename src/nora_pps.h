#ifndef NORA_PPS_H
#define NORA_PPS_H

/*
 * nora_pps.h
 * ---------------
 * Peripheral Pin Select (PPS) routing -- the companion to the NORA GPIO HAL
 * (same hal_gpio family). NORA GPIO owns the pin's ELECTRICAL attributes
 * (TRIS/LAT/ANSEL/pull/OD); this module owns the SIGNAL ROUTING (which RP pin a
 * peripheral input reads from / a peripheral output drives), i.e. the RPINRx
 * (input-select) and RPORx (output _RPnnR) registers.
 *
 * Why a thin PPS layer:
 *   Board code otherwise writes device SFRs directly -- e.g.
 *       _RP90R = 29;        // SCK2 out on RP90 (what is 29?)
 *       _SS2R  = 29;        // SS2 in from RP29
 *   which leaks the RPORx/RPINRx map and the raw function codes into the board.
 *   These two calls hide that:
 *       nora_pps_route_output(NORA_PPS_OUTPUT_SCK2, 90u);
 *       nora_pps_route_input (NORA_PPS_INPUT_SS2,   29u);
 *   leaving only "which signal" + "which RP" at the call site.
 *
 * Processor adaptation: the implementation keys off the XC SFR/constant macros
 * (_RPOUT_xxx, _RPnnR, the input-select registers) with #ifdef, so a peripheral
 * or RP that the selected device does not define is simply unroutable (the call
 * returns false) -- no per-device part-number conditionals here.
 *
 * Coverage: both route_* accept any PHYSICAL remappable pin the selected device
 * defines. RP is a nora_gpio_rp_t supplied by the board pin map; its numeric range
 * and encoding are processor-specific. The dsPIC33AK backend supports
 * RP1..RP128 (#ifdef-guarded on _RPnnR); the RPV virtual outputs
 * (RP129..RP144) are intentionally NOT routable here -- if they are ever needed,
 * add a separate virtual-output API rather than overloading this GPIO-typed one.
 * route_input() rejects an rp that is not a physical pin on this device (returns
 * false before writing). The peripheral-SIGNAL enums contain the signals currently
 * required by this codebase (U1TX/RX, U2TX/RX, SPI1-4, CLC1-3, PWM1H/2H/3H,
 * PWM5H/5L-8H/8L, REFI1, CAN1TX/RX); they do NOT represent every PPS-capable
 * peripheral the device supports. Add a new signal by extending the enum and the
 * matching case in nora_pps_dspic33ak.c. Routing a signal absent on the selected
 * device returns false.
 *
 * Pairing with hal_gpio: PPS routing does NOT configure the pin's direction or
 * analog/digital select. Configure the pin first (nora_gpio_rp_config_*),
 * then route the signal. Order is glitch-aware for outputs: seed the GPIO output
 * (LAT/TRIS) before the PPS output starts driving it.
 *
 * IOLOCK: each route_* call brackets its own write with unlock()/lock() (IOLOCK
 * resets to 0 = unlocked, so this only adds protection). unlock()/lock() are also
 * exposed for code that writes PPS SFRs directly (e.g. the PWM driver) or wants a
 * single window around a batch of direct writes.
 */

#include <stdbool.h>

#include "nora_gpio.h"   /* nora_gpio_rp_t */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * PPS OUTPUT functions (a peripheral output driven onto an RP pin). Names mirror
 * the XC _RPOUT_* output-code set; the implementation maps each to its _RPOUT_x
 * value, so AK128/AK512 code differences are handled by the device header.
 */
typedef enum
{
    NORA_PPS_OUTPUT_U1TX,
    NORA_PPS_OUTPUT_U2TX,

    NORA_PPS_OUTPUT_SS1,
    NORA_PPS_OUTPUT_SCK1,
    NORA_PPS_OUTPUT_SDO1,

    NORA_PPS_OUTPUT_SS2,
    NORA_PPS_OUTPUT_SCK2,
    NORA_PPS_OUTPUT_SDO2,

    NORA_PPS_OUTPUT_SS3,
    NORA_PPS_OUTPUT_SCK3,
    NORA_PPS_OUTPUT_SDO3,

    NORA_PPS_OUTPUT_SS4,
    NORA_PPS_OUTPUT_SCK4,
    NORA_PPS_OUTPUT_SDO4,

    NORA_PPS_OUTPUT_CLC1,
    NORA_PPS_OUTPUT_CLC2,
    NORA_PPS_OUTPUT_CLC3,

    NORA_PPS_OUTPUT_PWM1H,
    NORA_PPS_OUTPUT_PWM2H,
    NORA_PPS_OUTPUT_PWM3H,
    NORA_PPS_OUTPUT_PWM5H,
    NORA_PPS_OUTPUT_PWM5L,
    NORA_PPS_OUTPUT_PWM6H,
    NORA_PPS_OUTPUT_PWM6L,
    NORA_PPS_OUTPUT_PWM7H,
    NORA_PPS_OUTPUT_PWM7L,
    NORA_PPS_OUTPUT_PWM8H,
    NORA_PPS_OUTPUT_PWM8L,

    NORA_PPS_OUTPUT_CAN1TX
} nora_pps_output_t;

/*
 * PPS INPUT functions (a peripheral input fed from an RP pin). Each maps to its
 * RPINRx input-select register (a bit-field alias; assignment takes the RP
 * number directly).
 */
typedef enum
{
    NORA_PPS_INPUT_U1RX,
    NORA_PPS_INPUT_U2RX,

    NORA_PPS_INPUT_SS1,
    NORA_PPS_INPUT_SCK1,
    NORA_PPS_INPUT_SDI1,

    NORA_PPS_INPUT_SS2,
    NORA_PPS_INPUT_SCK2,
    NORA_PPS_INPUT_SDI2,

    NORA_PPS_INPUT_SS3,
    NORA_PPS_INPUT_SCK3,
    NORA_PPS_INPUT_SDI3,

    NORA_PPS_INPUT_SS4,
    NORA_PPS_INPUT_SCK4,
    NORA_PPS_INPUT_SDI4,

    NORA_PPS_INPUT_CLCINA,
    NORA_PPS_INPUT_CLCINB,
    NORA_PPS_INPUT_CLCINC,

    NORA_PPS_INPUT_REFI1,

    NORA_PPS_INPUT_CAN1RX,

    // SCCP/MCCP Input Capture inputs (ICM1..ICM9 -> RPINR2..6). Route a pin here to feed a
    // CCP channel's Input Capture (see hal_ccp_input_capture).
    NORA_PPS_INPUT_ICM1,
    NORA_PPS_INPUT_ICM2,
    NORA_PPS_INPUT_ICM3,
    NORA_PPS_INPUT_ICM4,
    NORA_PPS_INPUT_ICM5,
    NORA_PPS_INPUT_ICM6,
    NORA_PPS_INPUT_ICM7,
    NORA_PPS_INPUT_ICM8,
    NORA_PPS_INPUT_ICM9
} nora_pps_input_t;

/*
 * PPS lock gate (RPCON.IOLOCK). unlock() makes the PPS map writable, lock()
 * protects it. route_*() below bracket their own writes; expose these only for
 * direct-SFR PPS writers or a single window around a batch.
 */
void nora_pps_unlock(void);   /* IOLOCK = 0 : PPS registers writable  */
void nora_pps_lock(void);     /* IOLOCK = 1 : PPS registers protected */

/*
 * Route a peripheral OUTPUT onto an RP pin (writes the RP's _RPnnR with the
 * peripheral's output function code). Self-brackets IOLOCK. Returns false if the
 * peripheral output is not available on this device, or the RP pin has no output
 * PPS register here (range/encoding contract only -- not a board-bonding check;
 * configure the GPIO output first via nora_gpio_rp_config_digital_output()).
 */
bool nora_pps_route_output(nora_pps_output_t output, nora_gpio_rp_t rp);

/*
 * Route a peripheral INPUT to read from an RP pin (writes the peripheral's RPINRx
 * input-select with the RP number). Self-brackets IOLOCK. Returns false if the
 * peripheral input is not available on this device, OR if rp is not a physical
 * remappable pin on this device (rp == 0, rp > 128, or an undefined RP is
 * rejected before any register write). Configure the GPIO input first via
 * nora_gpio_rp_config_digital_input().
 */
bool nora_pps_route_input(nora_pps_input_t input, nora_gpio_rp_t rp);

/*
 * Combined "pinmux" helpers: digital-configure the RP pin AND route the PPS signal in one call.
 * These wrap the two-step sequence every PPS user must perform -- first make the pin a digital
 * input/output (nora_gpio_rp_config_digital_input/output), then route the peripheral signal
 * (nora_pps_route_input/output) -- so the pairing and its glitch-aware order are guaranteed
 * in one place. The GPIO configuration is applied FIRST (for an output, the initial level is
 * seeded before the driver is enabled -- see nora_gpio_rp_config_digital_output). Returns
 * false and routes nothing further if the GPIO configuration fails; otherwise returns the result
 * of the PPS route. No register access is duplicated here -- both existing low-level APIs are
 * reused as-is.
 */
bool nora_pinmux_route_input(nora_pps_input_t function, nora_gpio_rp_t rp);
bool nora_pinmux_route_output(nora_pps_output_t function, nora_gpio_rp_t rp,
                                   bool initial_high);

#ifdef __cplusplus
}
#endif

#endif /* NORA_PPS_H */
