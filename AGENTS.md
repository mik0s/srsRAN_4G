# AGENTS.md

## Project context

This repository is a fork of srsRAN_4G used to develop `ltesurvey`,
a passive LTE survey tool primarily tested with HackRF One through SoapySDR.

The project specification is:

    docs/ltesurvey/SPEC.md

Read that document before making changes related to ltesurvey.

## Development baseline

The hardware-verified baseline is tagged:

    ltesurvey-known-good-20260920

At that tag the following were verified on real HackRF One hardware:

- required ltesurvey targets build on macOS ARM64;
- `pdsch_ue` successfully decodes MIB, SIB1 and SIB5;
- structured probe JSON is produced correctly;
- `cell_search` correctly retunes between LTE EARFCNs;
- a short B3 raster scan found the expected cell without stale-sample
  duplication between adjacent EARFCNs.

Do not move, recreate or modify this tag.

## Scope

The project performs passive LTE downlink survey and decoding of public
broadcast information.

In scope:

- LTE carrier/cell discovery;
- PSS/SSS detection;
- MIB decoding;
- SIB decoding;
- PLMN/TAC/ECI/eNodeB/cell identification;
- passive RF/PHY measurements;
- SIB5-based carrier discovery;
- offline PLMN/operator lookup;
- HackRF/SoapySDR support;
- scanner CLI, JSON output and packaging.

Keep changes focused on this scope.

## Critical HackRF / SoapySDR invariants

The current RF implementation contains hardware-verified fixes.

Do not refactor or revert them unless the task explicitly requires it and
the consequences are understood.

In particular:

1. An existing SoapySDR RX stream must be recreated after an actual
   sample-rate change, even when the stream is currently inactive.

2. An existing SoapySDR RX stream must be recreated after an actual
   RX-frequency change, even when the stream is currently inactive.

3. If the RX stream was active before such a change, it must be stopped
   before reconfiguration and restarted afterwards.

4. PSS/cell-search RX must be stopped before the MIB decoder starts its
   own RX operation.

5. `rf_soapy_flush_buffer()` operates in samples, not bytes.

These fixes address real failures observed with HackRF hardware,
including stale samples after retuning and failures when changing sample
rate from 1.92 MHz to 15.36 MHz.

Treat changes to:

    lib/src/phy/rf/rf_soapy_imp.c

as high-risk changes.

## RF measurement semantics

Do not label an RF value as RSRP, RSRQ, RSSI, SINR, dBm, or another
standard LTE metric unless its actual source and semantics justify that
label.

The existing `rf_metric` is not calibrated and must remain explicitly
marked as uncalibrated.

Do not invent calibration.

## LTE identity semantics

Preserve LTE identifiers in their native form.

For PLMN:

- preserve MCC;
- preserve MNC;
- preserve whether the MNC has 2 or 3 digits;
- preserve all PLMNs advertised by the cell;
- do not arbitrarily select a "primary" PLMN.

For ECI use the currently supported LTE split:

    eNB ID  = ECI >> 8
    Cell ID = ECI & 0xff

Do not assume PCI is globally unique.

A discovered cell should ultimately be identified using at least
EARFCN + PCI, with decoded cell identity added when available.

## Discovery architecture

Do not run the full PDSCH/SIB probe on every 100 kHz LTE raster point.

The intended discovery pipeline is:

    LTE band/range
        -> lightweight PSS/SSS search
        -> MIB verification
        -> candidate cell
        -> full probe
        -> SIB1/SIB5
        -> optional SIB5 expansion

MIB verification is required before treating a PSS candidate as a
discovered LTE cell.

## Existing probe architecture

The current implementation intentionally uses `pdsch_ue` as a bounded
single-frequency subprocess probe with structured JSON output.

This is an intermediate architecture.

Do not perform a large extraction/refactor of the PHY/probe engine as
part of unrelated work.

A reusable in-process probe API may be introduced later as a dedicated
refactoring phase.

## Build policy

The primary development platform is currently:

- Apple Silicon / ARM64;
- macOS;
- Homebrew;
- HackRF One;
- SoapySDR / SoapyHackRF.

The project is normally configured with:

    -DENABLE_GUI=OFF
    -DENABLE_SRSUE=OFF
    -DENABLE_SRSENB=OFF
    -DENABLE_SRSEPC=OFF
    -DENABLE_UHD=OFF
    -DENABLE_SOAPYSDR=ON

The required development targets are currently:

    pdsch_ue
    cell_search
    lte_scan

A full-tree macOS build is not currently an acceptance requirement.

The upstream tree contains Linux-specific components, including
`lib/src/system/sys_metrics_processor.cc`, which currently includes
`<sys/sysinfo.h>` and can break a full macOS build.

Do not expand a ltesurvey task into a general srsRAN macOS port unless
explicitly requested.

## Testing

Separate tests into:

1. software-only tests that the agent can run;
2. hardware tests requiring a real HackRF One.

Never claim that a hardware test passed unless it was actually executed
with hardware.

When hardware verification is required but unavailable, provide the
exact commands and expected invariants for the human operator to test.

Live RF observations such as PCI, SNR, signal strength and available
cells can change and must not be treated as immutable test constants.

## Change discipline

Before implementing a phase:

1. inspect the relevant existing code;
2. inspect `git status`;
3. inspect changes relative to the known-good baseline when appropriate;
4. understand existing behavior before refactoring it.

Prefer small, reviewable changes.

Do not combine unrelated cleanup or broad refactoring with a feature
phase.

Do not silently change existing CLI or JSON semantics.

If a task becomes substantially larger than expected, stop at a logical
boundary and document what remains instead of expanding the scope
without review.

## Git

Development takes place on the `ltesurvey` branch or feature branches
based on it.

The tag:

    ltesurvey-known-good-20260920

is the hardware-verified recovery/reference point.

Do not rewrite published history or force-push unless explicitly
requested.

Keep commits logically scoped and write commit messages describing the
functional change.

## Generated/local files

Do not commit local development artifacts, known-good source backups,
build output or archival patches that are intentionally excluded by
`.gitignore`.

## Documentation

When implementation changes architecture, CLI, JSON schema, installation
procedure, or important RF behavior, update the corresponding
documentation in `docs/ltesurvey/`.

Do not let the implementation and specification silently diverge.
