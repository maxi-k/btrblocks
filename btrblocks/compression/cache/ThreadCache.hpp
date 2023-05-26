#pragma once
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "tbb/enumerable_thread_specific.h"
// -------------------------------------------------------------------------------------
#include <sstream>
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
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
  bool isOnHotPath();
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
  static tbb::enumerable_thread_specific<ThreadCacheContainer> data;
  static ThreadCacheContainer& get();
  // -------------------------------------------------------------------------------------
  static void dumpSet(string rel_name, string col_name, string col_type);
  static void dumpPush(const string& scheme_name,
                       double cf,
                       u32 before,
                       u32 after,
                       u32 unique_count,
                       const string& comment = "");
  static void dumpFsst(u32 before_total, u32 before_pool, u32 after_pool, u32 after_total);
  // -------------------------------------------------------------------------------------
  static bool hasUsedFsst();
  static void setFsst();
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db
