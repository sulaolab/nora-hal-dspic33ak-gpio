# NORA migration — where these sources come from (2026-08-09)

This repository used to be the place the GPIO/PPS HAL was edited. It is not any
more. Since the NORA migration it is a **published snapshot**: `src/` is filled
from the tree that is actually built and run on hardware, and this file records
which tree, which commit, and how the equality was checked.

## The chain

```
dsp-sonora audio board project        the tree that runs on hardware
  main = 91adb63
        |  vendored, byte-for-byte
        v
sulaolab/dspic33ak-hal-starter        MPLAB X project, 11 HAL modules
  refactor/nora-hal = b70982d
        |  published, byte-for-byte
        v
sulaolab/nora-hal-dspic33ak-gpio      this repository
```

Direction matters: it used to run the other way (starter vendored *from* the
standalone repos). It was reversed because only the board project exercises the
code on silicon, so it is the only place a fix can be validated before it is
published. A fix made here and not upstream would be a fork.

## What the two commits did

| commit | what |
|---|---|
| `b5fbcec` | **rename only** — 7 files, all detected as R100 (100 % similarity). No byte of content changed, so the rename is reviewable on its own. |
| `f812d11` | **content refresh** — the 7 files replaced with the starter's bytes. This is where the functional delta below entered. |

Splitting them this way is the whole point: a reviewer can confirm the rename is
mechanical without reading content, and then read the content diff without
rename noise.

### The rename mapping

| before | after | why |
|---|---|---|
| `src/dspic33ak_gpio.h` | `src/nora_gpio.h` | public header: no chip in the name |
| `src/dspic33ak_gpio.c` | `src/nora_gpio_dspic33ak.c` | backend: tagged |
| `src/dspic33ak_gpio_reg.h` | `src/nora_gpio_dspic33ak_reg.h` | backend-private register layer: tagged |
| `src/dspic33ak_gpio_event.h` | `src/nora_gpio_event.h` | public header |
| `src/dspic33ak_gpio_event.c` | `src/nora_gpio_event_dspic33ak.c` | backend |
| `src/dspic33ak_pps.h` | `src/nora_pps.h` | public header |
| `src/dspic33ak_pps.c` | `src/nora_pps_dspic33ak.c` | backend |

The tag is `_dspic33ak`, not `_dspic33a`. `dsPIC33A` is the *core* family name;
the file drives dsPIC33AK SFRs. A dsPIC33CK backend would be `_dspic33ck` — a
different silicon family (dsPIC33**C**), never shortened to `_dspic33c`.

## Proof of identity with the upstream tree

Git blob hashes, this repository at `f812d11` vs
`dspic33ak-hal-starter` at `b70982d`. Identical hash = identical bytes; git
normalises EOLs into the blob on both sides, so the CRLF working trees do not
disturb the comparison.

| file | blob | bytes |
|---|---|---|
| `nora_gpio.h` | `1af4373dbe53` | 11592 |
| `nora_gpio_dspic33ak.c` | `55a8eb64300a` | 13697 |
| `nora_gpio_dspic33ak_reg.h` | `229ae5e7d3f6` | 1244 |
| `nora_gpio_event.h` | `e74f26b4b390` | 3180 |
| `nora_gpio_event_dspic33ak.c` | `e0c3097fcefb` | 22189 |
| `nora_pps.h` | `0ff5f37553c5` | 7484 |
| `nora_pps_dspic33ak.c` | `d8f333f6c9f5` | 22549 |

**7 of 7 identical.** The starter's copies are themselves byte-identical to the
sonora board project's, verified the same way when the starter was re-synced.

## What actually changed in the content refresh

Method: take each new file, reverse the naming (`nora_` → `dspic33ak_`,
`NORA_` → `DSPIC33AK_`, and strip the `_dspic33ak` backend tag from the file
names), and diff against the pre-rename blob. Whatever is left is *not* naming.

Residue, in lines (a `+`/`-` pair on the same construct counts as both):

| file | + | − | of which non-comment |
|---|---|---|---|
| `nora_gpio.h` | 23 | 7 | +2 / −2 |
| `nora_gpio_dspic33ak.c` | 11 | 7 | +6 / −6 |
| `nora_gpio_dspic33ak_reg.h` | 0 | 0 | — |
| `nora_gpio_event.h` | 1 | 1 | 0 / 0 |
| `nora_gpio_event_dspic33ak.c` | 154 | 36 | +127 / −31 |
| `nora_pps.h` | 46 | 7 | +22 / −1 |
| `nora_pps_dspic33ak.c` | 81 | 1 | +72 / 0 |

All of it is accounted for:

* **Public API: +2, nothing removed, nothing renamed.** Comparing the exported
  function names of the three headers before and after gives 45 → 47, and the
  two additions are `nora_pinmux_route_input` / `nora_pinmux_route_output` —
  wrappers that do the digital-pin configuration *and* the PPS route in one
  call, in the order every PPS user needs anyway. They add no register access;
  they call the two existing APIs unchanged.
* **PPS signal enums grew.** Outputs gained SPI3 (SS/SCK/SDO) and SPI4 SS;
  inputs gained SPI3 (SS/SCK/SDI) and SPI4 SS/SCK, plus the nine Input Capture
  inputs `ICM1..ICM9` (used by the CCP input-capture HAL). Enumerators are
  appended, so existing values keep their numbering.
* **Two parameter types tightened.** `nora_gpio_pin_from_rp` and
  `nora_gpio_rp_from_pin` take/return `nora_gpio_rp_t` where they took/returned
  a bare `uint8_t`. `nora_gpio_rp_t` *is* `typedef uint8_t`, so this is both
  source- and ABI-compatible; it only makes the intent visible.
* **One behavioural change**, in `nora_gpio_config()`: when the requested
  direction is input, `TRIS` is now set **first**, before analog/pull/open-drain,
  instead of last. A pin that is currently a live output therefore stops driving
  before its electrical attributes change underneath it, which makes a runtime
  output→input transition safe and not just first-time configuration. The output
  path is unchanged (latch seeded, then `TRIS` cleared).
* **One new internal helper**, `irq_flag_is_set`, file-static in the event
  backend. Not visible to callers.
* **Comments.** The portability wording was reworked from "device adaptation" to
  "processor adaptation", and the RP range is now described as a board/processor
  fact rather than a fixed RP1..RP128 (the dsPIC33AK backend still supports
  exactly RP1..RP128, `#ifdef`-guarded on `_RPnnR`).

`nora_gpio_dspic33ak_reg.h` has **zero** residue — it is a pure rename.

The starter's own migration record predicted this delta as
"`+2 (nora_pinmux_route_input/output)`", which the measurement confirms for the
API surface. The enum, comment and TRIS-ordering work rode along with it and is
listed above so it is not discovered later as an unexplained difference.

## Comment corrections made here, ahead of upstream

Everything above describes the state as published on 2026-08-08, when every file under
`src/` was byte-identical to upstream. On **2026-08-09** a documentation review found a
class of error that the identity proof above cannot see, and it was fixed here first
rather than waiting for the next upstream refresh.

* `src/nora_gpio.h`, `src/nora_pps.h` — six comments wrote the HAL family name as `Nora`
  rather than `NORA`.
* `src/nora_gpio_dspic33ak.c` — one comment said `dsPIC33A` where it means the dsPIC33AK
  backend. The `dsPIC33A` in `nora_gpio_dspic33ak_reg.h` was left alone: there it
  correctly says "the dsPIC33AK (dsPIC33A core) GPIO SFRs", and the `dsPIC33A/h/` DFP
  paths are literal directory names.

No executable code changed. The edits are comments and Markdown; the compiled
result is unchanged.

### Why the proof in "Proof of identity" does not catch this

Step 3 reverse-normalises the NORA names back to `dspic33ak_*` and diffs against the
pre-rename blob, so whatever is left is not naming. Two error classes cancel out exactly
in that diff and are therefore invisible to it:

* **A document reference to a file that was renamed.** A prose mention of
  `nora_<mod>_hw.{c,h}` reverse-normalises to `dspic33ak_<mod>_hw.{c,h}`, which is the
  *correct* pre-rename name — the diff is empty, yet the file is now called
  `nora_<mod>_dspic33ak_hw.{c,h}` and the reference is dead. The same cancellation hides
  `Nora` vs `NORA` and `dsPIC33A` vs `dsPIC33AK`: both sides of the diff are naming, so
  naming errors are exactly what it is blind to.
* **A document that omits a file the refresh added.** An absent line produces no diff
  line at all.

Both blind spots were observed across the NORA-HAL migration fleet; the subset that
affected *this* repository is the list above. Neither is detectable by
reverse-normalisation. What does detect them is resolving every `nora_*.{c,h}`
mentioned in prose against the actual contents of `src/`, and reading every
`dsPIC33A` / `Nora` hit rather than counting them — which is how these were found.

## Hardware evidence

There is no build or test in this repository — it is sources only. The evidence
is the upstream project's: `dspic33ak-hal-starter`
`docs/nora_hal_migration_analysis.md` §11e records a PASS run of all 11 NORA-ised
modules on PKOB4 `020085204RYN000057` (dsPIC33AK512MPS512, Device ID `0xa77c`)
on 2026-08-09.

Scope, stated plainly: that run has no dedicated GPIO unit test. GPIO and PPS are
covered *indirectly* but pervasively — the console UART, the SST26 chip-select /
reset / write-protect lines, the I2C scan, the CAN1 bus and the TDM8 smoke test
each need pins configured through this HAL and signals routed through its PPS
companion, and all of those checks passed. The board project that vendors the
same bytes also drives LEDs, buttons and the CN event layer.

## Consumer impact

* The public namespace changed from `dspic33ak_*` / `DSPIC33AK_*` to `nora_*` /
  `NORA_*` and **no compatibility aliases were added**. Call sites must be
  renamed; the substitution is purely textual.
* The `#include` names changed — see the rename mapping above.
* `sulaolab/dspic33ak-gpio-cmsis-driver` vendors this repository by hard-coded
  file list and repository URL in `tools/sync_hal_from_upstream.py`; it will not
  sync until that list and URL are updated. Tracked separately from this
  migration.
