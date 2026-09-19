#include "lte_probe_result.h"
#include <string>
#include "lte_earfcn.h"

#include <cstring>
#include <cstdio>

static lte_probe_result_t result;

extern "C" void lte_probe_result_reset(void)
{
  std::memset(&result, 0, sizeof(result));
}

extern "C" const lte_probe_result_t* lte_probe_result_get(void)
{
  return &result;
}

extern "C" void lte_probe_set_mib(void)
{
  result.have_mib = 1;
}

extern "C" void lte_probe_set_cell_identity(uint16_t tac,
                                              uint32_t eci)
{
  result.tac     = tac;
  result.eci     = eci;
  result.enb_id  = eci >> 8;
  result.cell_id = eci & 0xff;
}

extern "C" void lte_probe_add_plmn(uint16_t mcc,
                                    uint16_t mnc,
                                    uint8_t mnc_digits)
{
  if (result.nof_plmns >= LTE_PROBE_MAX_PLMNS) {
    return;
  }

  lte_probe_plmn_t* p = &result.plmns[result.nof_plmns++];

  p->mcc        = mcc;
  p->mnc        = mnc;
  p->mnc_digits = mnc_digits;
}

extern "C" void lte_probe_add_neighbor_earfcn(uint32_t earfcn)
{
  for (uint8_t i = 0; i < result.nof_neighbor_earfcns; ++i) {
    if (result.neighbor_earfcns[i] == earfcn) {
      return;
    }
  }

  if (result.nof_neighbor_earfcns >= LTE_PROBE_MAX_NEIGHBORS) {
    return;
  }

  result.neighbor_earfcns[result.nof_neighbor_earfcns++] = earfcn;
}

extern "C" void lte_probe_mark_sib1(void)
{
  result.have_sib1 = 1;
}

extern "C" void lte_probe_mark_sib5(void)
{
  result.have_sib5 = 1;
}

extern "C" void lte_probe_set_band(uint8_t band)
{
  result.band = band;
}


extern "C" void lte_probe_result_dump(void)
{
  std::fprintf(stderr, "\nLTE PROBE:\n");

  std::fprintf(stderr, "  SIB1:       %s\n",
               result.have_sib1 ? "yes" : "no");
  std::fprintf(stderr, "  SIB5:       %s\n",
               result.have_sib5 ? "yes" : "no");

  std::fprintf(stderr, "  Frequency:  %.4f MHz\n",
               result.frequency_hz / 1.0e6);

  uint32_t earfcn = 0;
  if (lte_frequency_to_earfcn(result.frequency_hz,
                              result.band,
                              &earfcn) == 0) {
    result.earfcn = earfcn;
    std::fprintf(stderr, "  EARFCN:     %u\n",
                 (unsigned)result.earfcn);
  } else {
    std::fprintf(stderr, "  EARFCN:     unknown\n");
  }

  std::fprintf(stderr, "  PCI:        %u\n",
               (unsigned)result.pci);

  const double bandwidth_mhz =
      lte_prb_to_bandwidth_mhz(result.nof_prb);

  if (bandwidth_mhz > 0.0) {
    std::fprintf(stderr, "  Bandwidth:  %.1f MHz (%u PRB)\n",
                 bandwidth_mhz,
                 (unsigned)result.nof_prb);
  } else {
    std::fprintf(stderr, "  Bandwidth:  unknown (%u PRB)\n",
                 (unsigned)result.nof_prb);
  }
  std::fprintf(stderr, "  SNR:        %.1f dB\n",
               result.snr_db);

  if (result.signal_level_calibrated) {
    std::fprintf(stderr, "  Level:      %.1f dBm\n",
                 result.signal_level);
  } else {
    std::fprintf(stderr, "  RF metric:  %.1f (uncalibrated)\n",
                 result.signal_level);
  }

  if (result.have_sib1) {
    std::fprintf(stderr, "  Band:       %u\n",
                 (unsigned)result.band);

    std::fprintf(stderr, "  PLMN:       ");
    if (result.nof_plmns == 0) {
      std::fprintf(stderr, "none");
    } else {
      for (uint8_t i = 0; i < result.nof_plmns; ++i) {
        if (i != 0) {
          std::fprintf(stderr, ", ");
        }

        const lte_probe_plmn_t* p = &result.plmns[i];

        std::fprintf(stderr, "%03u-",
                     (unsigned)p->mcc);

        if (p->mnc_digits == 2) {
          std::fprintf(stderr, "%02u", (unsigned)p->mnc);
        } else {
          std::fprintf(stderr, "%03u", (unsigned)p->mnc);
        }
      }
    }
    std::fprintf(stderr, "\n");

    std::fprintf(stderr, "  TAC:        %u\n",
                 (unsigned)result.tac);
    std::fprintf(stderr, "  ECI:        %u\n",
                 (unsigned)result.eci);
    std::fprintf(stderr, "  eNB ID:     %u\n",
                 (unsigned)result.enb_id);
    std::fprintf(stderr, "  Cell ID:    %u\n",
                 (unsigned)result.cell_id);
  }

  std::fprintf(stderr, "  Neighbors:  ");

  if (result.nof_neighbor_earfcns == 0) {
    std::fprintf(stderr, "none");
  } else {
    for (uint8_t i = 0; i < result.nof_neighbor_earfcns; ++i) {
      if (i != 0) {
        std::fprintf(stderr, ", ");
      }

      std::fprintf(stderr, "%u",
                   (unsigned)result.neighbor_earfcns[i]);
    }
  }

  std::fprintf(stderr, "\n\n");
}

extern "C" void lte_probe_set_phy(double frequency_hz,
                                   uint16_t pci,
                                   uint16_t nof_prb,
                                   float snr_db,
                                   float signal_level,
                                   uint8_t signal_level_calibrated)
{
  result.frequency_hz            = frequency_hz;
  result.pci                     = pci;
  result.nof_prb                 = nof_prb;
  result.snr_db                  = snr_db;
  result.signal_level            = signal_level;
  result.signal_level_calibrated = signal_level_calibrated;
}

extern "C" int lte_probe_result_write_json(const char* filename,
                                             lte_probe_status_t status)
{
  if (filename == NULL || filename[0] == '\0') {
    return -1;
  }

  std::string tmp_filename = std::string(filename) + ".tmp";

  FILE* f = std::fopen(tmp_filename.c_str(), "w");
  if (f == NULL) {
    return -1;
  }

  if (status == LTE_PROBE_NO_CELL) {
    std::fprintf(f,
                 "{\"status\":\"no_cell\",\"frequency_hz\":%.0f}\n",
                 result.frequency_hz);
  } else if (status == LTE_PROBE_ERROR) {
    std::fprintf(f,
                 "{\"status\":\"error\",\"frequency_hz\":%.0f}\n",
                 result.frequency_hz);
  } else {
    uint32_t earfcn = 0;
    const int have_earfcn =
        lte_frequency_to_earfcn(result.frequency_hz,
                                result.band,
                                &earfcn) == 0;

    if (have_earfcn) {
      result.earfcn = earfcn;
    }

    const char* probe_status =
        result.have_sib1 ? "ok" : "partial";

    std::fprintf(f, "{");
    std::fprintf(f, "\"status\":\"%s\"", probe_status);
    std::fprintf(f, ",\"frequency_hz\":%.0f", result.frequency_hz);
    std::fprintf(f, ",\"have_mib\":%s",
                 result.have_mib ? "true" : "false");
    std::fprintf(f, ",\"have_sib1\":%s",
                 result.have_sib1 ? "true" : "false");
    std::fprintf(f, ",\"have_sib5\":%s",
                 result.have_sib5 ? "true" : "false");

    if (have_earfcn) {
      std::fprintf(f, ",\"earfcn\":%u", (unsigned)result.earfcn);
    } else {
      std::fprintf(f, ",\"earfcn\":null");
    }

    std::fprintf(f, ",\"band\":%u", (unsigned)result.band);
    std::fprintf(f, ",\"pci\":%u", (unsigned)result.pci);
    std::fprintf(f, ",\"nof_prb\":%u", (unsigned)result.nof_prb);
    std::fprintf(f,
                 ",\"bandwidth_mhz\":%.1f",
                 lte_prb_to_bandwidth_mhz(result.nof_prb));

    std::fprintf(f, ",\"snr_db\":%.1f", result.snr_db);
    std::fprintf(f, ",\"rf_metric\":%.1f", result.signal_level);
    std::fprintf(f,
                 ",\"rf_metric_calibrated\":%s",
                 result.signal_level_calibrated ? "true" : "false");

    std::fprintf(f, ",\"tac\":%u", (unsigned)result.tac);
    std::fprintf(f, ",\"eci\":%u", (unsigned)result.eci);
    std::fprintf(f, ",\"enb_id\":%u", (unsigned)result.enb_id);
    std::fprintf(f, ",\"cell_id\":%u", (unsigned)result.cell_id);

    std::fprintf(f, ",\"plmns\":[");
    for (uint8_t i = 0; i < result.nof_plmns; ++i) {
      if (i != 0) {
        std::fprintf(f, ",");
      }

      const lte_probe_plmn_t* plmn = &result.plmns[i];

      std::fprintf(f,
                   "{\"mcc\":%u,\"mnc\":%u,\"mnc_digits\":%u}",
                   (unsigned)plmn->mcc,
                   (unsigned)plmn->mnc,
                   (unsigned)plmn->mnc_digits);
    }
    std::fprintf(f, "]");

    std::fprintf(f, ",\"neighbors\":[");
    for (uint8_t i = 0; i < result.nof_neighbor_earfcns; ++i) {
      if (i != 0) {
        std::fprintf(f, ",");
      }

      std::fprintf(f,
                   "%u",
                   (unsigned)result.neighbor_earfcns[i]);
    }
    std::fprintf(f, "]");

    std::fprintf(f, "}\n");
  }

  if (std::fclose(f) != 0) {
    std::remove(tmp_filename.c_str());
    return -1;
  }

  if (std::rename(tmp_filename.c_str(), filename) != 0) {
    std::remove(tmp_filename.c_str());
    return -1;
  }

  return 0;
}
