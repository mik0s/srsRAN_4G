# ltesurvey — техническая спецификация

## 1. Назначение документа

Этот документ определяет требования, архитектурное направление, ограничения и этапы разработки `ltesurvey` — инструмента пассивного исследования LTE-сетей с использованием SDR.

Основная текущая аппаратная платформа:

- HackRF One;
- SoapySDR / SoapyHackRF;
- macOS ARM64.

Проект разрабатывается внутри fork `srsRAN_4G`.

Документ является основной технической спецификацией `ltesurvey`. Перед существенными изменениями реализации необходимо также прочитать корневой `AGENTS.md`.

---

# 2. Цель проекта

Необходимо получить законченный пользовательский инструмент:

```text
lte-scan
```

который позволяет пассивно исследовать LTE downlink и получать структурированную информацию о доступных LTE-сетях и ячейках.

Типичный сценарий:

```bash
lte-scan --band 3,7,8,20
```

Результат должен позволять ответить как минимум на следующие вопросы:

- какие LTE carriers доступны;
- на каких частотах и EARFCN они работают;
- к каким LTE bands они относятся;
- какие PCI обнаружены;
- какая ширина полосы используется;
- какой PLMN или несколько PLMN транслирует ячейка;
- каким операторам соответствуют PLMN;
- какой TAC используется;
- какой ECI;
- какой eNodeB ID;
- какой Cell ID;
- какие inter-frequency carriers объявлены через SIB5;
- каковы доступные и корректно интерпретируемые RF/PHY показатели качества.

Инструмент должен быть пригоден как для интерактивной работы человека, так и для дальнейшей автоматизированной обработки результатов.

---

# 3. Scope

## 3.1. В scope

Проект выполняет пассивный LTE downlink survey.

В scope входят:

- поиск LTE carriers;
- PSS/SSS detection;
- определение PCI;
- MIB decoding;
- определение LTE bandwidth / PRB;
- SIB1 decoding;
- SIB5 decoding;
- при необходимости использование других публичных SIB;
- получение PLMN;
- offline PLMN → operator lookup;
- TAC;
- ECI;
- eNodeB ID;
- Cell ID;
- LTE band;
- EARFCN;
- downlink frequency;
- passive RF/PHY measurements;
- поиск дополнительных carriers через SIB5;
- HackRF One;
- SoapySDR;
- human-readable output;
- machine-readable JSON;
- установка и дистрибуция законченного инструмента.

## 3.2. Не является целью текущего проекта

Не требуется для текущей версии:

- LTE attach;
- регистрация в сети;
- передача uplink;
- rogue eNodeB;
- IMSI catcher;
- jamming;
- перехват пользовательского трафика;
- получение приватных данных абонентов;
- декодирование пользовательских credentials;
- полноценная реализация LTE UE;
- полноценная реализация LTE eNodeB/EPC.

`ltesurvey` должен оставаться пассивным инструментом исследования публично транслируемой LTE-информации.

---

# 4. Исходный проект

Разработка ведётся в fork:

```text
srsRAN_4G
```

Git remotes:

```text
origin   -> пользовательский fork
upstream -> официальный srsRAN_4G
```

Основная development branch:

```text
ltesurvey
```

`master` используется как стабильная точка относительно текущего hardware-verified baseline.

---

# 5. Hardware-verified baseline

Контрольная точка проекта:

```text
ltesurvey-known-good-20260920
```

Этот tag нельзя перемещать или пересоздавать.

На этой версии реальным HackRF One подтверждены:

- сборка необходимых `ltesurvey` targets на macOS ARM64;
- стабильная работа HackRF через SoapySDR;
- корректное изменение sample rate;
- корректный retune между LTE EARFCN;
- отсутствие ранее наблюдавшегося stale RX stream при последовательном сканировании;
- обнаружение LTE cell;
- MIB decoding;
- SIB1 decoding;
- SIB5 decoding;
- формирование structured probe JSON.

Контрольная LTE cell во время проверки:

```text
Band:          B3
EARFCN:        1596
Frequency:     1844.6 MHz
PCI:           346
PRB:           50
Bandwidth:     10 MHz

PLMN:
    250-02
    250-11

TAC:           47825
ECI:           201034970
eNB ID:        785292
Cell ID:       218
```

Во время последней проверки SIB5 содержал:

```text
200
1820
2850
3048
3750
6350
```

Эти live RF значения не должны использоваться как неизменяемые unit-test constants. Состав сети, PCI, RF metrics и доступность carriers могут изменяться.

Baseline означает исправность тракта:

```text
HackRF
 -> SoapySDR
 -> synchronization
 -> cell discovery
 -> MIB
 -> PDSCH
 -> SIB1/SIB5
 -> structured result
```

---

# 6. Текущая среда разработки

Проверенная среда:

```text
Platform:       Apple Silicon / ARM64
OS:             macOS
Compiler:       AppleClang
Build system:   CMake
Package system: Homebrew

SDR:            HackRF One
RF abstraction: SoapySDR
Driver:         SoapyHackRF
```

На момент формирования baseline использовались приблизительно:

```text
macOS           26.6.2
AppleClang      21
CMake           4.1.2
SoapySDR        0.8.1
SoapyHackRF     0.3.4
HackRF firmware 2.4.0
```

Версии являются описанием проверенной среды, а не обязательным hard dependency без дополнительного обоснования.

---

# 7. Сборка

Текущая конфигурация разработки:

```bash
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_GUI=OFF \
  -DENABLE_SRSUE=OFF \
  -DENABLE_SRSENB=OFF \
  -DENABLE_SRSEPC=OFF \
  -DENABLE_UHD=OFF \
  -DENABLE_SOAPYSDR=ON
```

Основные необходимые targets:

```text
pdsch_ue
cell_search
lte_scan
```

Пример:

```bash
make -j$(sysctl -n hw.ncpu) \
  pdsch_ue \
  cell_search \
  lte_scan
```

Полная сборка всего upstream `srsRAN_4G` на macOS ARM64 в настоящий момент не является acceptance criterion.

В upstream tree существуют Linux-specific компоненты. В частности:

```text
lib/src/system/sys_metrics_processor.cc
```

использует:

```c
#include <sys/sysinfo.h>
```

что ломает full-tree build на macOS.

Не следует превращать разработку `ltesurvey` в полный macOS port всего `srsRAN_4G`, если это не является отдельной явно поставленной задачей.

---

# 8. Критические HackRF / SoapySDR invariants

В текущей реализации присутствуют исправления, подтверждённые реальным HackRF.

Их необходимо считать критической частью baseline.

## 8.1. Sample-rate change

При фактическом изменении RX sample rate существующий SoapySDR RX stream должен быть пересоздан:

```text
close old RX stream
 -> change sample rate
 -> create RX stream again
```

Это требуется даже в случае, если stream в момент изменения находится в inactive state.

Если stream был active до изменения, после пересоздания его необходимо снова активировать.

Причиной исправления была нестабильность HackRF/Soapy при переходе, в частности:

```text
1.92 MHz -> 15.36 MHz
```

для LTE 10 MHz / 50 PRB.

## 8.2. Frequency retune

При фактическом изменении RX frequency существующий Soapy RX stream также должен быть пересоздан независимо от его active state.

До исправления последовательный LTE raster scan мог получать stale/duplicated samples после retune.

Симптомом были одинаковые:

- PCI;
- PSR;
- peak;
- CFO;

на разных соседних EARFCN, а также false MIB results.

Изменение settle delay или пересоздание только cell-search object проблему не устраняло.

## 8.3. PSS → MIB RX lifecycle

После завершения PSS/cell search RX stream должен быть остановлен перед тем, как MIB decoder начинает собственную RX operation.

Концептуально:

```c
srsran_ue_cellsearch_scan(...);

srsran_rf_stop_rx_stream(&rf);

rf_mib_decoder(...);
```

## 8.4. Flush buffer

`rf_soapy_flush_buffer()` должен работать с количеством samples, а не количеством bytes.

## 8.5. Правило изменения RF backend

Изменения:

```text
lib/src/phy/rf/rf_soapy_imp.c
```

считать high-risk.

Не следует рефакторить этот код только ради stylistic cleanup.

После изменения RF backend требуется повторная hardware verification.

---

# 9. Известное поведение HackRF

В процессе разработки наблюдалось состояние, при котором:

- USB device продолжает существовать;
- Soapy/HackRF API продолжает работать;
- но ранее известные LTE carriers перестают обнаруживаться.

В таком случае необходимо учитывать возможность некорректного состояния RF hardware/firmware.

Перед диагностикой software regression следует попробовать физический:

```text
HackRF power-cycle / USB reconnect
```

Отсутствие cell в конкретном запуске само по себе не доказывает regression.

---

# 10. LTE discovery architecture


## 10.0. Spectrum pre-discovery и Arinst SSA R3

Помимо прямого LTE raster scan через SDR, `ltesurvey` должен поддерживать offline spectrum pre-discovery по сохранённым логам спектроанализатора Arinst SSA R3 (PRO).

Arinst в этом сценарии не заменяет SDR и не предоставляет LTE PHY/IQ decode. Его задача — быстро сузить пространство поиска: определить широкополосные области, похожие на LTE carriers, сопоставить их с LTE band/raster hypotheses и сформировать короткий список частот/EARFCN для последующей проверки HackRF.

Целевая цепочка:

```text
Arinst SSA R3
  -> saved LOG_N directory
  -> Spectrum*.csv / Trace*.csv
  -> ltesurvey spectrum pre-discovery
  -> ranked LTE band / center / bandwidth / EARFCN hypotheses
  -> HackRF IQ capture or live probe
  -> PSS/SSS
  -> MIB
  -> confirmed LTE cell
```

### Формат входа Arinst

Естественной единицей импорта считать каталог одного сохранения Arinst, например:

```text
LOG_5/
  Spectrum1.csv
  Trace2.csv
  Trace3.csv
  Trace4.csv
  screencapture.bmp
```

Importer должен:

- принимать каталог сохранения;
- находить `Spectrum*.csv` и `Trace*.csv`;
- читать metadata из комментариев CSV, включая mode, bandwidth/RBW, points и RF input, если они присутствуют;
- поддерживать как `#Spectrum no. N`, так и `#Trace no. N`;
- игнорировать корректные пустые trace-файлы с `#Points 0`, не считая это ошибкой всего bundle;
- не требовать `screencapture.bmp` для анализа; screenshot является optional companion artifact;
- приводить вход к общей внутренней модели `frequency_hz + amplitude_dbm + metadata`;
- не считать amplitude Arinst абсолютным calibrated LTE RSRP/RSRQ/RSSI/SINR.

Поддержка ZIP как production input необязательна. Committed fixtures могут храниться в компактном архиве и распаковываться test helper'ом во временный каталог.

### Discovery semantics

Spectrum detector должен искать не отдельные узкополосные пики, а широкополосные LTE-like hypotheses.

Минимальный подход:

```text
parse samples
 -> normalize / interpolate irregular frequency grid if needed
 -> estimate local noise floor
 -> identify wideband energy regions
 -> score LTE bandwidth hypotheses: 1.4/3/5/10/15/20 MHz
 -> constrain centers to valid LTE band/raster hypotheses
 -> resolve overlapping/nested hypotheses
 -> rank candidates
```

Нельзя использовать один фиксированный absolute dBm threshold для всех sweep'ов. Speed/Precision и RF conditions могут давать разные absolute levels при сохранении общей формы спектра.

Для overlapping hypotheses требуется suppression/resolution: например, вложенное 5 MHz окно внутри более убедительной 10 MHz carrier hypothesis не должно автоматически становиться отдельной cell.

Spectrum candidate не является подтверждённой LTE cell. Даже high-confidence candidate должен считаться только целью для SDR verification, пока не пройдены PSS/SSS + MIB.

### Первые реальные fixtures

В репозитории находятся два независимых B3 sweep одного диапазона 1805–1880 MHz:

```text
tests/fixtures/arinst/b3-speed/LOG_4.zip
tests/fixtures/arinst/b3-precision/LOG_5.zip
```

Они содержат оригинальную структуру CSV-каталога Arinst без BMP screenshot, чтобы не увеличивать repository size без необходимости.

`LOG_4`:

```text
Mode:       Speed
RBW:        2.5 kHz
Points:     800
RF input:   15 dB
Range:      approximately 1805.076–1879.976 MHz
```

`LOG_5`:

```text
Mode:       Precision
RBW:        25.0 kHz
Points:     800
RF input:   15 dB
Range:      approximately 1805.064–1880.111 MHz
```

В обоих bundle `Trace2.csv`, `Trace3.csv` и `Trace4.csv` являются пустыми файлами с `#Points 0`. Это intentional regression case для importer.

### Ground truth и связь с IQ fixture

Оба Arinst B3 sweep должны позволять spectrum detector'у сформировать высокоранговую hypothesis в области известной LTE carrier:

```text
Band:       B3
EARFCN:     1596
DL:         1844.6 MHz
BW:         approximately 10 MHz
```

Точная граница/центр, оценённые непосредственно из amplitude trace, могут немного смещаться. Acceptance не должен требовать arbitrary measured center ровно 1844.600 MHz.

Правильный regression criterion:

- после band/raster hypothesis scoring в списке кандидатов присутствует B3 EARFCN 1596;
- он находится среди high-ranked candidates;
- detector не утверждает, что это confirmed cell;
- существующая IQ fixture `tests/fixtures/lte/b3-earfcn1596/capture.cs8` затем подтверждает тот же EARFCN через настоящий LTE PHY/MIB path.

Таким образом, две fixture-группы образуют связанный regression chain:

```text
Arinst spectrum fixture
  -> spectrum candidate: B3 / EARFCN 1596
  -> HackRF IQ fixture
  -> MIB-confirmed B3 / EARFCN 1596 / PCI 346 / 10 MHz
```

Точное ranking threshold и confidence model следует зафиксировать после реализации baseline detector и проверки на обоих sweep, не подгоняя алгоритм под один файл.


### Field cross-calibration HackRF по Arinst

Arinst SSA R3 без tracking generator может использоваться как field reference для привязки относительных HackRF measurements к приблизительной шкале dBm по сохранённому `LOG_N`.

Это не является лабораторной или метрологической калибровкой. Цель — получить reproducible local correction для конкретного HackRF, его gain settings, antenna/cable path, частотного диапазона и условий съёмки.

Целевая схема:

```text
Arinst LOG_N spectrum
        +
HackRF IQ / spectrum measurement
        ↓
compare equivalent frequency windows
        ↓
derive local correction profile
        ↓
estimated HackRF power in dBm
```

Критическое требование: сравнивать необходимо одну и ту же физическую величину в эквивалентной полосе.

Нельзя напрямую вычитать:

```text
Arinst peak bin @ RBW 25 kHz
minus
HackRF total IQ power over 10 MHz
```

Такие значения имеют разную bandwidth semantics и не образуют корректный calibration offset.

Предпочтительный подход:

- привести Arinst trace и HackRF measurement к общей frequency grid или общим channel windows;
- использовать одинаковые frequency boundaries;
- сравнивать PSD-like или integrated/channel-power-like величины с явно определённой bandwidth semantics;
- фиксировать HackRF `LNA`, `VGA`, `AMP`, sample rate и другие gain-affecting settings;
- фиксировать Arinst mode, RBW, RF input setting и диапазон;
- снимать reference и HackRF measurement максимально близко по времени;
- по возможности использовать ту же антенну, кабель и положение;
- если RF path различается, считать полученную correction profile привязанной к конкретной полевой конфигурации, а не только к самому HackRF.

Минимальная модель correction:

```text
offset(f, gain_profile) =
    reference_power_dbm(f, bandwidth)
  - hackrf_relative_power_dbfs(f, bandwidth)
```

После этого:

```text
estimated_power_dbm =
    hackrf_relative_power_dbfs + interpolated_offset
```

Один глобальный offset на весь диапазон использовать нельзя без отдельного подтверждения. Correction profile должен как минимум учитывать frequency dependence и HackRF gain profile.

Возможная структура profile:

```json
{
  "reference_source": "arinst_ssa_r3",
  "reference_mode": "field_cross_calibration",
  "hackrf": {
    "lna_gain_db": 32,
    "vga_gain_db": 20,
    "amp": false,
    "sample_rate_hz": 15360000
  },
  "reference": {
    "mode": "Precision",
    "rbw_hz": 25000,
    "rf_input_db": 15
  },
  "points": [
    {
      "frequency_hz": 1844600000,
      "bandwidth_hz": 10000000,
      "offset_db": -38.0
    }
  ]
}
```

Точная schema является implementation detail следующего этапа и может быть уточнена после первого реального paired measurement.

Machine-readable output должен явно отличать:

```text
rf_metric_calibrated: false
```

от приблизительной field-referenced оценки. Не следует устанавливать `calibrated=true` только потому, что применён Arinst-derived offset.

Предпочтительные поля:

```text
estimated_power_dbm
power_reference = "arinst_ssa_r3"
power_reference_mode = "field_cross_calibration"
power_reference_profile = ...
```

Также следует хранить uncertainty/quality metadata, когда появится обоснованная модель ошибки.

Важно: даже после такой cross-calibration `estimated_power_dbm` не является автоматически LTE RSRP.

Для RSRP требуется отдельный LTE-aware measurement path по reference signal resource elements:

```text
calibrated/field-referenced IQ amplitude
        +
LTE CRS extraction
        ↓
estimated RSRP
```

Поэтому нельзя переименовывать broadband/channel power в `rsrp_dbm`, `rsrq_db`, `rssi_dbm` или `sinr_db` без корректной LTE PHY semantics.

Первый practical experiment для B3 рекомендуется строить на paired captures вокруг известной carrier:

```text
B3
EARFCN 1596
1844.6 MHz
10 MHz
```

Arinst `LOG_N` и HackRF measurement должны быть сняты максимально близко по времени при документированных gain/RF settings. Этот эксперимент предназначен для проверки feasibility и repeatability field cross-calibration, а не для установления метрологической точности прибора.



Главное архитектурное требование:

**не запускать полный PDSCH/SIB probe на каждой точке LTE 100 kHz raster.**

Discovery pipeline должен иметь вид:

```text
LTE band / EARFCN range
        |
        v
lightweight PSS/SSS search
        |
        v
candidate
        |
        v
MIB verification
        |
        v
confirmed LTE cell
        |
        v
full probe
        |
        +--> SIB1
        |
        +--> SIB5
        |
        v
structured cell result
```

PSS candidate без успешного MIB нельзя считать подтверждённой LTE cell.

---

# 11. Текущий cell_search

`cell_search` является upstream-derived LTE raster scanner.

Основная логика:

```text
open RF once
 -> obtain EARFCN range
 -> set lightweight sample rate
 -> scan LTE raster
 -> PSS/SSS
 -> candidates
 -> MIB verification
```

Используется:

```text
srsran_ue_cellsearch_scan()
```

`found_cells[3]` позволяет получать кандидата для каждого `N_id_2`.

Для дальнейшей обработки должны использоваться только MIB-confirmed candidates.

`cell_search` не является wideband FFT scanner.

Он последовательно проходит LTE raster.

---

# 12. Structured discovery backend

Следующий архитектурный шаг — превратить `cell_search` в чистый discovery backend.

Он должен уметь выдавать machine-readable результат, например через:

```bash
cell_search ... -J discovery.json
```

JSON должен содержать только подтверждённые через MIB LTE candidates.

Минимальные поля candidate:

```json
{
  "earfcn": 1596,
  "band": 3,
  "frequency_hz": 1844600000,
  "pci": 346,
  "nof_prb": 50,
  "bandwidth_mhz": 10.0
}
```

Допускаются diagnostic fields:

```text
PSR
peak
CFO
PSS power
```

если их семантика корректно определена.

Diagnostic candidate и confirmed cell должны быть логически различимы.

---

# 13. Probe architecture

Текущий `pdsch_ue` используется как bounded single-frequency probe.

Пример:

```bash
pdsch_ue \
  -I soapy \
  -a "driver=hackrf" \
  -f 1844600000 \
  -g 40 \
  -X 3 \
  -J result.json \
  -n 5000 \
  -Q
```

Это является осознанной промежуточной архитектурой.

Полный lifecycle `pdsch_ue` в настоящий момент сильно связан с:

- RF;
- ue_sync;
- MIB;
- PDSCH;
- global state;
- counters;
- CLI;
- network/GUI legacy functionality.

Поэтому не следует выполнять большой extraction/refactoring только ради превращения probe в библиотеку на ранних этапах.

В будущем допустима архитектура:

```text
lte_probe_run()
```

или отдельный reusable probe engine, но это должно быть отдельным этапом.

---

# 14. Probe status semantics

Результат single-frequency probe должен иметь однозначные состояния.

## no_cell

```text
MIB не получен
```

## partial

```text
MIB получен
SIB1 не получен
```

## ok

```text
MIB получен
SIB1 получен
```

SIB5 является optional.

Отсутствие SIB5 не превращает `ok` в failure.

---

# 15. Probe exit codes

Минимальная семантика:

```text
0 -> ok или partial
2 -> no_cell
other -> execution/internal error
```

Machine-readable JSON является authoritative result.

stdout/stderr могут использоваться для human diagnostics.

---

# 16. Текущий probe JSON

Пример hardware-verified результата:

```json
{
  "status": "ok",
  "frequency_hz": 1844600000,
  "have_mib": true,
  "have_sib1": true,
  "have_sib5": true,
  "earfcn": 1596,
  "band": 3,
  "pci": 346,
  "nof_prb": 50,
  "bandwidth_mhz": 10.0,
  "snr_db": 3.9,
  "rf_metric": 28.7,
  "rf_metric_calibrated": false,
  "tac": 47825,
  "eci": 201034970,
  "enb_id": 785292,
  "cell_id": 218,
  "plmns": [
    {
      "mcc": 250,
      "mnc": 2,
      "mnc_digits": 2
    },
    {
      "mcc": 250,
      "mnc": 11,
      "mnc_digits": 2
    }
  ],
  "neighbors": [
    2850,
    3048,
    200,
    1820,
    3750,
    6350
  ]
}
```

Пример отсутствия cell:

```json
{
  "status": "no_cell",
  "frequency_hz": 811000000
}
```

JSON должен записываться безопасно, предпочтительно:

```text
temporary file
 -> complete write
 -> atomic rename
```

---

# 17. SIB decoding

Текущая реализация умеет декодировать публичную LTE System Information.

Ранее подтверждено получение:

```text
SIB1
SIB2
SIB3
SIB5
SIB6
SIB7
```

Особенно важны для `ltesurvey`:

```text
SIB1
SIB5
```

## SIB1

Используется как минимум для:

- PLMN;
- TAC;
- ECI;
- Cell ID;
- SI scheduling;
- systemInfoValueTag.

## SIB5

Используется для получения inter-frequency LTE carrier information.

Важно:

SIB5 означает, что сеть объявляет carrier для reselection.

Это **не гарантирует**, что carrier:

- сейчас физически принимается;
- относится к той же eNodeB;
- содержит доступную cell в текущей точке;
- обязательно будет успешно декодирован HackRF.

---

# 18. System Information scheduling

Следует использовать фактический SI schedule из SIB1:

```text
schedulingInfoList
si-WindowLength
```

Нельзя предполагать фиксированное расписание SIB.

Duplicate suppression для SIB должен учитывать:

```text
systemInfoValueTag
```

---

# 19. LTE identity model

## 19.1. PCI

PCI:

```text
0..503
```

не является глобально уникальным идентификатором cell.

Минимальный discovery key:

```text
(EARFCN, PCI)
```

После получения SIB1 желательно использовать ECI как более сильную идентичность.

## 19.2. ECI

Текущий LTE split:

```text
eNB ID  = ECI >> 8
Cell ID = ECI & 0xff
```

Контрольные значения:

```text
ECI 201034970
 -> eNB ID 785292
 -> Cell ID 218
```

и:

```text
ECI 201035002
 -> eNB ID 785292
 -> Cell ID 250
```

Это позволяет, например, установить, что две cells могут принадлежать одному eNodeB.

---

# 20. PLMN representation

PLMN нельзя хранить только как integer.

Необходимо сохранить:

```text
MCC
MNC
MNC digit count
```

Пример:

```json
{
  "mcc": 250,
  "mnc": 2,
  "mnc_digits": 2
}
```

Human representation:

```text
250-02
```

Должны поддерживаться:

- 2-digit MNC;
- 3-digit MNC;
- leading zero;
- несколько PLMN в одной cell.

Нельзя произвольно выбирать один PLMN как primary.

---

# 21. Offline PLMN database

`lte-scan` должен уметь локально преобразовывать:

```text
MCC + MNC
```

в operator name.

Runtime Internet dependency не допускается.

Предпочтительная структура:

```text
<prefix>/share/ltesurvey/mcc-mnc.json
```

или эквивалентный generated compact format.

База должна иметь:

- документированный источник;
- redistribution-compatible license;
- дату/версию данных;
- поддержку 2/3 digit MNC;
- корректную обработку leading zero.

При неизвестном PLMN scan не должен завершаться ошибкой.

Например:

```text
250-99 (unknown)
```

Raw PLMN всегда должен сохраняться независимо от operator lookup.

---

# 22. EARFCN mapping

Поддерживаемые на текущем этапе LTE bands:

```text
1
3
7
8
20
28
38
40
41
```

Используется стандартное преобразование:

```text
F_DL = F_DL_low + 0.1 * (N_DL - N_Offs-DL)
```

Текущая таблица:

```cpp
static const lte_band_info bands[] = {
    {1,  2110.0, 0,     0,     599},
    {3,  1805.0, 1200,  1200,  1949},
    {7,  2620.0, 2750,  2750,  3449},
    {8,   925.0, 3450,  3450,  3799},
    {20,  791.0, 6150,  6150,  6449},
    {28,  758.0, 9210,  9210,  9659},
    {38, 2570.0, 37750, 37750, 38249},
    {40, 2300.0, 38650, 38650, 39649},
    {41, 2496.0, 39650, 39650, 41589},
};
```

API:

```cpp
int lte_frequency_to_earfcn(
    double frequency_hz,
    uint8_t band,
    uint32_t* earfcn);

int lte_earfcn_to_frequency(
    uint32_t earfcn,
    uint8_t* band,
    double* frequency_hz);

double lte_prb_to_bandwidth_mhz(
    uint16_t nof_prb);

int lte_band_earfcn_range(
    uint8_t band,
    uint32_t* first_earfcn,
    uint32_t* last_earfcn);
```

---

# 23. PRB → bandwidth

Необходима стандартная mapping:

```text
6 PRB   -> 1.4 MHz
15 PRB  -> 3 MHz
25 PRB  -> 5 MHz
50 PRB  -> 10 MHz
75 PRB  -> 15 MHz
100 PRB -> 20 MHz
```

---

# 24. lte-scan — конечный CLI

Публичный пользовательский executable:

```text
lte-scan
```

Пользователь не должен вручную координировать несколько внутренних программ.

Внутри реализации допустимо использовать:

```text
cell_search
pdsch_ue
```

как subprocess backends на промежуточном этапе.

---

# 25. Требуемые CLI scenarios

Минимально:

```bash
lte-scan
```

```bash
lte-scan --band 3
```

```bash
lte-scan --band 3,7,8,20
```

```bash
lte-scan --earfcn 1596
```

```bash
lte-scan --earfcn 1596,3750
```

```bash
lte-scan --seed 1596
```

```bash
lte-scan --json survey.json
```

```bash
lte-scan --verbose
```

```bash
lte-scan --check
```

```bash
lte-scan --version
```

Допускается сохранение advanced parameters:

```text
--gain
--attempts
--subframes
--retries
```

---

# 26. Default scan

В законченной версии:

```bash
lte-scan
```

должен выполнять разумный default scan для поддерживаемых/настроенных LTE bands.

Точный default band set должен быть явно документирован и не должен быть скрытым platform-specific поведением.

---

# 27. Human-readable output

Целевой формат:

```text
Band EARFCN  DL MHz   PCI  BW    PLMN             Operator        TAC    eNB     Cell  SNR
B3   1596    1844.6   346  10M   250-02,250-11    MegaFon/Yota    47825  785292  218   3.9
B8   3750     955.0   104  10M   250-02,250-11    MegaFon/Yota    47825  785292  250   4.4
```

Это иллюстрация структуры, а не требование к точному spacing.

Итоговый summary может содержать:

```text
Cells found:  N
eNodeBs:       M
PLMNs:         ...
```

Нельзя автоматически объявлять:

```text
BEST CELL
```

на основании одного показателя.

---

# 28. Machine-readable scan result

Целевой top-level JSON:

```json
{
  "schema_version": 1,
  "tool": {
    "name": "lte-scan",
    "version": "..."
  },
  "radio": {
    "backend": "soapy",
    "driver": "hackrf"
  },
  "scan": {
    "bands": [3, 8, 20]
  },
  "cells": []
}
```

Cell object должен в зависимости от доступности содержать:

```text
band
earfcn
frequency_hz
pci

have_mib
have_sib1
have_sib5

nof_prb
bandwidth_mhz

snr_db

rf_metric
rf_metric_calibrated

plmns

tac
eci
enb_id
cell_id

neighbors
```

Optional data должны быть представлены однозначно: отсутствием field, `null` или другим единым документированным способом.

---

# 29. RF/PHY metrics

Необходимо отдельно провести audit доступных в srsRAN PHY metrics.

Следует исследовать как минимум:

- SNR;
- SINR;
- RSRP;
- RSRQ;
- RSSI;
- noise estimate;
- другие доступные channel-estimation metrics.

Для каждого используемого значения необходимо установить:

1. источник;
2. физический смысл;
3. единицы;
4. absolute или relative;
5. calibrated или uncalibrated;
6. можно ли сравнивать между разными bands;
7. можно ли сравнивать между разными gain settings;
8. является ли значение LTE-standard metric или внутренней оценкой.

---

# 30. rf_metric

Текущий:

```text
rf_metric
```

не является подтверждённым calibrated RSRP.

Поэтому обязательно:

```json
"rf_metric_calibrated": false
```

Нельзя переименовывать его в:

```text
RSRP
RSSI
dBm
```

без технического доказательства корректной семантики.

Нельзя вводить фиктивную calibration.

---

# 31. SNR

SNR может использоваться как относительный PHY quality indicator при условии, что его источник и метод вычисления задокументированы.

Следует избегать предположения:

```text
maximum SNR == universally best cell
```

Реальная полезность LTE cell зависит также от:

- bandwidth;
- interference;
- load;
- uplink conditions;
- scheduler;
- CA;
- network policy;
- UE capabilities.

`ltesurvey` измеряет downlink passive RF/PHY environment, а не полную user experience.

---

# 32. SIB5 graph traversal

`--seed` должен позволять начинать с известного EARFCN:

```bash
lte-scan --seed 1596
```

Алгоритм:

```text
probe seed
 -> decode SIB5
 -> enqueue unseen EARFCNs
 -> probe
 -> decode their SIB5
 -> repeat
```

Обязательно использовать:

```text
visited
scheduled
```

для защиты от циклов.

Например:

```text
1596 -> 3750 -> 1596
```

не должен создавать бесконечный scan.

---

# 33. Merge semantics

Результаты повторных probes нельзя слепо объединять только по EARFCN.

На одном EARFCN могут существовать разные PCI.

Минимальный ключ:

```text
(EARFCN, PCI)
```

Если retry обнаружил другой PCI, cell-specific data нельзя merge в предыдущую cell.

После получения ECI identity model может быть дополнительно усилен.

---

# 34. Retry policy

Retry должен быть bounded.

Предпочтительная семантика:

```text
no_cell:
    обычно не retry

partial:
    retry для попытки получить SIB1

ok without SIB5:
    optional retry для SIB5
```

Нельзя создавать бесконечные retry loops.

Параметры должны иметь разумные defaults и при необходимости быть доступны через CLI.

---

# 35. Subprocess architecture

На текущем этапе допустимо:

```text
lte-scan
    |
    +--> cell_search
    |
    +--> pdsch_ue
```

Требования:

- subprocess timeout;
- корректный exit-code handling;
- cleanup temporary files;
- cleanup после SIGINT/Ctrl-C;
- child termination;
- освобождение HackRF;
- отсутствие orphan processes.

Не следует полагаться на parsing human stdout, если backend может предоставить structured JSON.

---

# 36. Internal executable discovery

Во время development допустим поиск sibling executable рядом с `lte-scan`.

После installation программа не должна зависеть от текущего working directory.

Целевая структура:

```text
<prefix>/bin/lte-scan

<prefix>/libexec/ltesurvey/lte-cell-search
<prefix>/libexec/ltesurvey/lte-probe

<prefix>/share/ltesurvey/mcc-mnc.json
```

Имена внутренних executables могут быть уточнены позже.

---

# 37. Installation

Первый целевой механизм установки:

```bash
cmake --install
```

После установки пользователь должен иметь возможность вызвать:

```bash
lte-scan
```

из PATH без знания внутренней структуры srsRAN build tree.

---

# 38. Distribution

После стабилизации installation target подготовить возможность распространения, например:

```text
ltesurvey-X.Y.Z-macos-arm64.tar.gz
```

В дальнейшем допустим:

```text
Homebrew formula / custom tap
```

Но packaging не должен блокировать реализацию core scanner.

---

# 39. Self-test

Необходимо реализовать:

```bash
lte-scan --check
```

Он должен проверить как минимум:

```text
lte-scan installation
internal backends
SoapySDR availability
HackRF driver availability
HackRF device availability
PLMN database
```

По возможности вывод:

```text
lte-scan          OK
SoapySDR          OK
SoapyHackRF       OK
HackRF One        OK
PLMN database     OK
```

Self-test не обязан выполнять длительный LTE scan.

---

# 40. Version information

```bash
lte-scan --version
```

должен показывать версию `ltesurvey`.

JSON должен также включать:

```text
schema_version
tool version
```

Чтобы будущие программы могли корректно обрабатывать разные версии output.

---

# 41. Logging

Нужно разделить:

## normal

Пользовательский результат без лишней PHY/RF трассировки.

## verbose

Дополнительная информация:

- какой band сканируется;
- EARFCN range;
- candidate;
- MIB verification;
- probe;
- retry;
- SIB5 expansion.

## debug

Подробная техническая диагностика backend.

Существующие unconditional сообщения вроде:

```text
RXTRACE
CELLSEARCH
```

не должны оставаться в normal output законченного инструмента.

---

# 42. Error handling

Необходимо различать как минимум:

```text
HackRF not found
SoapySDR unavailable
SoapyHackRF unavailable
unsupported band
invalid EARFCN
discovery backend failed
probe backend failed
timeout
malformed backend JSON
PLMN database unavailable
```

Отсутствие PLMN database должно по возможности быть nonfatal:

```text
operator lookup unavailable
```

но raw PLMN scan должен продолжиться.

---

# 43. Timing

Желательно измерять:

```text
discovery time
probe time
total scan time
```

Это позволит оптимизировать scanner на последующих этапах.

---

# 44. Unit tests

Software-only tests должны покрывать как минимум EARFCN mapping.

## B3

```text
EARFCN 1596 -> 1844.6 MHz
```

## B8

```text
EARFCN 3750 -> 955.0 MHz
```

## B20

```text
EARFCN 6350 -> 811.0 MHz
```

---

# 45. PRB tests

Проверить:

```text
6   -> 1.4
15  -> 3
25  -> 5
50  -> 10
75  -> 15
100 -> 20
```

---

# 46. PLMN tests

Особенно проверить leading zero:

```text
MCC        250
MNC        2
MNC digits 2

-> 250-02
```

Нельзя получить:

```text
250-2
```

---

# 47. ECI tests

```text
201034970
 -> eNB 785292
 -> Cell 218
```

```text
201035002
 -> eNB 785292
 -> Cell 250
```

---

# 48. JSON tests

Проверить:

- valid JSON;
- required fields;
- optional fields;
- `ok`;
- `partial`;
- `no_cell`;
- multiple PLMN;
- SIB5 neighbor list;
- uncalibrated RF metric flag.

---

# 49. Hardware tests

Hardware tests выполняются отдельно от software-only CI.

## Known EARFCN probe

Пример:

```bash
pdsch_ue \
  -I soapy \
  -a "driver=hackrf" \
  -f 1844600000 \
  -g 40 \
  -X 3 \
  -J /tmp/lte-test.json \
  -n 5000 \
  -Q
```

Проверить:

```text
MIB
SIB1
valid JSON
```

SIB5 желательно, но он не является обязательным для каждого запуска.

---

# 50. Retune regression test

Пример:

```bash
cell_search \
  -a "driver=hackrf" \
  -b 3 \
  -s 1594 \
  -e 1599 \
  -n 3 \
  -g 40
```

Проверяется отсутствие stale duplicated results между соседними EARFCN.

Нельзя считать точные PSR/peak/CFO/SNR неизменяемыми.

---

# 51. Repeated scan test

Несколько последовательных scans должны выполняться без:

- зависания RX;
- невозможности retune;
- stale samples;
- необходимости перезапускать процесс после каждого carrier;
- resource leak.

---

# 52. Ctrl-C test

Во время scan:

```text
Ctrl-C
```

должен:

- завершить scanner;
- завершить child processes;
- закрыть RF;
- удалить temporary files;
- не оставить HackRF занятым orphan process.

---

# 53. Missing HackRF test

При отсутствии HackRF пользователь должен получить понятную ошибку.

Не должно быть:

- crash;
- бесконечного ожидания;
- misleading `no_cell`.

---

# 54. Known live observations

В ходе разработки наблюдались, среди прочего:

## B3

```text
EARFCN     1596
Frequency  1844.6 MHz
PCI        346
BW         10 MHz
```

## B8

Ранее наблюдалось:

```text
EARFCN     3750
Frequency  955.0 MHz
PCI        104
BW         10 MHz
```

У B3/B8 были получены cells с одинаковым:

```text
eNB ID = 785292
```

и разными Cell ID.

Эти данные полезны как hardware reference, но не должны быть hardcoded в scanner.

---

# 55. Целевая архитектура

Ближайшая архитектура:

```text
                     +-------------------+
                     |     lte-scan      |
                     +---------+---------+
                               |
                  +------------+-------------+
                  |                          |
                  v                          v
        +------------------+       +------------------+
        | cell discovery   |       |  PLMN database   |
        +--------+---------+       +------------------+
                 |
                 v
        confirmed candidates
                 |
                 v
        +------------------+
        |    LTE probe     |
        +--------+---------+
                 |
                 v
          MIB/SIB results
                 |
                 v
        +------------------+
        | result model     |
        +--------+---------+
                 |
          +------+------+
          |             |
          v             v
       table           JSON
```

На первом этапе `cell discovery` и `LTE probe` могут оставаться subprocess executables.

---

# 56. Более дальняя архитектура

После стабилизации поведения допустим переход к:

```text
RF backend
    |
LTE PHY
    |
cell discovery
    |
probe engine
    |
result model
    |
+---+----------------+
|                    |
CLI               future UI/API
```

Но ранний architectural cleanup не должен ставить под угрозу уже проверенную HackRF functionality.

---

# 57. Возможный Android backend

Android не входит в текущий implementation scope, но архитектура не должна без необходимости блокировать его в будущем.

Перспективно:

```text
common LTE result model
       |
       +--> SDR/HackRF backend
       |
       +--> Android modem telemetry backend
```

Это разные источники данных.

SDR backend обеспечивает независимое пассивное исследование эфира.

Android/Qualcomm-class modem может в будущем предоставлять более корректно откалиброванные UE measurements.

Не следует смешивать эти задачи на текущем этапе.

---

# 58. Development principles

Основной принцип:

```text
working hardware behavior > architectural elegance
```

Сначала сохранить и покрыть интерфейсами hardware-verified behavior.

Затем улучшать архитектуру небольшими контролируемыми этапами.

---

# 59. Phase 0 — repository audit

Перед первой новой реализацией необходимо:

1. прочитать `AGENTS.md`;
2. прочитать этот документ;
3. проверить текущую branch;
4. выполнить `git status`;
5. изучить relevant source files;
6. при необходимости сравнить изменения с:

```text
ltesurvey-known-good-20260920
```

7. определить, какие части следующей фазы уже реализованы;
8. не переписывать работающий код без необходимости.

Phase 0 не должен превращаться в большой refactoring.

---

# 60. Phase 1 — structured LTE cell discovery

## Цель

Получить clean structured discovery backend.

## Основная задача

Добавить/довести machine-readable output `cell_search`, например:

```bash
cell_search ... -J discovery.json
```

## Результат

Backend должен выдавать список MIB-confirmed LTE candidates.

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
      "bandwidth_mhz": 10.0
    }
  ]
}
```

Допустимо включить metadata/schema version.

## Требования

- RF должен открываться один раз на scan;
- последовательный retune должен сохранять hardware-verified semantics;
- PSS candidates должны подтверждаться MIB;
- false PSS candidates не должны попадать как confirmed cells;
- human diagnostics не должны быть единственным API;
- JSON должен быть deterministic и valid;
- normal legacy behavior по возможности не ломать.

## Не делать в Phase 1

Не требуется:

- PLMN database;
- packaging;
- full `lte-scan --band`;
- большой probe refactor;
- full macOS srsRAN port;
- RF backend cleanup.

## Offline IQ regression fixture

Phase 1 должен включать software-only regression test на реальной записи LTE downlink. Цель fixture — воспроизводимо проверить тракт discovery без наличия HackRF в CI:

```text
recorded LTE IQ
 -> file RF backend
 -> PSS/SSS
 -> candidate
 -> PBCH/MIB
 -> structured discovery JSON
```

Проверенная fixture подготовлена из реального HackRF capture контрольной B3 cell. Исходная запись была получена на:

```text
center frequency: 1844600000 Hz
sample rate:      15360000 sample/s
HackRF AMP:       off
LNA gain:         32
VGA gain:         32
```

Исходный high-rate capture не требуется хранить в repository. Для regression используется предварительно отфильтрованный и decimated сигнал.

Формат repository fixture:

```text
sample format:       signed int8
IQ layout:           interleaved I,Q
sample rate:         1920000 sample/s
duration:            4.5 s
size:                17280000 bytes
center frequency:    1844600000 Hz
LTE band:            3
EARFCN:              1596
normalization:       peak component approximately 100
```

Нормализация является только способом эффективного использования динамического диапазона fixture. Она не сохраняет абсолютную RF amplitude и не должна использоваться для получения dBm, RSRP, RSSI или других calibrated measurements.

Перед подачей в текущий `rf_file` backend fixture преобразуется:

```text
CS8 @ 1.92 Msps
 -> deterministic CS8-to-FC32 conversion
 -> FC32 @ 1.92 Msps
 -> rf_file with base_srate=1920000
```

Для этого преобразования не требуется resampling, filtering, SciPy или иной DSP. Оно должно быть простым детерминированным sample-format conversion. Временный FC32 файл не хранится в repository.

Ожидаемый единственный MIB-confirmed результат fixture:

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

Regression test должен проверять точный набор MIB-confirmed cells. Тест должен завершаться ошибкой как при отсутствии ожидаемой cell, так и при появлении дополнительной MIB-confirmed cell.

PSR, peak, CFO и некалиброванные power-like значения не являются стабильными acceptance constants для этой fixture.

Текущий `cell_search` использует half-open EARFCN range. Для проверки одного EARFCN 1596 используется:

```text
-s 1596 -e 1597
```

Один статический `rx_file` не моделирует физический retune между разными EARFCN. Поэтому эта fixture предназначена для single-frequency discovery regression и не заменяет hardware retune test.

При file backend PSS search и последующий MIB decoder последовательно потребляют один и тот же конечный IQ stream. Экспериментально 1, 2 и 3 секунды этой записи заканчивались EOF до успешного MIB decode, а 4 и 5 секунд проходили. Для fixture выбрана длительность 4.5 секунды как проверенный запас относительно границы.

Рекомендуемая структура:

```text
tests/fixtures/lte/b3-earfcn1596/
    README.md
    metadata.json
    capture.cs8

tests/tools/
    cs8_to_fc32.py
```

`metadata.json` должен описывать формат, sample rate, duration, center frequency, band, EARFCN, normalization, ожидаемые cells и SHA-256 IQ fixture.

Regression test должен использовать structured discovery JSON как authoritative result, а не разбирать human-readable stdout.

## Acceptance

Software build:

```text
pdsch_ue
cell_search
lte_scan
```

должен оставаться успешным.

Software-only offline regression должна подтвердить:

```text
CS8 fixture
 -> FC32
 -> file RF backend
 -> PSS/SSS
 -> PCI 346
 -> MIB
 -> 50 PRB / 10 MHz
 -> 2 antenna ports
 -> structured candidate
```

и отсутствие дополнительных MIB-confirmed cells.

Hardware verification должна подтвердить:

```text
band/range scan
 -> real cell
 -> MIB
 -> structured candidate
```

и отсутствие stale results при retune.

Offline IQ fixture не заменяет hardware verification retune/sample-rate/SoapySDR semantics.

---

# 61. Phase 2 — lte-scan --band

Добавить:

```bash
lte-scan --band 3
```

Алгоритм:

```text
lte-scan
 -> invoke discovery once for B3
 -> parse structured candidates
 -> invoke full probe only for candidates
 -> aggregate cells
 -> output
```

Нельзя выполнять full `pdsch_ue` для каждой 100 kHz raster point.

---

# 62. Phase 3 — multiple bands

Добавить:

```bash
lte-scan --band 3,7,8,20
```

Требования:

- deduplication;
- predictable ordering;
- per-band error handling;
- scan должен продолжаться при отсутствии cells в одном band;
- aggregate output.

---

# 63. Phase 4 — SIB5 graph discovery

Объединить band discovery и SIB5 expansion.

Например:

```text
B3 discovery
 -> EARFCN 1596
 -> SIB5
 -> 3750
 -> probe
 -> additional SIB5
```

Использовать bounded graph traversal.

---

# 64. Phase 5 — PLMN database

Добавить offline operator lookup.

Требования:

- redistributable source;
- documented license;
- source/version metadata;
- local database;
- no mandatory Internet;
- 2/3 digit MNC;
- multiple PLMN;
- unknown fallback.

---

# 65. Phase 6 — measurement audit

Провести технический audit PHY measurements.

Только после этого определить окончательный набор колонок:

```text
SNR
RSRP
RSRQ
SINR
RSSI
...
```

если они действительно доступны с корректной семантикой.

Не следует подгонять существующие значения под желаемые LTE metric names.

---

# 66. Phase 7 — CLI polish

Довести:

```text
normal output
verbose
debug
errors
summary
--check
--version
JSON schema
```

Удалить/скрыть legacy debug noise из normal operation.

---

# 67. Phase 8 — installation and packaging

Реализовать:

```text
cmake --install
```

и корректный installed layout.

После этого подготовить release packaging.

---

# 68. Phase 9 — regression and cleanup

После стабилизации функциональности:

- удалить obsolete experimental code;
- уменьшить duplication;
- улучшить naming;
- оформить architecture documentation;
- добавить regression tests;
- проверить signal handling;
- проверить temporary files;
- проверить subprocess lifecycle;
- проверить repeated hardware scans.

Большие refactorings делать только после фиксации соответствующих behavior tests.

---

# 69. Правила выполнения фаз

Не следует реализовывать все фазы за одну coding session.

Каждая фаза должна быть достаточно небольшой для:

```text
inspect
 -> implement
 -> build
 -> review
 -> software test
 -> hardware test where needed
 -> commit
```

Если задача неожиданно становится существенно шире, следует остановиться на логической границе и описать оставшуюся работу.

---

# 70. Git discipline

Основная development branch:

```text
ltesurvey
```

Допустимы feature branches, например:

```text
feature/structured-discovery
feature/band-scan
feature/plmn-database
```

Контрольный baseline:

```text
ltesurvey-known-good-20260920
```

Полезная проверка cumulative changes:

```bash
git diff ltesurvey-known-good-20260920..HEAD
```

Не переписывать опубликованную историю без явной необходимости.

Коммиты должны быть логически ограниченными.

---

# 71. Hardware verification и coding agents

Coding agent может выполнять software build/tests в доступной ему среде.

Нельзя считать hardware test выполненным, если агент фактически не имел доступа к HackRF.

После изменений, затрагивающих:

```text
RF
Soapy
stream lifecycle
sample rate
frequency retune
cell search
MIB acquisition
```

необходимо предоставить точные команды для hardware verification.

Финальное подтверждение таких изменений выполняется на реальном HackRF.

---

# 72. Documentation discipline

При изменении:

- архитектуры;
- CLI;
- JSON schema;
- installation layout;
- RF semantics;
- measurement semantics;

должна обновляться документация:

```text
docs/ltesurvey/
```

Реализация и specification не должны незаметно расходиться.

---

# 73. Definition of Done для первой законченной версии

Первая законченная версия `ltesurvey` считается достигнутой, когда пользователь может установить проект и выполнить:

```bash
lte-scan --band 3,7,8,20
```

и получить без ручного запуска внутренних utilities:

1. список реально обнаруженных LTE cells;
2. band;
3. EARFCN;
4. frequency;
5. PCI;
6. bandwidth;
7. PLMN;
8. operator name;
9. TAC;
10. ECI;
11. eNodeB ID;
12. Cell ID;
13. корректно определённые доступные RF/PHY metrics;
14. SIB5-derived neighbor carriers;
15. human-readable table;
16. structured JSON.

Дополнительно должны работать:

```bash
lte-scan --check
lte-scan --version
```

и установленный scanner не должен зависеть от build directory или текущего working directory.

---

# 74. Основной критерий качества

`ltesurvey` должен показывать пользователю то, что реально известно из эфира, и явно отделять это от:

- предположений;
- lookup metadata;
- некалиброванных измерений;
- временно недоступных данных.

Особенно важно:

```text
неизвестно != ошибка
не обнаружено != не существует
PSS candidate != confirmed LTE cell
SIB5 neighbor != currently received cell
rf_metric != calibrated RSRP
PCI != globally unique cell identity
strongest signal != automatically best network
```

Точность семантики важнее количества выводимых параметров.
