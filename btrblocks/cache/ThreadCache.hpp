#pragma once
// ------------------------------------------------------------------------------
#include "common/Log.hpp"
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
  std::ostream& operator<<([[maybe_unused]] const string& str) {
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
    if (estimation_level == 0) {
      return (log << str);
    }
#endif
    return log;
  }
  // -------------------------------------------------------------------------------------
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
  ThreadCacheContainer() { log << '\n'; }
  ~ThreadCacheContainer() {
    log << '\n';
    Log::info("{}", log.str());
  }
#endif
};
class ThreadCache {
 public:
  static ThreadCacheContainer& get() {
    thread_local ThreadCacheContainer local;
    return local;
  }
  // -------------------------------------------------------------------------------------
  static void dumpSet([[maybe_unused]] string rel_name,
                      [[maybe_unused]] string col_name,
                      [[maybe_unused]] string col_type) {
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
    get().dump_meta.rel_name = std::move(rel_name);
    get().dump_meta.col_name = std::move(col_name);
    get().dump_meta.col_type = std::move(col_type);
#endif
  }
  // ------------------------------------------------------------------------------
  static void dumpPush([[maybe_unused]] const string& scheme_name,
                       [[maybe_unused]] double cf,
                       [[maybe_unused]] u32 before,
                       [[maybe_unused]] u32 after,
                       [[maybe_unused]] u32 unique_count,
                       [[maybe_unused]] const string& comment = "") {
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
    get().estimation_deviation_csv
        << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name << '\t'
        << get().dump_meta.col_type << '\t' << get().dump_meta.chunk_i << '\t'
        << get().compression_level << '\t' << scheme_name << '\t' << cf << '\t' << before << '\t'
        << after << '\t' << CD(before) / CD(after) << '\t' << comment << '\t' << unique_count
        << '\n';
#endif
  }
  // ------------------------------------------------------------------------------
  static void dumpFsst([[maybe_unused]] u32 before_total,
                       [[maybe_unused]] u32 before_pool,
                       [[maybe_unused]] u32 after_pool,
                       [[maybe_unused]] u32 after_total) {
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
    get().fsst_csv << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name << '\t'
                   << get().dump_meta.chunk_i << '\t' << before_total << '\t' << before_pool << '\t'
                   << after_pool << '\t' << after_total << '\n';
#endif
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
