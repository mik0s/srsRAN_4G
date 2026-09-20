# Arinst SSA R3 B3 spectrum fixtures

Эти fixtures получены реальным Arinst SSA R3 с PRO firmware при сканировании LTE Band 3 downlink (1805–1880 MHz).

## Состав

- `b3-speed/LOG_4.zip` — Mode Speed, RBW 2.5 kHz, 800 points.
- `b3-precision/LOG_5.zip` — Mode Precision, RBW 25.0 kHz, 800 points.

Архивы сохраняют исходную структуру каталога Arinst, но intentionally не содержат `screencapture.bmp`, поскольку BMP не нужен для automated regression и существенно увеличивает размер репозитория.

После распаковки каждый bundle содержит:

```text
LOG_N/
  Spectrum1.csv
  Trace2.csv
  Trace3.csv
  Trace4.csv
```

`Trace2.csv`, `Trace3.csv` и `Trace4.csv` содержат `#Points 0`. Importer должен корректно игнорировать такие файлы.

## Ground truth

Известная LTE carrier в этом диапазоне:

```text
Band:       3
EARFCN:     1596
Frequency:  1844.6 MHz
Bandwidth:  10 MHz
```

Spectrum fixture не является IQ и не должна использоваться для PSS/SSS/PBCH/MIB decode. Её назначение — offline spectrum pre-discovery и формирование LTE raster hypotheses.

Подтверждающая IQ fixture находится в:

```text
tests/fixtures/lte/b3-earfcn1596/
```

## Integrity

Original CSV SHA-256:

```text
LOG_4/Spectrum1.csv  0adb8e0386efe26487ec55b06520f4100bb2845d6b719f8b5fddf5e5316bf18f
LOG_4/Trace2.csv     fa027431e3b541072f25c72d6cbd45ec687d7f23cc9ec87dc2ab0e7091006cd0
LOG_4/Trace3.csv     3f8e5efba28271d39bf26b07791194a91b9497333370ebb4bb4d4c87d76e46f7
LOG_4/Trace4.csv     0ccf8ec3c2fd2776f46372645a62601eeefc5845efa3d4a0c156766baf502482

LOG_5/Spectrum1.csv  810f2f4f56b4f86509ee620bd16b7aa4242e98496a28c1c535f6f558e2c7545d
LOG_5/Trace2.csv     fa027431e3b541072f25c72d6cbd45ec687d7f23cc9ec87dc2ab0e7091006cd0
LOG_5/Trace3.csv     3f8e5efba28271d39bf26b07791194a91b9497333370ebb4bb4d4c87d76e46f7
LOG_5/Trace4.csv     0ccf8ec3c2fd2776f46372645a62601eeefc5845efa3d4a0c156766baf502482
```
