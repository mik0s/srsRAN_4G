#include "lte_probe_result.h"
#include "pdsch_ue_sib.h"

#include "srsran/asn1/asn1_utils.h"
#include "srsran/asn1/rrc.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace asn1;
using namespace asn1::rrc;

struct si_schedule_entry {
  uint16_t period_rf;
  uint32_t start_tti;
};

static si_schedule_entry si_schedule[32];
static uint32_t          nof_si_entries = 0;
static uint32_t          si_window_ms   = 0;
static bool              have_si_schedule = false;

/*
 * Presentation state for decoded SystemInformation.
 *
 * systemInfoValueTag from SIB1 identifies the current SI generation.
 * Once a SIB has been printed for that generation, repeated broadcasts
 * are suppressed. A new value tag makes all SIBs printable again.
 */
static bool     have_si_value_tag = false;
static uint8_t  si_value_tag      = 0;
static uint32_t seen_sibs         = 0;

static uint32_t sib_seen_bit(int type)
{
  switch (type) {
    case sib_info_item_c::types_opts::sib2:
      return 1u << 2;
    case sib_info_item_c::types_opts::sib3:
      return 1u << 3;
    case sib_info_item_c::types_opts::sib5:
      return 1u << 5;
    case sib_info_item_c::types_opts::sib6:
      return 1u << 6;
    case sib_info_item_c::types_opts::sib7:
      return 1u << 7;
    default:
      return 0;
  }
}


extern "C" int pdsch_ue_si_window(uint32_t sfn, uint32_t sf_idx)
{
  if (!have_si_schedule || sf_idx >= 10) {
    return 0;
  }

  /*
   * Work in absolute TTIs within the 1024-frame SFN cycle.
   * For each SI message:
   *
   *   x = (n - 1) * w
   *
   * start frame within its period = floor(x / 10)
   * start subframe                = x mod 10
   *
   * Using TTI arithmetic also handles SI windows that cross a
   * radio-frame boundary.
   */
  const uint32_t current_tti = sfn * 10 + sf_idx;

  for (uint32_t i = 0; i < nof_si_entries; ++i) {
    const uint32_t period_tti = si_schedule[i].period_rf * 10;
    const uint32_t start_tti  = si_schedule[i].start_tti;

    if (period_tti == 0) {
      continue;
    }

    /*
     * SFN wraps at 1024. The SI periodicities divide 1024, so reducing
     * current TTI modulo the SI period gives the position within the
     * current scheduling period.
     */
    const uint32_t pos = current_tti % period_tti;

    if (pos >= start_tti &&
        pos < start_tti + si_window_ms) {
      return 1;
    }
  }

  return 0;
}

static uint32_t bitstring_to_u32(const std::string& bits)
{
  uint32_t v = 0;

  for (char c : bits) {
    v <<= 1;
    if (c == '1') {
      v |= 1;
    }
  }

  return v;
}

static void print_plmn(const plmn_id_s& plmn)
{
  if (plmn.mcc_present) {
    for (size_t i = 0; i < plmn.mcc.size(); ++i) {
      std::fprintf(stderr, "%u", (unsigned)plmn.mcc[i]);
    }
  } else {
    std::fprintf(stderr, "???");
  }

  std::fprintf(stderr, "-");

  for (size_t i = 0; i < plmn.mnc.size(); ++i) {
    std::fprintf(stderr, "%u", (unsigned)plmn.mnc[i]);
  }
}


static void probe_add_plmn(const plmn_id_s& plmn)
{
  /*
   * MCC may be absent when ASN.1 uses the MCC inherited from
   * the preceding PLMN entry. For now only store self-contained
   * PLMN identities; inheritance can be handled separately if
   * encountered in real broadcasts.
   */
  if (!plmn.mcc_present || plmn.mcc.size() != 3) {
    return;
  }

  uint16_t mcc = 0;
  for (size_t i = 0; i < plmn.mcc.size(); ++i) {
    mcc = (uint16_t)(mcc * 10 + plmn.mcc[i]);
  }

  uint16_t mnc = 0;
  for (size_t i = 0; i < plmn.mnc.size(); ++i) {
    mnc = (uint16_t)(mnc * 10 + plmn.mnc[i]);
  }

  lte_probe_add_plmn(mcc, mnc, (uint8_t)plmn.mnc.size());
}


static void print_sib2(const sib_type2_s& s)
{
  const rr_cfg_common_sib_s& rr = s.rr_cfg_common;
  const rach_cfg_common_s& rach = rr.rach_cfg_common;
  const prach_cfg_sib_s& prach = rr.prach_cfg;
  const pusch_cfg_common_s& pusch = rr.pusch_cfg_common;
  const ul_pwr_ctrl_common_s& pwr = rr.ul_pwr_ctrl_common;

  std::fprintf(stderr, "LTE SIB2:\n");

  if (s.freq_info.ul_bw_present) {
    std::fprintf(stderr, "  UL bandwidth:        %u PRB\n",
                 (unsigned)s.freq_info.ul_bw.to_number());
  } else {
    std::fprintf(stderr, "  UL bandwidth:        same as DL\n");
  }

  if (s.freq_info.ul_carrier_freq_present) {
    std::fprintf(stderr, "  UL EARFCN:           %u\n",
                 (unsigned)s.freq_info.ul_carrier_freq);
  }

  std::fprintf(stderr, "  PRACH root seq:      %u\n",
               (unsigned)prach.root_seq_idx);

  std::fprintf(stderr, "  PRACH config index:  %u\n",
               (unsigned)prach.prach_cfg_info.prach_cfg_idx);

  std::fprintf(stderr, "  PRACH freq offset:   %u\n",
               (unsigned)prach.prach_cfg_info.prach_freq_offset);

  std::fprintf(stderr, "  RA preambles:        %u\n",
               (unsigned)rach.preamb_info.nof_ra_preambs.to_number());

  std::fprintf(stderr, "  P0 PUSCH:            %d dBm\n",
               (int)pwr.p0_nominal_pusch);

  std::fprintf(stderr, "  P0 PUCCH:            %d dBm\n",
               (int)pwr.p0_nominal_pucch);

  std::fprintf(stderr, "  UL 64QAM:            %s\n",
               pusch.pusch_cfg_basic.enable64_qam ? "yes" : "no");

  std::fprintf(stderr, "\n");
}

static void print_sib3(const sib_type3_s& s)
{
  std::fprintf(stderr, "LTE SIB3:\n");
  std::fprintf(stderr, "  q-Hyst:              %s\n",
              s.cell_resel_info_common.q_hyst.to_string());

  const auto& serving = s.cell_resel_serving_freq_info;

  std::fprintf(stderr, "  Serving priority:    %u\n",
              (unsigned)serving.cell_resel_prio);

  if (serving.s_non_intra_search_present) {
    std::fprintf(stderr, "  s-NonIntraSearch:    %u\n",
                (unsigned)serving.s_non_intra_search);
  }

  const auto& intra = s.intra_freq_cell_resel_info;

  std::fprintf(stderr, "  q-RxLevMin:          %d dBm\n",
              (int)intra.q_rx_lev_min);

  if (intra.s_intra_search_present) {
    std::fprintf(stderr, "  s-IntraSearch:       %u\n",
                (unsigned)intra.s_intra_search);
  }

  if (intra.allowed_meas_bw_present) {
    std::fprintf(stderr, "  Allowed meas BW:     %s\n",
                intra.allowed_meas_bw.to_string());
  }

  std::fprintf(stderr, "  t-ReselectionEUTRA:  %u s\n",
              (unsigned)intra.t_resel_eutra);

  std::fprintf(stderr, "\n");
}

static void print_sib5(const sib_type5_s& s)
{
  std::fprintf(stderr, "LTE SIB5:\n");

  if (s.inter_freq_carrier_freq_list.size() == 0) {
    std::fprintf(stderr, "  No inter-frequency E-UTRA carriers\n\n");
    return;
  }

  for (size_t i = 0; i < s.inter_freq_carrier_freq_list.size(); ++i) {
    const inter_freq_carrier_freq_info_s& c =
        s.inter_freq_carrier_freq_list[i];

    lte_probe_add_neighbor_earfcn((uint32_t)c.dl_carrier_freq);

    std::fprintf(stderr, "  E-UTRA carrier %zu:\n", i + 1);
    std::fprintf(stderr, "    EARFCN:             %u\n",
                (unsigned)c.dl_carrier_freq);
    std::fprintf(stderr, "    q-RxLevMin:         %d dBm\n",
                (int)c.q_rx_lev_min);
    std::fprintf(stderr, "    Allowed meas BW:    %s\n",
                c.allowed_meas_bw.to_string());
    std::fprintf(stderr, "    threshX-High:       %u\n",
                (unsigned)c.thresh_x_high);
    std::fprintf(stderr, "    threshX-Low:        %u\n",
                (unsigned)c.thresh_x_low);
    std::fprintf(stderr, "    t-ReselectionEUTRA: %u s\n",
                (unsigned)c.t_resel_eutra);

    if (c.cell_resel_prio_present) {
      std::fprintf(stderr, "    Priority:           %u\n",
                  (unsigned)c.cell_resel_prio);
    }

    if (c.p_max_present) {
      std::fprintf(stderr, "    p-Max:              %d dBm\n",
                  (int)c.p_max);
    }

    if (c.q_offset_freq_present) {
      std::fprintf(stderr, "    q-OffsetFreq:       %s\n",
                  c.q_offset_freq.to_string());
    }

    if (c.q_qual_min_r9_present) {
      std::fprintf(stderr, "    q-QualMin:          %d dB\n",
                  (int)c.q_qual_min_r9);
    }

    if (c.inter_freq_neigh_cell_list_present) {
      std::fprintf(stderr, "    Explicit neighbours: %u\n",
                  (unsigned)c.inter_freq_neigh_cell_list.size());
    }

    if (c.inter_freq_excluded_cell_list_present) {
      std::fprintf(stderr, "    Excluded ranges:     %u\n",
                  (unsigned)c.inter_freq_excluded_cell_list.size());
    }
  }

  std::fprintf(stderr, "\n");
}

static void print_sib6(const sib_type6_s& s)
{
  std::fprintf(stderr, "LTE SIB6:\n");

  if (s.carrier_freq_list_utra_fdd_present) {
    for (size_t i = 0; i < s.carrier_freq_list_utra_fdd.size(); ++i) {
      const carrier_freq_utra_fdd_s& c =
          s.carrier_freq_list_utra_fdd[i];

      std::fprintf(stderr, "  UTRA-FDD carrier %zu:\n", i + 1);
      std::fprintf(stderr, "    UARFCN:        %u\n",
                  (unsigned)c.carrier_freq);
      std::fprintf(stderr, "    q-RxLevMin:    %d dBm\n",
                  (int)c.q_rx_lev_min);
      std::fprintf(stderr, "    q-QualMin:     %d dB\n",
                  (int)c.q_qual_min);
      std::fprintf(stderr, "    threshX-High:  %u\n",
                  (unsigned)c.thresh_x_high);
      std::fprintf(stderr, "    threshX-Low:   %u\n",
                  (unsigned)c.thresh_x_low);

      if (c.cell_resel_prio_present) {
        std::fprintf(stderr, "    Priority:      %u\n",
                    (unsigned)c.cell_resel_prio);
      }
    }
  }

  if (s.carrier_freq_list_utra_tdd_present) {
    for (size_t i = 0; i < s.carrier_freq_list_utra_tdd.size(); ++i) {
      const carrier_freq_utra_tdd_s& c =
          s.carrier_freq_list_utra_tdd[i];

      std::fprintf(stderr, "  UTRA-TDD carrier %zu:\n", i + 1);
      std::fprintf(stderr, "    UARFCN:        %u\n",
                  (unsigned)c.carrier_freq);
      std::fprintf(stderr, "    q-RxLevMin:    %d dBm\n",
                  (int)c.q_rx_lev_min);
      std::fprintf(stderr, "    threshX-High:  %u\n",
                  (unsigned)c.thresh_x_high);
      std::fprintf(stderr, "    threshX-Low:   %u\n",
                  (unsigned)c.thresh_x_low);

      if (c.cell_resel_prio_present) {
        std::fprintf(stderr, "    Priority:      %u\n",
                    (unsigned)c.cell_resel_prio);
      }
    }
  }

  std::fprintf(stderr, "  t-ReselectionUTRA: %u s\n",
              (unsigned)s.t_resel_utra);
  std::fprintf(stderr, "\n");
}

static void print_sib7(const sib_type7_s& s)
{
  std::fprintf(stderr, "LTE SIB7:\n");
  std::fprintf(stderr, "  t-ReselectionGERAN: %u s\n",
               (unsigned)s.t_resel_geran);

  if (!s.carrier_freqs_info_list_present) {
    std::fprintf(stderr, "  No GERAN carrier information\n\n");
    return;
  }

  for (uint32_t i = 0; i < s.carrier_freqs_info_list.size(); ++i) {
    const carrier_freqs_info_geran_s& c =
        s.carrier_freqs_info_list[i];

    const carrier_freqs_geran_s& f = c.carrier_freqs;

    std::fprintf(stderr, "  GERAN group %u:\n",
                 (unsigned)(i + 1));

    std::fprintf(stderr, "    Band:          %s\n",
                 f.band_ind.to_string());

    std::fprintf(stderr, "    Starting ARFCN: %u\n",
                 (unsigned)f.start_arfcn);

    switch (f.following_arfcns.type().value) {
      case carrier_freqs_geran_s::following_arfcns_c_::types_opts::
          explicit_list_of_arfcns: {
        const auto& list =
            f.following_arfcns.explicit_list_of_arfcns();

        std::fprintf(stderr, "    ARFCNs:        %u",
                     (unsigned)f.start_arfcn);

        for (uint32_t j = 0; j < list.size(); ++j) {
          std::fprintf(stderr, ", %u",
                       (unsigned)list[j]);
        }

        std::fprintf(stderr, "\n");
        break;
      }

      case carrier_freqs_geran_s::following_arfcns_c_::types_opts::
          equally_spaced_arfcns: {
        const auto& eq =
            f.following_arfcns.equally_spaced_arfcns();

        std::fprintf(stderr, "    ARFCNs:        %u",
                     (unsigned)f.start_arfcn);

        uint32_t arfcn = f.start_arfcn;
        for (uint32_t j = 0; j < eq.nof_following_arfcns; ++j) {
          arfcn += eq.arfcn_spacing;
          std::fprintf(stderr, ", %u", (unsigned)arfcn);
        }

        std::fprintf(stderr, "\n");
        break;
      }

      case carrier_freqs_geran_s::following_arfcns_c_::types_opts::
          variable_bit_map_of_arfcns:
        std::fprintf(stderr, "    ARFCNs:        %u + variable bitmap\n",
                     (unsigned)f.start_arfcn);
        break;

      default:
        std::fprintf(stderr, "    ARFCNs:        unknown encoding\n");
        break;
    }

    if (c.common_info.cell_resel_prio_present) {
      std::fprintf(stderr, "    Priority:      %u\n",
                   (unsigned)c.common_info.cell_resel_prio);
    }

    std::fprintf(stderr, "    q-RxLevMin:    %u\n",
                 (unsigned)c.common_info.q_rx_lev_min);
    std::fprintf(stderr, "    threshX-High:  %u\n",
                 (unsigned)c.common_info.thresh_x_high);
    std::fprintf(stderr, "    threshX-Low:   %u\n",
                 (unsigned)c.common_info.thresh_x_low);

    if (c.common_info.p_max_geran_present) {
      std::fprintf(stderr, "    p-MaxGERAN:    %u\n",
                   (unsigned)c.common_info.p_max_geran);
    }
  }

  std::fprintf(stderr, "\n");
}

extern "C" int pdsch_ue_decode_bcch(const uint8_t* data, uint32_t nbytes)
{
  if (data == nullptr || nbytes == 0) {
    return -1;
  }

  cbit_ref bref(data, nbytes);
  bcch_dl_sch_msg_s msg;

  if (msg.unpack(bref) != SRSASN_SUCCESS) {
    return -1;
  }

  if (msg.msg.type().value !=
      bcch_dl_sch_msg_type_c::types_opts::c1) {
    return 0;
  }

  if (msg.msg.c1().type().value ==
      bcch_dl_sch_msg_type_c::c1_c_::types_opts::sys_info) {

    const sys_info_s& sys_info = msg.msg.c1().sys_info();

    if (sys_info.crit_exts.type().value !=
        sys_info_s::crit_exts_c_::types_opts::sys_info_r8) {
      return 0;
    }

    const sys_info_r8_ies_s& r8 = sys_info.crit_exts.sys_info_r8();

    for (size_t i = 0; i < r8.sib_type_and_info.size(); ++i) {
      const sib_info_item_c& item = r8.sib_type_and_info[i];

      const uint32_t seen_bit = sib_seen_bit((int)item.type().value);

      /*
       * SystemInformation is periodically rebroadcast. Print each
       * supported SIB only once for the current systemInfoValueTag.
       */
      if (seen_bit != 0 && (seen_sibs & seen_bit) != 0) {
        continue;
      }

      switch (item.type().value) {
        case sib_info_item_c::types_opts::sib2:
        print_sib2(item.sib2());
        break;

        case sib_info_item_c::types_opts::sib3:
          print_sib3(item.sib3());
          break;

        case sib_info_item_c::types_opts::sib5:
          print_sib5(item.sib5());
          lte_probe_mark_sib5();
          break;

        case sib_info_item_c::types_opts::sib6:
          print_sib6(item.sib6());
          break;

        case sib_info_item_c::types_opts::sib7:
          print_sib7(item.sib7());
          break;

        default:
          std::fprintf(stderr, "LTE SystemInformation: %s (formatter not implemented)\n\n",
                      item.type().to_string());
          break;
      }

      if (seen_bit != 0) {
        seen_sibs |= seen_bit;
      }
    }

    return 1;
  }

  if (msg.msg.c1().type().value !=
      bcch_dl_sch_msg_type_c::c1_c_::types_opts::sib_type1) {
    return 0;
  }

  const sib_type1_s& sib1 = msg.msg.c1().sib_type1();

  /*
   * Build SI scheduling table from SIB1.
   *
   * SchedulingInfo entry i corresponds to SI message n=i+1.
   * SIB2 is implicitly carried by the first SI message and therefore
   * does not appear in its sib-MappingInfo.
   */
  si_window_ms   = sib1.si_win_len.to_number();
  nof_si_entries = 0;

  for (size_t i = 0;
       i < sib1.sched_info_list.size() &&
       nof_si_entries < sizeof(si_schedule) / sizeof(si_schedule[0]);
       ++i) {

    const uint32_t period_rf = sib1.sched_info_list[i].si_periodicity.to_number();

    if (period_rf == 0 || si_window_ms == 0) {
      continue;
    }

    const uint32_t x = (uint32_t)i * si_window_ms;

    si_schedule[nof_si_entries].period_rf = (uint16_t)period_rf;
    si_schedule[nof_si_entries].start_tti = x;
    ++nof_si_entries;
  }

  have_si_schedule = nof_si_entries > 0;

  /*
   * A changed systemInfoValueTag means that the broadcast SI set has
   * changed. Allow every supported SIB to be printed again.
   */
  if (!have_si_value_tag || si_value_tag != sib1.sys_info_value_tag) {
    si_value_tag      = sib1.sys_info_value_tag;
    have_si_value_tag = true;
    seen_sibs         = 0;
  }

  /*
   * Suppress repetitions of an identical SIB1 transport block.
   *
   * SIB1 is normally repeated frequently. Comparing the actual decoded
   * transport block also means that a changed SIB1 is printed even if
   * something unexpected happens with systemInfoValueTag handling.
   */
  static uint8_t  last_data[256];
  static uint32_t last_len = 0;
  static bool     have_last = false;

  if (have_last &&
      last_len == nbytes &&
      nbytes <= sizeof(last_data) &&
      std::memcmp(last_data, data, nbytes) == 0) {
    return 1;
  }

  if (nbytes <= sizeof(last_data)) {
    std::memcpy(last_data, data, nbytes);
    last_len  = nbytes;
    have_last = true;
  }

  const std::string tac_bits  = sib1.cell_access_related_info.tac.to_string();
  const std::string cell_bits = sib1.cell_access_related_info.cell_id.to_string();

  const uint32_t tac = bitstring_to_u32(tac_bits);
  const uint32_t eci = bitstring_to_u32(cell_bits);

  /*
   * 28-bit E-UTRAN Cell Identifier:
   *
   *   ECI = eNB-ID (20 bits) || Cell-ID (8 bits)
   *
   * This is the conventional macro-eNB interpretation.
   */
  const uint32_t enb_id  = eci >> 8;
  const uint32_t cell_id = eci & 0xff;

  /*
   * Export the decoded cell identity to the machine-readable
   * probe result. Keep the human-readable SIB output independent.
   */
  lte_probe_set_cell_identity((uint16_t)tac, eci);
  lte_probe_set_band((uint8_t)sib1.freq_band_ind);

  for (size_t i = 0;
       i < sib1.cell_access_related_info.plmn_id_list.size();
       ++i) {
    probe_add_plmn(
        sib1.cell_access_related_info.plmn_id_list[i].plmn_id);
  }

  lte_probe_mark_sib1();

  std::fprintf(stderr, "\nLTE SIB1:\n");

  std::fprintf(stderr, "  PLMN:        ");
  for (size_t i = 0; i < sib1.cell_access_related_info.plmn_id_list.size(); ++i) {
    if (i != 0) {
      std::fprintf(stderr, ", ");
    }

    print_plmn(sib1.cell_access_related_info.plmn_id_list[i].plmn_id);
  }
  std::fprintf(stderr, "\n");

  std::fprintf(stderr, "  TAC:         %u (0x%04X)\n",
              (unsigned)tac,
              (unsigned)tac);

  std::fprintf(stderr, "  ECI:         %u (0x%07X)\n",
              (unsigned)eci,
              (unsigned)eci);

  std::fprintf(stderr, "  eNB ID:      %u (0x%05X)\n",
              (unsigned)enb_id,
              (unsigned)enb_id);

  std::fprintf(stderr, "  Cell ID:     %u (0x%02X)\n",
              (unsigned)cell_id,
              (unsigned)cell_id);

  std::fprintf(stderr, "  Band:        %u\n",
              (unsigned)sib1.freq_band_ind);

  std::fprintf(stderr, "  Barred:      %s\n",
              sib1.cell_access_related_info.cell_barred.to_string());

  std::fprintf(stderr, "  Reselection: %s\n",
              sib1.cell_access_related_info.intra_freq_resel.to_string());

  std::fprintf(stderr, "  q-RxLevMin:  %d dBm\n",
              (int)sib1.cell_sel_info.q_rx_lev_min);

  std::fprintf(stderr, "  SI tag:      %u\n",
              (unsigned)sib1.sys_info_value_tag);

  std::fprintf(stderr, "  SI window:   %u ms\n",
              (unsigned)sib1.si_win_len.to_number());

  std::fprintf(stderr, "\n");

  return 1;
}
