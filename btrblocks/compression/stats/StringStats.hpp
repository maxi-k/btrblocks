#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "storage/StringArrayViewer.hpp"
// -------------------------------------------------------------------------------------
#include <set>
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
// -------------------------------------------------------------------------------------
struct StringStats {
  std::set<str> distinct_values;
  // -------------------------------------------------------------------------------------
  u32 total_size;           // everything in the column including slots
  u32 total_length;         // only string starting from slots end
  u32 total_unique_length;  // only the unique (dict) strings
  u32 tuple_count;
  // -------------------------------------------------------------------------------------
  u32 null_count;
  u32 unique_count;
  u32 set_count;
  // -------------------------------------------------------------------------------------
  static StringStats generateStats(const StringArrayViewer src,
                                   const BITMAP* nullmap,
                                   u32 tuple_count,
                                   SIZE column_data_size);
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db
// -------------------------------------------------------------------------------------
