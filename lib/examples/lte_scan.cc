#include "lte_earfcn.h"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <fstream>
#include <set>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

struct scan_result {
  uint32_t earfcn = 0;
  uint32_t band = 0;
  double frequency_hz = 0.0;
  int exit_code = -1;
  uint32_t probe_count = 1;
  std::string status;
  std::string json;

  bool have_mib = false;
  bool have_sib1 = false;
  bool have_sib5 = false;

  uint32_t pci = 0;
  uint32_t nof_prb = 0;
  double bandwidth_mhz = 0.0;
  double snr_db = 0.0;
  uint32_t tac = 0;
  uint32_t eci = 0;
  uint32_t enb_id = 0;
  uint32_t cell_id = 0;

  std::vector<std::string> plmns;
  std::vector<uint32_t> neighbors;
};

static void usage(const char* prog)
{
  std::fprintf(stderr,
               "Usage: %s (--earfcn LIST | --seed EARFCN) [--gain DB] [--attempts N] "
               "[--subframes N] [--retries N] [--json FILE]\n"
               "\n"
               "Options:\n"
               "  -e, --earfcn LIST    Comma-separated LTE DL EARFCNs\n"
              "  -s, --seed EARFCN    Discover carriers recursively via SIB5\n"
               "  -J, --json FILE      Write complete scan result as JSON\n"
              "  -g, --gain DB        RF gain passed to pdsch_ue (default: 40)\n"
               "  -X, --attempts N     Cell-search attempts per carrier (default: 2)\n"
               "  -n, --subframes N    Max subframes after cell detection "
               "(default: 5000)\n"
               "  -R, --retries N      Additional decode retries in seed mode "
              "(default: 1)\n"
              "  -h, --help           Show this help\n",
               prog);
}

static bool parse_u32(const std::string& s, uint32_t* value)
{
  if (s.empty()) {
    return false;
  }

  char* end = nullptr;
  errno = 0;
  unsigned long v = std::strtoul(s.c_str(), &end, 10);

  if (errno != 0 || end == s.c_str() || *end != '\0' ||
      v > 0xffffffffUL) {
    return false;
  }

  *value = static_cast<uint32_t>(v);
  return true;
}

static bool parse_earfcn_list(const std::string& list,
                              std::vector<uint32_t>* out)
{
  std::stringstream ss(list);
  std::string item;

  while (std::getline(ss, item, ',')) {
    uint32_t earfcn = 0;

    if (!parse_u32(item, &earfcn)) {
      return false;
    }

    out->push_back(earfcn);
  }

  return !out->empty();
}

static bool read_file(const std::string& filename, std::string* data)
{
  std::ifstream f(filename);
  if (!f) {
    return false;
  }

  std::ostringstream ss;
  ss << f.rdbuf();
  *data = ss.str();
  return true;
}

static std::string json_string(const std::string& json,
                               const std::string& key)
{
  const std::string needle = "\"" + key + "\":\"";
  size_t p = json.find(needle);

  if (p == std::string::npos) {
    return {};
  }

  p += needle.size();
  size_t e = json.find('"', p);

  if (e == std::string::npos) {
    return {};
  }

  return json.substr(p, e - p);
}


static bool json_bool(const std::string& json,
                      const std::string& key,
                      bool* value)
{
  const std::string needle = "\"" + key + "\":";
  size_t p = json.find(needle);

  if (p == std::string::npos) {
    return false;
  }

  p += needle.size();

  while (p < json.size() &&
         (json[p] == ' ' || json[p] == '\t' ||
          json[p] == '\r' || json[p] == '\n')) {
    ++p;
  }

  if (json.compare(p, 4, "true") == 0) {
    *value = true;
    return true;
  }

  if (json.compare(p, 5, "false") == 0) {
    *value = false;
    return true;
  }

  return false;
}

static bool json_number(const std::string& json,
                        const std::string& key,
                        double* value)
{
  const std::string needle = "\"" + key + "\":";
  size_t p = json.find(needle);

  if (p == std::string::npos) {
    return false;
  }

  p += needle.size();

  char* end = nullptr;
  *value = std::strtod(json.c_str() + p, &end);

  return end != json.c_str() + p;
}

static bool json_u32(const std::string& json,
                     const std::string& key,
                     uint32_t* value)
{
  double v = 0.0;

  if (!json_number(json, key, &v) || v < 0.0) {
    return false;
  }

  *value = static_cast<uint32_t>(v);
  return true;
}

static std::vector<uint32_t> json_u32_array(const std::string& json,
                                             const std::string& key)
{
  std::vector<uint32_t> values;

  const std::string needle = "\"" + key + "\":[";
  size_t p = json.find(needle);

  if (p == std::string::npos) {
    return values;
  }

  p += needle.size();

  size_t end = json.find(']', p);
  if (end == std::string::npos) {
    return values;
  }

  std::string body = json.substr(p, end - p);
  std::stringstream ss(body);
  std::string item;

  while (std::getline(ss, item, ',')) {
    uint32_t value = 0;

    if (parse_u32(item, &value)) {
      values.push_back(value);
    }
  }

  return values;
}

static std::vector<std::string> json_plmns(const std::string& json)
{
  std::vector<std::string> values;

  const std::string needle = "\"plmns\":[";
  size_t p = json.find(needle);

  if (p == std::string::npos) {
    return values;
  }

  p += needle.size();

  size_t end = json.find(']', p);
  if (end == std::string::npos) {
    return values;
  }

  std::string body = json.substr(p, end - p);
  size_t pos = 0;

  while (true) {
    size_t begin = body.find('{', pos);
    if (begin == std::string::npos) {
      break;
    }

    size_t finish = body.find('}', begin);
    if (finish == std::string::npos) {
      break;
    }

    std::string item = body.substr(begin, finish - begin + 1);

    uint32_t mcc = 0;
    uint32_t mnc = 0;
    uint32_t digits = 0;

    if (json_u32(item, "mcc", &mcc) &&
        json_u32(item, "mnc", &mnc) &&
        json_u32(item, "mnc_digits", &digits)) {
      char buf[32];

      if (digits == 3) {
        std::snprintf(buf, sizeof(buf), "%03u-%03u", mcc, mnc);
      } else {
        std::snprintf(buf, sizeof(buf), "%03u-%02u", mcc, mnc);
      }

      values.emplace_back(buf);
    }

    pos = finish + 1;
  }

  return values;
}

static std::string join_strings(const std::vector<std::string>& values,
                                const char* separator)
{
  std::ostringstream ss;

  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      ss << separator;
    }

    ss << values[i];
  }

  return ss.str();
}

static std::string join_u32(const std::vector<uint32_t>& values,
                            const char* separator)
{
  std::ostringstream ss;

  for (size_t i = 0; i < values.size(); ++i) {
    if (i != 0) {
      ss << separator;
    }

    ss << values[i];
  }

  return ss.str();
}

static std::string shell_quote(const std::string& s)
{
  std::string out = "'";

  for (char c : s) {
    if (c == '\'') {
      out += "'\\''";
    } else {
      out += c;
    }
  }

  out += "'";
  return out;
}

static int decode_system_status(int status)
{
  if (status == -1) {
    return -1;
  }

  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }

  if (WIFSIGNALED(status)) {
    return 128 + WTERMSIG(status);
  }

  return -1;
}

static scan_result probe(const std::string& pdsch_ue,
                         uint32_t earfcn,
                         double gain,
                         uint32_t attempts,
                         uint32_t subframes)
{
  scan_result r;
  r.earfcn = earfcn;

  uint8_t band = 0;
  double frequency_hz = 0.0;

  if (lte_earfcn_to_frequency(earfcn, &band, &frequency_hz) != 0) {
    r.status = "invalid_earfcn";
    return r;
  }

  r.frequency_hz = frequency_hz;
  r.band = band;

  char tmp_template[] = "/tmp/lte_scan_XXXXXX";
  int fd = mkstemp(tmp_template);

  if (fd < 0) {
    r.status = "tempfile_error";
    return r;
  }

  close(fd);
  unlink(tmp_template);

  const std::string json_file = tmp_template;

  std::ostringstream cmd;
  cmd << shell_quote(pdsch_ue)
      << " -I soapy"
      << " -a " << shell_quote("driver=hackrf")
      << " -f " << static_cast<unsigned long long>(frequency_hz)
      << " -g " << gain
      << " -X " << attempts
      << " -J " << shell_quote(json_file)
      << " -n " << subframes
      << " -Q"
      << " >/dev/null 2>/dev/null";

  int system_status = std::system(cmd.str().c_str());
  r.exit_code = decode_system_status(system_status);

  if (!read_file(json_file, &r.json)) {
    r.status = "probe_error";
    unlink(json_file.c_str());
    return r;
  }

  unlink(json_file.c_str());

  r.status = json_string(r.json, "status");

  if (r.status.empty()) {
    r.status = "invalid_json";
    return r;
  }

  json_bool(r.json, "have_mib", &r.have_mib);
  json_bool(r.json, "have_sib1", &r.have_sib1);
  json_bool(r.json, "have_sib5", &r.have_sib5);

  if (r.status == "ok" || r.status == "partial") {
    json_u32(r.json, "pci", &r.pci);
    json_u32(r.json, "nof_prb", &r.nof_prb);
    json_number(r.json, "bandwidth_mhz", &r.bandwidth_mhz);
    json_number(r.json, "snr_db", &r.snr_db);
    json_u32(r.json, "tac", &r.tac);
    json_u32(r.json, "eci", &r.eci);
    json_u32(r.json, "enb_id", &r.enb_id);
    json_u32(r.json, "cell_id", &r.cell_id);

    r.plmns = json_plmns(r.json);
    r.neighbors = json_u32_array(r.json, "neighbors");
  }

  return r;
}

static void merge_probe_result(scan_result* dst,
                               const scan_result& src)
{
  dst->probe_count += src.probe_count;

  /*
   * Keep the most recent successful RF measurement.
   */
  if (src.have_mib) {
    dst->pci = src.pci;
    dst->nof_prb = src.nof_prb;
    dst->bandwidth_mhz = src.bandwidth_mhz;
    dst->snr_db = src.snr_db;
    dst->have_mib = true;
  }

  /*
   * SIB1 contains the cell identity. Once decoded, do not lose it
   * because a later retry happened to miss SIB1.
   */
  if (src.have_sib1) {
    dst->tac = src.tac;
    dst->eci = src.eci;
    dst->enb_id = src.enb_id;
    dst->cell_id = src.cell_id;
    dst->plmns = src.plmns;
    dst->have_sib1 = true;
  }

  /*
   * SIB5 is independently useful for recursive discovery.
   */
  if (src.have_sib5) {
    dst->neighbors = src.neighbors;
    dst->have_sib5 = true;
  }

  if (dst->have_sib1) {
    dst->status = "ok";
    dst->exit_code = 0;
  } else if (dst->have_mib) {
    dst->status = "partial";
    dst->exit_code = 0;
  } else {
    dst->status = src.status;
    dst->exit_code = src.exit_code;
  }
}

static int write_scan_json(const std::string& filename,
                           const std::string& mode,
                           uint32_t seed_earfcn,
                           double gain,
                           uint32_t attempts,
                           uint32_t subframes,
                           uint32_t retries,
                           const std::vector<scan_result>& results)
{
  const std::string tmp_filename = filename + ".tmp";
  FILE* f = std::fopen(tmp_filename.c_str(), "w");

  if (!f) {
    return -1;
  }

  size_t ok_count = 0;
  size_t partial_count = 0;

  for (const scan_result& r : results) {
    if (r.status == "ok") {
      ++ok_count;
    } else if (r.status == "partial") {
      ++partial_count;
    }
  }

  std::fprintf(f, "{\n");
  std::fprintf(f, "  \"mode\":\"%s\"", mode.c_str());

  if (mode == "seed") {
    std::fprintf(f, ",\n  \"seed_earfcn\":%u", seed_earfcn);
  }

  std::fprintf(f, ",\n  \"gain_db\":%.1f", gain);
  std::fprintf(f, ",\n  \"attempts\":%u", attempts);
  std::fprintf(f, ",\n  \"subframes\":%u", subframes);
  std::fprintf(f, ",\n  \"retries\":%u", retries);

  std::fprintf(f, ",\n  \"summary\":{");
  std::fprintf(f, "\"carriers_probed\":%zu", results.size());
  std::fprintf(f, ",\"cells_detected\":%zu",
               ok_count + partial_count);
  std::fprintf(f, ",\"cells_identified\":%zu", ok_count);
  std::fprintf(f, ",\"partial\":%zu", partial_count);
  std::fprintf(f, "}");

  std::fprintf(f, ",\n  \"results\":[\n");

  for (size_t i = 0; i < results.size(); ++i) {
    const scan_result& r = results[i];

    std::fprintf(f, "    {");
    std::fprintf(f, "\"status\":\"%s\"", r.status.c_str());
    std::fprintf(f, ",\"earfcn\":%u", r.earfcn);
    std::fprintf(f, ",\"frequency_hz\":%.0f", r.frequency_hz);
    std::fprintf(f, ",\"exit_code\":%d", r.exit_code);
    std::fprintf(f, ",\"probe_count\":%u", r.probe_count);

    std::fprintf(f, ",\"have_mib\":%s",
                 r.have_mib ? "true" : "false");
    std::fprintf(f, ",\"have_sib1\":%s",
                 r.have_sib1 ? "true" : "false");
    std::fprintf(f, ",\"have_sib5\":%s",
                 r.have_sib5 ? "true" : "false");

    if (r.status == "ok" || r.status == "partial") {
      std::fprintf(f, ",\"band\":%u", r.band);
      std::fprintf(f, ",\"pci\":%u", r.pci);
      std::fprintf(f, ",\"nof_prb\":%u", r.nof_prb);
      std::fprintf(f, ",\"bandwidth_mhz\":%.1f", r.bandwidth_mhz);
      std::fprintf(f, ",\"snr_db\":%.1f", r.snr_db);

      /*
       * SIB1-derived identity fields are meaningful only when
       * SIB1 was actually decoded.
       */
      if (r.have_sib1) {
        std::fprintf(f, ",\"tac\":%u", r.tac);
        std::fprintf(f, ",\"eci\":%u", r.eci);
        std::fprintf(f, ",\"enb_id\":%u", r.enb_id);
        std::fprintf(f, ",\"cell_id\":%u", r.cell_id);

        std::fprintf(f, ",\"plmns\":[");
        for (size_t j = 0; j < r.plmns.size(); ++j) {
          if (j != 0) {
            std::fprintf(f, ",");
          }
          std::fprintf(f, "\"%s\"", r.plmns[j].c_str());
        }
        std::fprintf(f, "]");
      }

      if (r.have_sib5) {
        std::fprintf(f, ",\"neighbors\":[");
        for (size_t j = 0; j < r.neighbors.size(); ++j) {
          if (j != 0) {
            std::fprintf(f, ",");
          }
          std::fprintf(f, "%u", r.neighbors[j]);
        }
        std::fprintf(f, "]");
      }
    }

    std::fprintf(f, "}%s\n",
                 i + 1 == results.size() ? "" : ",");
  }

  std::fprintf(f, "  ]\n");
  std::fprintf(f, "}\n");

  if (std::fclose(f) != 0) {
    unlink(tmp_filename.c_str());
    return -1;
  }

  if (std::rename(tmp_filename.c_str(), filename.c_str()) != 0) {
    unlink(tmp_filename.c_str());
    return -1;
  }

  return 0;
}

int main(int argc, char** argv)
{
  std::string earfcn_arg;
  std::string seed_arg;
  std::string json_output;
  double gain = 40.0;
  uint32_t attempts = 2;
  uint32_t subframes = 5000;
  uint32_t retries = 1;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];

    auto require_value = [&](const char* option) -> const char* {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "%s requires an argument\n", option);
        std::exit(2);
      }
      return argv[++i];
    };

    if (arg == "-e" || arg == "--earfcn") {
      earfcn_arg = require_value(arg.c_str());
    } else if (arg == "-s" || arg == "--seed") {
      seed_arg = require_value(arg.c_str());
    } else if (arg == "-J" || arg == "--json") {
      json_output = require_value(arg.c_str());
    } else if (arg == "-g" || arg == "--gain") {
      gain = std::strtod(require_value(arg.c_str()), nullptr);
    } else if (arg == "-X" || arg == "--attempts") {
      attempts =
          static_cast<uint32_t>(std::strtoul(require_value(arg.c_str()),
                                             nullptr,
                                             10));
    } else if (arg == "-R" || arg == "--retries") {
      retries =
          static_cast<uint32_t>(std::strtoul(require_value(arg.c_str()),
                                             nullptr,
                                             10));
    } else if (arg == "-n" || arg == "--subframes") {
      subframes =
          static_cast<uint32_t>(std::strtoul(require_value(arg.c_str()),
                                             nullptr,
                                             10));
    } else if (arg == "-h" || arg == "--help") {
      usage(argv[0]);
      return 0;
    } else {
      std::fprintf(stderr, "Unknown argument: %s\n", arg.c_str());
      usage(argv[0]);
      return 2;
    }
  }

  if (earfcn_arg.empty() && seed_arg.empty()) {
    std::fprintf(stderr,
                 "Either --earfcn or --seed must be specified\n");
    usage(argv[0]);
    return 2;
  }

  if (!earfcn_arg.empty() && !seed_arg.empty()) {
    std::fprintf(stderr,
                 "--earfcn and --seed are mutually exclusive\n");
    return 2;
  }

  std::vector<uint32_t> earfcns;
  bool seed_mode = !seed_arg.empty();

  if (seed_mode) {
    uint32_t seed = 0;

    if (!parse_u32(seed_arg, &seed)) {
      std::fprintf(stderr, "Invalid seed EARFCN: %s\n",
                   seed_arg.c_str());
      return 2;
    }

    earfcns.push_back(seed);
  } else {
    if (!parse_earfcn_list(earfcn_arg, &earfcns)) {
      std::fprintf(stderr, "Invalid EARFCN list: %s\n",
                   earfcn_arg.c_str());
      return 2;
    }
  }

  /*
   * pdsch_ue and lte_scan are built into the same directory.
   */
  std::string self = argv[0];
  size_t slash = self.find_last_of('/');
  std::string directory =
      slash == std::string::npos ? "." : self.substr(0, slash);

  const std::string pdsch_ue = directory + "/pdsch_ue";

  std::printf("%-7s %-9s %-4s %-5s %-13s %-5s %-6s %-6s %-8s %-5s %s\n",
              "EARFCN",
              "MHz",
              "Band",
              "PCI",
              "PLMN",
              "BW",
              "SNR",
              "TAC",
              "eNB",
              "Cell",
              "Status");

  std::printf("%-7s %-9s %-4s %-5s %-13s %-5s %-6s %-6s %-8s %-5s %s\n",
              "------",
              "-------",
              "----",
              "---",
              "----",
              "--",
              "---",
              "---",
              "---",
              "----",
              "------");

  std::set<uint32_t> visited;
  std::set<uint32_t> scheduled;

  for (uint32_t earfcn : earfcns) {
    scheduled.insert(earfcn);
  }

  std::vector<scan_result> results;

  for (size_t scan_index = 0; scan_index < earfcns.size(); ++scan_index) {
    uint32_t earfcn = earfcns[scan_index];

    if (seed_mode) {
      if (!visited.insert(earfcn).second) {
        continue;
      }
    }

    uint8_t band = 0;
    double frequency_hz = 0.0;

    if (lte_earfcn_to_frequency(earfcn, &band, &frequency_hz) != 0) {
      std::printf("%-7u %-9s %-4s %-5s %-13s %-5s %-6s %-6s %-8s %-5s %s\n",
                  earfcn, "-", "-", "-", "-", "-", "-", "-", "-", "-",
                  "invalid");
      continue;
    }

    std::fprintf(stderr,
                 "Scanning EARFCN %u (Band %u, %.1f MHz)...\n",
                 earfcn,
                 static_cast<unsigned>(band),
                 frequency_hz / 1e6);

    scan_result r =
        probe(pdsch_ue, earfcn, gain, attempts, subframes);

    /*
     * In seed mode a confirmed LTE carrier may need another receive
     * window to obtain SIB1 or SIB5. Do not retry no_cell carriers:
     * that would make recursive discovery unnecessarily expensive.
     */
    if (seed_mode && r.have_mib) {
      for (uint32_t retry = 0;
           retry < retries && (!r.have_sib1 || !r.have_sib5);
           ++retry) {
        const char* target =
            !r.have_sib1 ? "SIB1" : "SIB5";

        std::fprintf(stderr,
                     "Retrying EARFCN %u for %s (%u/%u)...\n",
                     earfcn,
                     target,
                     retry + 1,
                     retries);

        scan_result retry_result =
            probe(pdsch_ue, earfcn, gain, attempts, subframes);

        merge_probe_result(&r, retry_result);
      }
    }

    results.push_back(r);

    if (r.status == "ok" || r.status == "partial") {
      const std::string plmn = join_strings(r.plmns, ",");

      char bw[16];
      char snr[16];

      std::snprintf(bw, sizeof(bw), "%.1f", r.bandwidth_mhz);
      std::snprintf(snr, sizeof(snr), "%.1f", r.snr_db);

      std::printf("%-7u %-9.1f %-4u %-5u %-13s %-5s %-6s %-6u %-8u %-5u %s\n",
                  earfcn,
                  frequency_hz / 1e6,
                  static_cast<unsigned>(band),
                  r.pci,
                  plmn.c_str(),
                  bw,
                  snr,
                  r.tac,
                  r.enb_id,
                  r.cell_id,
                  r.status.c_str());

      if (!r.neighbors.empty()) {
        std::printf("        neighbors: %s\n",
                    join_u32(r.neighbors, ",").c_str());

        if (seed_mode && r.status == "ok") {
          for (uint32_t neighbor : r.neighbors) {
            if (scheduled.insert(neighbor).second) {
              earfcns.push_back(neighbor);
            }
          }
        }
      }
    } else {
      std::printf("%-7u %-9.1f %-4u %-5s %-13s %-5s %-6s %-6s %-8s %-5s %s\n",
                  earfcn,
                  frequency_hz / 1e6,
                  static_cast<unsigned>(band),
                  "-",
                  "-",
                  "-",
                  "-",
                  "-",
                  "-",
                  "-",
                  r.status.c_str());
    }

    std::fflush(stdout);
  }

  size_t ok_count = 0;
  size_t partial_count = 0;

  for (const scan_result& r : results) {
    if (r.status == "ok") {
      ++ok_count;
    } else if (r.status == "partial") {
      ++partial_count;
    }
  }

  std::printf("\n");
  std::printf("Scan complete: %zu carriers probed, "
              "%zu cells detected, %zu identified, %zu partial\n",
              results.size(),
              ok_count + partial_count,
              ok_count,
              partial_count);

  if (ok_count + partial_count > 0) {
    std::printf("\n");
    std::printf("%-7s %-9s %-4s %-5s %-13s %-5s %-6s "
                "%-6s %-10s %-8s %-5s %s\n",
                "EARFCN",
                "MHz",
                "Band",
                "PCI",
                "PLMN",
                "BW",
                "SNR",
                "TAC",
                "ECI",
                "eNB",
                "Cell",
                "Status");

    std::printf("%-7s %-9s %-4s %-5s %-13s %-5s %-6s "
                "%-6s %-10s %-8s %-5s %s\n",
                "------",
                "-------",
                "----",
                "---",
                "----",
                "--",
                "---",
                "---",
                "---",
                "---",
                "----",
                "------");

    for (const scan_result& r : results) {
      if (r.status != "ok" && r.status != "partial") {
        continue;
      }

      uint8_t band = 0;
      double frequency_hz = r.frequency_hz;

      if (lte_earfcn_to_frequency(r.earfcn,
                                  &band,
                                  &frequency_hz) != 0) {
        continue;
      }

      const std::string plmn = join_strings(r.plmns, ",");

      char bw[16];
      char snr[16];

      std::snprintf(bw, sizeof(bw), "%.1f", r.bandwidth_mhz);
      std::snprintf(snr, sizeof(snr), "%.1f", r.snr_db);

      std::printf("%-7u %-9.1f %-4u %-5u %-13s %-5s %-6s "
                  "%-6u %-10u %-8u %-5u %s\n",
                  r.earfcn,
                  frequency_hz / 1e6,
                  static_cast<unsigned>(band),
                  r.pci,
                  plmn.empty() ? "-" : plmn.c_str(),
                  bw,
                  snr,
                  r.tac,
                  r.eci,
                  r.enb_id,
                  r.cell_id,
                  r.status.c_str());
    }
  }

  if (!json_output.empty()) {
    const std::string mode = seed_mode ? "seed" : "earfcn";
    uint32_t seed_earfcn = 0;

    if (seed_mode && !earfcns.empty()) {
      seed_earfcn = earfcns.front();
    }

    if (write_scan_json(json_output,
                        mode,
                        seed_earfcn,
                        gain,
                        attempts,
                        subframes,
                        retries,
                        results) != 0) {
      std::fprintf(stderr,
                   "Could not write scan JSON: %s\n",
                   json_output.c_str());
      return 1;
    }
  }

  return 0;
}
