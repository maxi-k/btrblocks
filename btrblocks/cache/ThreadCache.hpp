#pragma once
// ------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <sstream>
// -------------------------------------------------------------------------------------
namespace btrblocks {
struct ThreadCacheContainer {
  struct DumpMeta {
    // rel col_n col_t block_n level_n estimated_cf before after
    string rel_name;
    string col_name;
    string col_type;
    u32 chunk_i;
  };
  DumpMeta dump_meta;
  std::stringstream estimation_deviation_csv;  // decision_tree
  std::stringstream fsst_csv;                  // fsst
  // -------------------------------------------------------------------------------------
  bool isOnHotPath() { return estimation_level == 0; }
  std::stringstream log;  // decision_tree
  u16 estimation_level = 0;
  u16 compression_level = 0;
  // -------------------------------------------------------------------------------------
  bool fsst = false;
  // -------------------------------------------------------------------------------------
  std::ostream& operator<<(const string& str) {
    if (estimation_level == 0) {
      return (log << str);
    }
    return log;
  }
  // -------------------------------------------------------------------------------------
};
class ThreadCache {
 public:
  static ThreadCacheContainer& get() {
    thread_local ThreadCacheContainer local;
    return local;
  }
  // -------------------------------------------------------------------------------------
  static void dumpSet(string rel_name, string col_name, string col_type) {
    get().dump_meta.rel_name = std::move(rel_name);
    get().dump_meta.col_name = std::move(col_name);
    get().dump_meta.col_type = std::move(col_type);
  }
  // ------------------------------------------------------------------------------
  static void dumpPush(const string& scheme_name,
                       double cf,
                       u32 before,
                       u32 after,
                       u32 unique_count,
                       const string& comment = "") {
    get().estimation_deviation_csv
        << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name << '\t'
        << get().dump_meta.col_type << '\t' << get().dump_meta.chunk_i << '\t'
        << get().compression_level << '\t' << scheme_name << '\t' << cf << '\t' << before << '\t'
        << after << '\t' << CD(before) / CD(after) << '\t' << comment << '\t' << unique_count
        << '\n';
  }
  // ------------------------------------------------------------------------------
  static void dumpFsst(u32 before_total, u32 before_pool, u32 after_pool, u32 after_total) {
    get().fsst_csv << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name << '\t'
                   << get().dump_meta.chunk_i << '\t' << before_total << '\t' << before_pool << '\t'
                   << after_pool << '\t' << after_total << '\n';
  }
  // -------------------------------------------------------------------------------------
  static bool hasUsedFsst() {
    bool fsst = get().fsst;
    get().fsst = false;
    return fsst;
  }
  // ------------------------------------------------------------------------------
  static void setFsst() {
    if (get().isOnHotPath()) {
      get().fsst = true;
    }
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
