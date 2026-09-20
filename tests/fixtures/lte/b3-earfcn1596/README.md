# LTE Band 3 / EARFCN 1596 discovery fixture

This directory contains a real LTE downlink IQ recording for software-only
regression testing of the ltesurvey discovery path.

## Expected cell

- LTE band: 3
- EARFCN: 1596
- Center frequency: 1844.6 MHz
- PCI: 346
- MIB bandwidth: 50 PRB / 10 MHz
- Antenna ports: 2

Exactly one MIB-confirmed cell is expected.

## IQ format

`capture.cs8` contains interleaved signed 8-bit IQ samples:

    I0 Q0 I1 Q1 I2 Q2 ...

Properties:

- sample format: signed int8
- sample rate: 1.92 Msps
- duration: 4.5 seconds
- complex samples: 8,640,000
- file size: 17,280,000 bytes
- IQ order: I,Q

SHA-256:

    09c719f5208ca8060d3c1b2c300cb88bfe26e2df519d5120fff5de20410d335e

## Provenance

The fixture was derived from a real HackRF One capture of the LTE carrier at
1844.6 MHz.

Original capture parameters:

- center frequency: 1844.6 MHz
- sample rate: 15.36 Msps
- RF amplifier: disabled
- LNA gain: 32
- VGA gain: 32

The original high-rate CS8 recording was converted to 1.92 Msps using proper
anti-alias filtering and decimation. The resulting signal was normalized before
being quantized to the repository CS8 representation.

The normalization exists only to use the available 8-bit fixture dynamic range
efficiently. Absolute RF amplitude is not preserved. Values derived from this
fixture must not be interpreted as calibrated dBm, RSRP, RSSI, or similar RF
measurements.

## Playback

The current srsRAN file RF backend consumes FC32 samples. Convert the fixture
before running the test:

    python3 tests/tools/cs8_to_fc32.py \
      tests/fixtures/lte/b3-earfcn1596/capture.cs8 \
      /tmp/b3-earfcn1596.fc32

Then run a single-EARFCN discovery:

    ./build/lib/examples/cell_search \
      -d file \
      -a "rx_file=/tmp/b3-earfcn1596.fc32,base_srate=1920000" \
      -b 3 \
      -s 1596 \
      -e 1597 \
      -g 0

The `cell_search` EARFCN range is half-open, therefore EARFCN 1596 alone is
specified as `[1596, 1597)`.

The static file fixture is intentionally a single-frequency test. It does not
simulate hardware retuning and does not replace HackRF/SoapySDR retune
regression testing.

## Regression semantics

The authoritative regression result is the structured discovery output, not
human-readable stdout.

The test must fail if:

- the expected PCI 346 cell is absent;
- MIB decoding fails;
- PRB/bandwidth differs from the expected value;
- antenna-port count differs from the expected value;
- an additional MIB-confirmed cell is reported.

PSR, peak, CFO, and uncalibrated power-like values are diagnostic and are not
stable acceptance constants.
