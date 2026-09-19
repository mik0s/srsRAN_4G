#ifndef LTE_PROBE_RESULT_H
#define LTE_PROBE_RESULT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LTE_PROBE_MAX_PLMNS      6
#define LTE_PROBE_MAX_NEIGHBORS 32

typedef enum {
  LTE_PROBE_OK       = 0,
  LTE_PROBE_NO_CELL  = 1,
  LTE_PROBE_ERROR    = -1
} lte_probe_status_t;

typedef struct {
  uint16_t mcc;
  uint16_t mnc;
  uint8_t  mnc_digits;
} lte_probe_plmn_t;

typedef struct {
  /* RF / PHY */
  double   frequency_hz;
  uint32_t earfcn;
  uint16_t pci;
  uint16_t nof_prb;
  float    snr_db;

  /*
   * HackRF/Soapy does not currently provide a calibrated absolute
   * LTE RSRP measurement. Keep this explicitly separate from SNR.
   */
  float    signal_level;
  uint8_t  signal_level_calibrated;

  /* Cell identity */
  uint8_t  band;
  uint16_t tac;
  uint32_t eci;
  uint32_t enb_id;
  uint8_t  cell_id;

  /* PLMNs */
  uint8_t          nof_plmns;
  lte_probe_plmn_t plmns[LTE_PROBE_MAX_PLMNS];

  /* SIB5 inter-frequency E-UTRA carriers */
  uint8_t  nof_neighbor_earfcns;
  uint32_t neighbor_earfcns[LTE_PROBE_MAX_NEIGHBORS];

  /* Decode state */
  uint8_t have_mib;
  uint8_t have_sib1;
  uint8_t have_sib5;
} lte_probe_result_t;

void lte_probe_result_reset(void);
void lte_probe_set_mib(void);
const lte_probe_result_t* lte_probe_result_get(void);
void lte_probe_result_dump(void);
int lte_probe_result_write_json(const char* filename,
                                lte_probe_status_t status);

void lte_probe_set_cell_identity(uint16_t tac,
                                 uint32_t eci);

void lte_probe_set_band(uint8_t band);

void lte_probe_set_phy(double frequency_hz,
                       uint16_t pci,
                       uint16_t nof_prb,
                       float snr_db,
                       float signal_level,
                       uint8_t signal_level_calibrated);

void lte_probe_add_plmn(uint16_t mcc,
                        uint16_t mnc,
                        uint8_t mnc_digits);

void lte_probe_add_neighbor_earfcn(uint32_t earfcn);

void lte_probe_mark_sib1(void);
void lte_probe_mark_sib5(void);

#ifdef __cplusplus
}
#endif

#endif
