#ifndef LTE_EARFCN_H
#define LTE_EARFCN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int lte_frequency_to_earfcn(double frequency_hz,
                            uint8_t band,
                            uint32_t* earfcn);

int lte_earfcn_to_frequency(uint32_t earfcn,
                            uint8_t* band,
                            double* frequency_hz);

double lte_prb_to_bandwidth_mhz(uint16_t nof_prb);

#ifdef __cplusplus
}
#endif

#endif
