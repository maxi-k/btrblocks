#include "ThreadCache.hpp"

#include <utility>
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
tbb::enumerable_thread_specific<ThreadCacheContainer> ThreadCache::data = {};
ThreadCacheContainer& ThreadCache::get() {
  return data.local();
}
// -------------------------------------------------------------------------------------
bool ThreadCacheContainer::isOnHotPath() {
  return estimation_level == 0;
}
// -------------------------------------------------------------------------------------
void ThreadCache::dumpSet(string rel_name, string col_name, string col_type) {
  get().dump_meta.rel_name = std::move(rel_name);
  get().dump_meta.col_name = std::move(col_name);
  get().dump_meta.col_type = std::move(col_type);
}
// -------------------------------------------------------------------------------------
void ThreadCache::dumpPush(const string& scheme_name,
                           double cf,
                           u32 before_size,
                           u32 after_size,
                           u32 unique_count,
                           const string& comment) {
  get().estimation_deviation_csv << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name
                                 << '\t' << get().dump_meta.col_type << '\t'
                                 << get().dump_meta.chunk_i << '\t' << get().compression_level
                                 << '\t' << scheme_name << '\t' << cf << '\t' << before_size << '\t'
                                 << after_size << '\t' << CD(before_size) / CD(after_size) << '\t'
                                 << comment << '\t' << unique_count << '\n';
}
// -------------------------------------------------------------------------------------
void ThreadCache::dumpFsst(u32 before_total, u32 before_pool, u32 after_pool, u32 after_total) {
  get().fsst_csv << get().dump_meta.rel_name << '\t' << get().dump_meta.col_name << '\t'
                 << get().dump_meta.chunk_i << '\t' << before_total << '\t' << before_pool << '\t'
                 << after_pool << '\t' << after_total << '\n';
}
// -------------------------------------------------------------------------------------
void ThreadCache::setFsst() {
  if (get().isOnHotPath()) {
    get().fsst = true;
  }
}
// -------------------------------------------------------------------------------------
bool ThreadCache::hasUsedFsst() {
  bool fsst = get().fsst;
  get().fsst = false;
  return fsst;
}
}  // namespace btrblocks::db
