#ifndef PDSCH_UE_SIB_H
#define PDSCH_UE_SIB_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Decode an LTE BCCH-DL-SCH transport block carried using SI-RNTI.
 *
 * Returns:
 *   1  successfully decoded supported BCCH-DL-SCH message
 *   0  valid BCCH-DL-SCH message, but not handled
 *  -1  ASN.1 decoding failed
 */
int pdsch_ue_decode_bcch(const uint8_t* data, uint32_t nbytes);

/*
 * Return non-zero when SFN/subframe belongs to an SI window advertised
 * by the most recently decoded SIB1.
 *
 * SIB1 itself is not included here; its fixed scheduling is handled
 * separately by pdsch_ue.c.
 */
int pdsch_ue_si_window(uint32_t sfn, uint32_t sf_idx);

#ifdef __cplusplus
}
#endif

#endif
