#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <cmath>
#include <map>
#include <random>
#include <set>
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
template <typename T>
struct NumberStats {
 public:
  NumberStats(const T* src, const BITMAP* bitmap, u32 tuple_count)
      : src(src), bitmap(bitmap), tuple_count(tuple_count) {}
  // -------------------------------------------------------------------------------------
  const T* src;
  const BITMAP* bitmap;
  std::map<T, u32> distinct_values;
  T min;
  T max;
  NumberStats() = delete;
  // -------------------------------------------------------------------------------------
  u32 tuple_count;
  u32 total_size;
  u32 null_count;
  u32 unique_count;
  u32 set_count;
  u32 average_run_length;
  bool is_sorted;
  // -------------------------------------------------------------------------------------
  tuple<vector<T>, vector<BITMAP>> samples(u32 n, u32 length) {
    // -------------------------------------------------------------------------------------
    std::random_device rd;   // Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd());  // Standard mersenne_twister_engine seeded with
                             // rd()
    // -------------------------------------------------------------------------------------
    // TODO : Construction Site !! need a better theory and algorithm for
    // sampling Constraints: RLE(runs), nulls, uniqueness, naive approach to
    // pick enough elements that approximate stratified sample run ~8 normal
    // rounds, then check if the
    vector<T> compiled_values;
    vector<BITMAP> compiled_bitmap;
    // -------------------------------------------------------------------------------------
    if (tuple_count <= n * length) {
      compiled_values.insert(compiled_values.end(), src, src + tuple_count);
      if (bitmap != nullptr) {
        compiled_bitmap.insert(compiled_bitmap.end(), bitmap, bitmap + tuple_count);
      } else {
        compiled_bitmap.insert(compiled_bitmap.end(), tuple_count, 1);
      }
    } else {
      u32 separator =
          tuple_count / n;  // how big is the slice of the input, of which we take a part....
      u32 remainder = tuple_count % n;
      for (u32 sample_i = 0; sample_i < n; sample_i++) {
        u32 range_end = ((sample_i == n - 1) ? (separator + remainder) : separator) - length;
        std::uniform_int_distribution<> dis(0, range_end);
        u32 partition_begin = sample_i * separator + dis(gen);
        // (sample_i * separator, (sample_i + 1 ) * separator) range to pick
        // from
        compiled_values.insert(compiled_values.end(), src + partition_begin,
                               src + partition_begin + length);
        if (bitmap == nullptr) {
          compiled_bitmap.insert(compiled_bitmap.end(), length, 1);
        } else {
          compiled_bitmap.insert(compiled_bitmap.end(), bitmap + partition_begin,
                                 bitmap + partition_begin + length);
        }
      }
    }

    return std::make_tuple(compiled_values, compiled_bitmap);
  }
  // -------------------------------------------------------------------------------------
  static NumberStats generateStats(const T* src, const BITMAP* nullmap, u32 tuple_count) {
    NumberStats stats(src, nullmap, tuple_count);
    // -------------------------------------------------------------------------------------
    stats.tuple_count = tuple_count;
    stats.total_size = tuple_count * sizeof(T);
    stats.null_count = 0;
    stats.average_run_length = 0;
    stats.is_sorted = true;
    // -------------------------------------------------------------------------------------
    bool is_init_value_initialized = false;
    // -------------------------------------------------------------------------------------
    // Let NULL_CODE (0) of null values also taken into stats consideration
    // -------------------------------------------------------------------------------------
    T last_value;
    u32 run_count = 0;
    // -------------------------------------------------------------------------------------
    for (u64 row_i = 0; row_i < tuple_count; row_i++) {
      if (!is_init_value_initialized) {
        stats.min = stats.max = last_value = src[0];
        is_init_value_initialized = true;
      }
      // -------------------------------------------------------------------------------------
      auto current_value = src[row_i];
      if (current_value != last_value && (nullmap == nullptr || nullmap[row_i])) {
        if (current_value < last_value) {
          stats.is_sorted = false;
        }
        last_value = current_value;
        run_count++;
      }
      if (stats.distinct_values.find(current_value) == stats.distinct_values.end()) {
        stats.distinct_values.insert({current_value, 1});
      } else {
        stats.distinct_values[current_value]++;
      }
      if (current_value > stats.max) {
        stats.max = current_value;
      } else if (current_value < stats.min) {
        stats.min = current_value;
      }
      if (nullmap != nullptr && !nullmap[row_i]) {
        stats.null_count++;
        continue;
      }
    }
    run_count++;
    // -------------------------------------------------------------------------------------
    stats.average_run_length = CD(tuple_count) / CD(run_count);
    stats.unique_count = stats.distinct_values.size();
    stats.set_count = stats.tuple_count - stats.null_count;
    // -------------------------------------------------------------------------------------
    return stats;
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
