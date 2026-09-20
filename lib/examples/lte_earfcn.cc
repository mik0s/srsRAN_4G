#include "lte_earfcn.h"

#include <cmath>
#include <cstddef>

struct lte_band_info {
  uint8_t  band;
  double   dl_low_mhz;
  uint32_t n_offs_dl;
  uint32_t earfcn_first;
  uint32_t earfcn_last;
};

/*
 * 3GPP TS 36.101, E-UTRA channel numbers.
 *
 * Keep this table deliberately small for the first scanner version.
 * Adding another LTE band requires only another entry here.
 */
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

static const lte_band_info* find_band(uint8_t band)
{
  for (size_t i = 0; i < sizeof(bands) / sizeof(bands[0]); ++i) {
    if (bands[i].band == band) {
      return &bands[i];
    }
  }

  return nullptr;
}

extern "C" int lte_band_earfcn_range(uint8_t band,
                                      uint32_t* first_earfcn,
                                      uint32_t* last_earfcn)
{
  if (!first_earfcn || !last_earfcn) {
    return -1;
  }

  const lte_band_info* b = find_band(band);
  if (!b) {
    return -1;
  }

  *first_earfcn = b->earfcn_first;
  *last_earfcn = b->earfcn_last;

  return 0;
}

extern "C" int lte_frequency_to_earfcn(double frequency_hz,
                                        uint8_t band,
                                        uint32_t* earfcn)
{
  if (!earfcn) {
    return -1;
  }

  const lte_band_info* b = find_band(band);
  if (!b) {
    return -1;
  }

  const double freq_mhz = frequency_hz / 1.0e6;

  /*
   * LTE raster is 100 kHz. RF tuning error/offset may leave the
   * measured/tuned frequency slightly away from the nominal raster,
   * therefore select the nearest EARFCN.
   */
  const long n = std::lround(
      (freq_mhz - b->dl_low_mhz) / 0.1 +
      (double)b->n_offs_dl);

  if (n < (long)b->earfcn_first ||
      n > (long)b->earfcn_last) {
    return -1;
  }

  *earfcn = (uint32_t)n;
  return 0;
}

extern "C" int lte_earfcn_to_frequency(uint32_t earfcn,
                                        uint8_t* band,
                                        double* frequency_hz)
{
  if (!frequency_hz) {
    return -1;
  }

  for (size_t i = 0; i < sizeof(bands) / sizeof(bands[0]); ++i) {
    const lte_band_info& b = bands[i];

    if (earfcn < b.earfcn_first ||
        earfcn > b.earfcn_last) {
      continue;
    }

    const double freq_mhz =
        b.dl_low_mhz +
        0.1 * ((double)earfcn - (double)b.n_offs_dl);

    *frequency_hz = freq_mhz * 1.0e6;

    if (band) {
      *band = b.band;
    }

    return 0;
  }

  return -1;
}

extern "C" double lte_prb_to_bandwidth_mhz(uint16_t nof_prb)
{
  switch (nof_prb) {
    case 6:   return 1.4;
    case 15:  return 3.0;
    case 25:  return 5.0;
    case 50:  return 10.0;
    case 75:  return 15.0;
    case 100: return 20.0;
    default:  return 0.0;
  }
}
