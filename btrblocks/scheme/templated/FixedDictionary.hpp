#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
template <typename NumberType>
struct FixedDictionaryStructure {
  u32 codes_offset;
  NumberType dict_slots[];
};
// -------------------------------------------------------------------------------------
template <typename CodeType, typename NumberType, typename StatsType>
inline double FDictExpectedCompressionRatio(StatsType& stats) {
  if (stats.unique_count > (std::numeric_limits<CodeType>::max() + 1)) {
    return 0;
  } else {
    const u32 after_size = sizeof(FixedDictionaryStructure<NumberType>) +
                           (stats.unique_count * sizeof(NumberType)) +
                           (stats.tuple_count * sizeof(CodeType));
    return static_cast<double>(stats.total_size) / static_cast<double>(after_size);
  }
}
// -------------------------------------------------------------------------------------
template <typename CodeType, typename NumberType, typename StatsType>
inline u32 FDictCompressColumn(const NumberType* src, const BITMAP*, u8* dest, StatsType& stats) {
  die_if(stats.distinct_values.size() <= (std::numeric_limits<CodeType>::max() + 1));
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<FixedDictionaryStructure<NumberType>*>(dest);
  const u32 dict_size = stats.distinct_values.size() * sizeof(NumberType);
  col_struct.codes_offset = sizeof(FixedDictionaryStructure<NumberType>) + dict_size;
  // -------------------------------------------------------------------------------------
  // Write dictionary
  u32 distinct_i = 0;
  for (const auto& distinct_element : stats.distinct_values) {
    col_struct.dict_slots[distinct_i] = distinct_element.first;
    distinct_i++;
  }
  // -------------------------------------------------------------------------------------
  NumberType* dict_begin = col_struct.dict_slots;
  NumberType* dict_end = dict_begin + stats.unique_count;
  // -------------------------------------------------------------------------------------
  auto codes = reinterpret_cast<CodeType*>(dest + col_struct.codes_offset);
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    auto it = std::lower_bound(dict_begin, dict_end, src[row_i]);
    if (it == dict_end) {
      die_if(stats.distinct_values.find(src[row_i]) != stats.distinct_values.end());
    }
    die_if(it != dict_end);
    codes[row_i] = static_cast<CodeType>(std::distance(dict_begin, it));
  }
  // -------------------------------------------------------------------------------------
  return reinterpret_cast<u8*>(&codes[stats.tuple_count]) - dest;
}
// -------------------------------------------------------------------------------------
template <typename CodeType, typename NumberType>
inline void FDictDecompressColumn(NumberType* dest,
                                  BitmapWrapper*,
                                  const u8* src,
                                  u32 tuple_count,
                                  u32 level) {
  // -------------------------------------------------------------------------------------
  const auto& col_struct = *reinterpret_cast<const FixedDictionaryStructure<NumberType>*>(src);
  // -------------------------------------------------------------------------------------
  // Get codes
  const auto codes = reinterpret_cast<const CodeType*>(src + col_struct.codes_offset);
  // -------------------------------------------------------------------------------------
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] = col_struct.dict_slots[codes[row_i]];
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
