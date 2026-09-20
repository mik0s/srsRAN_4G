# ltesurvey Phase 1 — Structured LTE Cell Discovery

## 1. Задача

Реализовать Phase 1 проекта `ltesurvey`: превратить `cell_search` в надёжный structured LTE discovery backend и добавить software-only regression test на реальной LTE IQ fixture, уже сохранённой в репозитории.

Репозиторий:

```text
mik0s/srsRAN_4G
```

Основная development branch:

```text
ltesurvey
```

Перед изменениями обязательно прочитать:

```text
AGENTS.md
docs/ltesurvey/SPEC.md
```

Эти документы являются authoritative. Особенно важно сохранить описанные там hardware-verified HackRF / SoapySDR stream lifecycle invariants.

Не начинать работу с большого refactoring. Сначала изучить текущую реализацию и определить минимальный безопасный набор изменений для Phase 1.

---

## 2. Текущий baseline

Hardware-verified reference tag:

```text
ltesurvey-known-good-20260920
```

Текущий `cell_search` уже:

- открывает RF один раз на scan;
- последовательно проходит LTE EARFCN range;
- выполняет PSS/SSS search;
- получает кандидатов для `N_id_2`;
- применяет существующий PSS candidate threshold;
- подтверждает кандидатов через PBCH/MIB;
- выводит в итоговый human-readable результат только успешно декодированные cells.

Текущее HackRF/SoapySDR поведение проверено на реальном оборудовании и не должно изменяться без необходимости.

Критические RF invariants описаны в `AGENTS.md` и `SPEC.md`. В частности:

- при фактическом изменении sample rate RX stream пересоздаётся;
- при фактическом изменении RX frequency RX stream пересоздаётся;
- active stream корректно останавливается/запускается при изменениях;
- после PSS/cell search RX должен быть остановлен до запуска MIB decoder;
- `rf_soapy_flush_buffer()` работает с количеством samples, а не bytes.

Не рефакторить `rf_soapy_imp.c` в рамках этой задачи.

---

## 3. Основная цель

Добавить deterministic machine-readable discovery output в `cell_search`.

Целевой интерфейс:

```bash
cell_search ... -J discovery.json
```

JSON должен представлять финальный MIB-confirmed discovery result.

Human-readable stdout/stderr можно сохранить для совместимости и диагностики, но automated consumers не должны зависеть от parsing этого вывода.

---

## 4. Семантика discovery result

PSS/SSS candidate не является подтверждённой LTE cell.

В `cells[]` может попасть только candidate, для которого успешно завершился PBCH/MIB decode.

```text
EARFCN
  -> PSS/SSS
  -> candidate
  -> MIB verification
  -> confirmed cell
  -> JSON cells[]
```

False PSS candidates не должны попадать в structured result.

---

## 5. Обязательные JSON fields

Top-level JSON должен содержать массив `cells`.

Для каждой MIB-confirmed cell необходимо выдавать как минимум:

```text
earfcn
band
frequency_hz
pci
nof_prb
bandwidth_mhz
nof_ports
```

Пример:

```json
{
  "cells": [
    {
      "earfcn": 1596,
      "band": 3,
      "frequency_hz": 1844600000,
      "pci": 346,
      "nof_prb": 50,
      "bandwidth_mhz": 10.0,
      "nof_ports": 2
    }
  ]
}
```

Integer fields должны записываться как JSON integers.

`frequency_hz` не следует получать обратным parsing отформатированной строки MHz. Нужно использовать существующий EARFCN/frequency mapping или внутреннее значение частоты и выполнять deterministic conversion.

`bandwidth_mhz` должен корректно соответствовать PRB:

```text
6   -> 1.4
15  -> 3
25  -> 5
50  -> 10
75  -> 15
100 -> 20
```

Допустимо добавить schema/version metadata, если это согласуется с текущей спецификацией, но не расширять Phase 1 до общего redesign result model.

---

## 6. Пустой результат

Успешный scan без MIB-confirmed LTE cells не является execution/internal error.

Он должен создавать valid JSON, например:

```json
{
  "cells": []
}
```

Не смешивать состояния:

```text
no confirmed cells
```

и:

```text
discovery backend execution failure
```

---

## 7. Deterministic output

Structured output должен быть deterministic для одинакового discovery result.

Сохранять scan order, если нет веской причины использовать другой stable ordering.

Не включать нестабильные runtime values в acceptance-critical result.

В частности, не считать стабильными regression constants:

```text
PSR
peak
CFO
current uncalibrated PSS power
```

Они могут оставаться diagnostic values.

Если diagnostic measurements позже добавляются в JSON, их семантика должна быть явно определена. Нельзя называть существующее некалиброванное power-like значение calibrated dBm, RSRP, RSRQ, RSSI или SINR.

---

## 8. Запись JSON

`-J <path>` должен создавать syntactically valid JSON.

Предпочтительная безопасная схема:

```text
temporary file
 -> complete write
 -> flush/close
 -> atomic rename to requested path
```

При невозможности создать, записать или завершить JSON файл программа должна возвращать понятную execution error, а не silently succeed.

Перед реализацией проверить существующий ltesurvey/probe JSON code и по возможности использовать его conventions/helpers.

Не добавлять крупную external JSON dependency только ради небольшого discovery result без необходимости.

---

## 9. CLI compatibility

Добавить:

```text
-J <path>
```

в `cell_search` и обновить usage/help.

Если `-J` не указан, существующее CLI поведение должно по возможности сохраниться.

Не удалять human-readable output в Phase 1.

Текущее поведение EARFCN range является half-open:

```text
-s 1596 -e 1597
```

сканирует EARFCN 1596.

Не менять эту семантику случайно.

---

## 10. Offline IQ regression fixture

В репозитории уже находится реальная LTE IQ fixture:

```text
tests/fixtures/lte/b3-earfcn1596/capture.cs8
tests/fixtures/lte/b3-earfcn1596/metadata.json
tests/fixtures/lte/b3-earfcn1596/README.md
```

Converter:

```text
tests/tools/cs8_to_fc32.py
```

Параметры committed fixture:

```text
format:              signed int8 interleaved I,Q
sample rate:         1920000 sample/s
duration:            4.5 s
complex samples:     8640000
size:                17280000 bytes
center frequency:    1844600000 Hz
band:                3
EARFCN:              1596
```

SHA-256:

```text
09c719f5208ca8060d3c1b2c300cb88bfe26e2df519d5120fff5de20410d335e
```

Fixture нормализована для эффективного использования CS8 dynamic range. Её amplitude не является calibrated RF amplitude.

---

## 11. Fixture conversion

Текущий srsRAN file RF backend принимает FC32.

Перед regression fixture преобразуется:

```bash
python3 tests/tools/cs8_to_fc32.py \
  tests/fixtures/lte/b3-earfcn1596/capture.cs8 \
  <temporary-file>.fc32
```

Resampling и filtering в CI не нужны: committed fixture уже имеет sample rate 1.92 Msps.

Не добавлять NumPy/SciPy dependency для conversion.

Сгенерированный FC32 является temporary test artifact и не должен попадать в Git.

---

## 12. Offline discovery invocation

Regression должен запускать настоящий `cell_search` discovery path через file RF backend.

Эквивалентная команда:

```bash
cell_search \
  -d file \
  -a "rx_file=<fixture.fc32>,base_srate=1920000" \
  -b 3 \
  -s 1596 \
  -e 1597 \
  -g 0 \
  -J <result.json>
```

Executable paths должны определяться из build/test environment, а не через developer-specific absolute paths.

HackRF для этого regression test не используется.

---

## 13. Ожидаемый regression result

Fixture должна давать ровно одну MIB-confirmed cell:

```text
EARFCN:        1596
Band:          3
Frequency:     1844600000 Hz
PCI:           346
PRB:           50
Bandwidth:     10 MHz
Antenna ports: 2
```

Regression test должен завершаться ошибкой, если:

- PCI 346 отсутствует;
- MIB decode неуспешен;
- EARFCN отличается;
- band отличается;
- frequency отличается;
- PRB отличается;
- bandwidth отличается;
- antenna-port count отличается;
- появляется дополнительная MIB-confirmed cell;
- JSON отсутствует;
- JSON malformed;
- структура `cells` не соответствует ожидаемой.

Нельзя ограничиваться grep human stdout по строке `Found CELL ID 346`. Проверять structured JSON.

Fixture специально позволяет проверять, что false PSS candidates не становятся confirmed cells.

Во время manual validation в записи наблюдался более слабый PSS candidate, например PCI 189, который не прошёл MIB verification. Такой PSS-only candidate не должен попадать в `cells[]`.

---

## 14. Реализация regression test

Добавить software-only regression test в существующую test/CI infrastructure.

Предпочтительно, чтобы тот же test можно было удобно запускать локально без GitHub Actions.

Использовать существующие project testing conventions, где это возможно.

Test должен выполнять:

```text
verify fixture integrity if practical
 -> convert CS8 to temporary FC32
 -> run cell_search with file RF backend
 -> request structured JSON
 -> parse JSON
 -> compare exact expected confirmed-cell set
 -> clean temporary files
```

Использовать temporary directory вместо фиксированных `/tmp` filenames, где это практично.

Cleanup должен происходить и при test failure.

Допустим standard-library Python helper/test. Не добавлять крупный test framework только ради этой проверки.

---

## 15. CI integration

Добавить offline fixture regression в существующий ltesurvey CI после build необходимых targets.

Не создавать второй redundant workflow, если текущий `.github/workflows/ltesurvey.yml` подходит для этой задачи.

Не создавать лишние duplicate runs для одного push/PR.

Fixture regression является software-only и не должен зависеть от:

- наличия HackRF;
- обнаружения физического HackRF через SoapyHackRF;
- Internet access во время test;
- текущего состояния live LTE network.

Test input — file RF backend.

---

## 16. Важное поведение file backend

Существующий file RF backend читает FC32 samples.

Для fixture:

```text
base_srate=1920000
```

Discovery sample rate также 1.92 Msps, поэтому decimation factor должен быть 1.

Не использовать простой integer boxcar decimator file backend для этой fixture.

Не изменять `rf_file` только ради прямой поддержки CS8 в Phase 1, если не обнаружена реальная blocking reason.

Static file физически не умеет retune между carriers. Поэтому fixture regression намеренно сканирует только один EARFCN:

```text
[1596, 1597)
```

Это не hardware retune test.

---

## 17. PSS -> MIB и конечный file stream

File backend не rewind'ит файл при stop/start RX.

PSS discovery и MIB verification последовательно потребляют один и тот же finite IQ stream. Для текущей fixture это ожидаемое поведение.

Экспериментально для этой записи проверено:

```text
1 s -> PSS найден, MIB EOF/fail
2 s -> PSS найден, MIB EOF/fail
3 s -> PSS найден, MIB EOF/fail
4 s -> MIB success
5 s -> MIB success
```

Committed fixture имеет длительность 4.5 s.

Не добавлять скрытый rewind/repeat в `rf_file` ради прохождения test.

---

## 18. Сохранение hardware behavior

Phase 1 не должен менять hardware-verified discovery lifecycle ради упрощения offline test.

В частности, не менять без необходимости:

```text
RF open-once semantics
retune semantics
sample-rate semantics
RX stop/start ordering between PSS and MIB
```

Если RF/backend change действительно становится необходим, остановиться и сначала описать причину, требуемое изменение и риски.

Hardware correctness важнее architectural cleanup.

---

## 19. Что не входит в Phase 1

Не реализовывать в рамках этой задачи:

- `lte-scan --band 3`;
- multi-band scanning;
- SIB1/SIB5 aggregation;
- PLMN database;
- PLMN/operator lookup;
- SIB5 graph traversal;
- packaging;
- installation layout redesign;
- measurement calibration;
- RSRP/RSRQ/RSSI/SINR redesign;
- большой `pdsch_ue` refactor;
- общий RF abstraction refactor;
- полный macOS port upstream srsRAN;
- direct CS8 support в production RF backend без реальной необходимости;
- unrelated cleanup.

Patch должен оставаться focused.

---

## 20. Build acceptance

Должны продолжать успешно собираться:

```text
pdsch_ue
cell_search
lte_scan
```

Использовать существующую ltesurvey CMake/CI configuration.

Успешный full-tree macOS build не является требованием.

---

## 21. Software acceptance criteria

Phase 1 software work считается выполненным, когда одновременно выполняются условия:

1. `cell_search -J <path>` создаёт valid deterministic JSON.
2. JSON содержит только MIB-confirmed cells.
3. Все обязательные cell fields присутствуют.
4. Empty scan создаёт valid `"cells": []`.
5. Legacy use без `-J` остаётся рабочим.
6. Committed B3 fixture автоматически преобразуется и обрабатывается.
7. Fixture result содержит ровно одну confirmed cell.
8. Эта cell имеет EARFCN 1596, band 3, frequency 1844600000 Hz, PCI 346, 50 PRB, 10 MHz и 2 ports.
9. Regression проверяет JSON, а не human stdout.
10. Regression запускается локально.
11. Regression запускается существующим CI.
12. Existing ltesurvey CI остаётся green.
13. Для software CI не требуется SDR hardware.
14. Не появляется NumPy/SciPy dependency.
15. Fixture amplitude нигде не выдаётся за calibrated RF measurement.

---

## 22. Hardware verification после реализации

Coding agent не должен утверждать, что hardware verification выполнена, если у него фактически нет доступа к HackRF One.

После реализации предоставить точные команды для manual hardware validation.

Как минимум нужен короткий B3 range test:

```text
real HackRF
 -> sequential retune
 -> PSS/SSS
 -> MIB
 -> structured JSON
```

Команда должна также позволять проверить отсутствие stale duplicated results после retune.

Не hardcode текущий live PCI или network identity как вечный hardware acceptance constant: live network может измениться.

Offline fixture, напротив, имеет фиксированные expected values.

---

## 23. Documentation

Если implementation semantics отличаются от текущего `SPEC.md`, обновить документацию.

Не допускать незаметного расхождения implementation и specification.

Если архитектурная семантика не меняется, избегать лишнего documentation churn.

---

## 24. Development procedure

Работать в focused feature branch, например:

```text
feature/structured-discovery
```

Перед изменениями проверить:

```bash
git status
git branch --show-current
git log --oneline -5
```

Перед реализацией изучить актуальный код.

Вероятно relevant:

```text
lib/examples/cell_search.c
existing ltesurvey/probe JSON implementation
tests/
.github/workflows/ltesurvey.yml
```

Не предполагать точные helper paths без inspection.

Рекомендуемый порядок:

```text
inspect
 -> design minimal patch
 -> implement structured JSON
 -> build
 -> add fixture regression
 -> run regression locally
 -> integrate CI
 -> run software tests
 -> review diff
 -> document manual hardware verification
```

Commits должны быть логически ограниченными. Возможное разбиение:

```text
feat: add structured JSON output to cell_search
test: add offline LTE discovery regression
ci: run LTE discovery regression
```

Если repository structure делает другое разбиение более естественным, допустимо адаптировать.

---

## 25. Stop conditions

Если реализация неожиданно требует:

- значительных RF backend changes;
- переписывания `cell_search`;
- крупных srsRAN PHY changes;
- изменения HackRF/Soapy stream lifecycle;
- новой крупной external dependency;
- изменения самой fixture;
- изменения expected PCI/PRB/ports только ради прохождения test;

остановиться на логической границе.

В отчёте описать:

```text
что обнаружено
почему текущего design недостаточно
какое изменение требуется
риски
предлагаемый следующий шаг
```

Не расширять scope молча.

---

## 26. Финальный отчёт

После выполнения предоставить:

- список изменённых файлов;
- краткое описание architecture/implementation;
- точную реализованную JSON schema;
- добавленные tests;
- команды локального запуска;
- результаты tests;
- CI changes;
- известные ограничения;
- точные команды HackRF hardware verification;
- созданные commits.

Отдельно явно указать, выполнялась ли hardware verification.

Успешный CI test на offline IQ fixture нельзя называть HackRF hardware verification.
