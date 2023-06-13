#pragma once
// ------------------------------------------------------------------------------
#include <cassert>
// ------------------------------------------------------------------------------
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
#include "roaring/roaring.hh"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// ------------------------------------------------------------------------------
template <typename NumberType>
struct FrequencyStructure {
  NumberType top_value;
  u32 exceptions_offset;
  u8 next_scheme;
  u8 data[];
};
// -------------------------------------------------------------------------------------
template <typename NumberType, typename SchemeType, typename StatsType, typename SchemeCodeType>
class TFrequency {
 public:
  static inline double expectedCompressionRatio(StatsType& stats) {
    // Threshold : stats.max larger than 1 byte (b/c PBP would do just as well
    // as frequency) top_1 occurrence pct >= 90%
    // TODO: run it on a sample
    if (stats.unique_count <= 1) {
      return 0;
    }

    if (CD(stats.null_count) * 100.0 / CD(stats.tuple_count) >= 90) {
      return stats.tuple_count - 1;
    }

    u32 occurence_count = stats.distinct_values.begin()->second;
    for (const auto& t : stats.distinct_values) {
      if (t.second > occurence_count) {
        occurence_count = t.second;
      }
    }

    if (CD(occurence_count) * 100.0 / CD(stats.tuple_count) >= 90) {
      if (stats.max >= (1 << 8)) {
        return stats.tuple_count - 1;
      }
    }
    return 0;
  }
  // -------------------------------------------------------------------------------------
  static inline u32 compressColumn(const NumberType* src,
                                   const BITMAP* nullmap,
                                   u8* dest,
                                   StatsType& stats,
                                   u8 allowed_cascading_level) {
    auto& col_struct = *new (dest)(FrequencyStructure<NumberType>);
    // -------------------------------------------------------------------------------------
    if (CD(stats.null_count) * 100.0 / CD(stats.tuple_count) >= 90) {
      col_struct.top_value = NULL_CODE;
    } else {
      col_struct.top_value = stats.distinct_values.begin()->first;
      u32 occurence_count = stats.distinct_values.begin()->second;
      for (const auto& t : stats.distinct_values) {
        if (t.second > occurence_count) {
          occurence_count = t.second;
          col_struct.top_value = t.first;
        }
      }
    }
    // -------------------------------------------------------------------------------------
    vector<NumberType> exceptions;
    Roaring exceptions_bitmap;
    for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
      if (src[row_i] != col_struct.top_value && (nullmap == nullptr || nullmap[row_i])) {
        exceptions.push_back(src[row_i]);
        exceptions_bitmap.add(row_i);
      }
    }
    die_if(exceptions_bitmap.cardinality() == exceptions.size());
    // -------------------------------------------------------------------------------------
    auto write_ptr = col_struct.data;
    // -------------------------------------------------------------------------------------
    exceptions_bitmap.runOptimize();
    exceptions_bitmap.setCopyOnWrite(true);
    write_ptr += exceptions_bitmap.write(reinterpret_cast<char*>(write_ptr), false);
    col_struct.exceptions_offset = write_ptr - col_struct.data;
    // -------------------------------------------------------------------------------------
    {
      // Compress exceptions
      u32 used_space;
      CSchemePicker<NumberType, SchemeType, StatsType, SchemeCodeType>::compress(
          exceptions.data(), nullptr, write_ptr, exceptions.size(), allowed_cascading_level - 1,
          used_space, col_struct.next_scheme, autoScheme(), "exceptions");
      write_ptr += used_space;
    }
    // -------------------------------------------------------------------------------------
    return write_ptr - dest;
  }
  // -------------------------------------------------------------------------------------
  static inline void decompressColumn(NumberType* dest,
                                      BitmapWrapper*,
                                      const u8* src,
                                      u32 tuple_count,
                                      u32 level) {
    const auto& col_struct = *reinterpret_cast<const FrequencyStructure<NumberType>*>(src);
    // -------------------------------------------------------------------------------------
    Roaring exceptions_bitmap =
        Roaring::read(reinterpret_cast<const char*>(col_struct.data), false);
    thread_local std::vector<std::vector<NumberType>> exceptions_v;
    auto exceptions = get_level_data(
        exceptions_v, exceptions_bitmap.cardinality() + SIMD_EXTRA_ELEMENTS(NumberType), level);
    if (exceptions_bitmap.cardinality() > 0) {
      CSchemePicker<NumberType, SchemeType, StatsType, SchemeCodeType>::MyTypeWrapper::getScheme(
          col_struct.next_scheme)
          .decompress(exceptions, nullptr, col_struct.data + col_struct.exceptions_offset,
                      exceptions_bitmap.cardinality(), level + 1);
    }
    // -------------------------------------------------------------------------------------
    // First write the top value to every single entry
    for (u32 i = 0; i < tuple_count; i++) {
      dest[i] = col_struct.top_value;
    }

    // Now fix every single entry that is an exception
    std::pair<NumberType*, NumberType*> param = {dest, exceptions};
    exceptions_bitmap.iterate(
        [](uint32_t value, void* param) {
          // TODO are the values actually being sorted here??? If not this won't
          // work.
          auto p = reinterpret_cast<std::pair<NumberType*, NumberType*>*>(param);
          // dest[value] = *exceptions_ptr++;
          p->first[value] = *(p->second);
          p->second++;
          return true;
        },
        &param);
  }
  // -------------------------------------------------------------------------------------
  static inline string fullDescription(const u8* src, const string& selfDescription) {
    const auto& col_struct = *reinterpret_cast<const FrequencyStructure<NumberType>*>(src);
    auto result = selfDescription;

    Roaring exceptions_bitmap =
        Roaring::read(reinterpret_cast<const char*>(col_struct.data), false);
    if (exceptions_bitmap.cardinality() > 0) {
      auto& scheme =
          CSchemePicker<NumberType, SchemeType, StatsType,
                        SchemeCodeType>::MyTypeWrapper::getScheme(col_struct.next_scheme);
      result += " -> ([NumberType] exceptions) " +
                scheme.fullDescription(col_struct.data + col_struct.exceptions_offset);
    }

    return result;
  }
};
}  // namespace btrblocks
