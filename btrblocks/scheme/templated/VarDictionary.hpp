#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy {
// ------------------------------------------------------------------------------
// Note: Here the first code is reserved for 0 size (or null) strings
// -------------------------------------------------------------------------------------
struct VarDictionaryStructure {
  u32 total_size;
  u32 codes_offset;
  u8 data[];
};
// -------------------------------------------------------------------------------------
template <typename CodeType>
inline double VDictExpectedCompressionRatio(StringStats& stats) {
  if (stats.distinct_values.size() > (std::numeric_limits<CodeType>::max())) {
    return 0;
  } else {
    u32 after_size = (stats.tuple_count * (sizeof(CodeType))) +
                     (sizeof(StringArrayViewer::Slot) * (1 + stats.distinct_values.size())) +
                     sizeof(VarDictionaryStructure);
    after_size += stats.total_unique_length;
    return CD(stats.total_size) / CD(after_size);
  }
}
// -------------------------------------------------------------------------------------
template <typename CodeType>
inline u32 VDictCompressColumn(const StringArrayViewer src,
                               const BITMAP*,
                               u8* dest,
                               StringStats& stats) {
  // Layout: STRINGS | OFFSETS | CODES
  // -------------------------------------------------------------------------------------
  die_if(stats.distinct_values.size() <= std::numeric_limits<CodeType>::max());
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<VarDictionaryStructure*>(dest);
  col_struct.total_size = stats.total_size;
  vector<str> distinct_values(stats.distinct_values.begin(), stats.distinct_values.end());
  // -------------------------------------------------------------------------------------
  // set is assumed to be implemented as red-black tree with order preserving
  // iterator
  auto dest_slot_ptr = reinterpret_cast<StringArrayViewer::Slot*>(col_struct.data);
  u8* str_write_ptr =
      col_struct.data + ((distinct_values.size() + 1) * sizeof(StringArrayViewer::Slot));
  for (const auto& distinct_str : distinct_values) {
    dest_slot_ptr++->offset =
        str_write_ptr - col_struct.data;  // Note, string offset is relative to the first slot
    std::memcpy(str_write_ptr, distinct_str.data(), distinct_str.length());
    str_write_ptr += distinct_str.length();
  }
  dest_slot_ptr->offset = str_write_ptr - col_struct.data;
  col_struct.codes_offset = str_write_ptr - col_struct.data;
  // -------------------------------------------------------------------------------------
  auto codes_write_ptr = reinterpret_cast<CodeType*>(str_write_ptr);
  // -------------------------------------------------------------------------------------
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    const str& current_value = src(row_i);
    auto it = std::lower_bound(distinct_values.begin(), distinct_values.end(), current_value);
    assert(it != distinct_values.end());
    *codes_write_ptr++ = std::distance(distinct_values.begin(), it);
  }
  return reinterpret_cast<u8*>(codes_write_ptr) - dest;
}
// -------------------------------------------------------------------------------------
template <typename CodeType>
inline u32 VDictGetDecompressedSize(const u8* src, u32) {
  return reinterpret_cast<const VarDictionaryStructure*>(src)->total_size;
}
// -------------------------------------------------------------------------------------
template <typename CodeType>
inline void VDictDecompressColumn(u8* dest,
                                  BitmapWrapper*,
                                  const u8* src,
                                  u32 tuple_count,
                                  u32 level) {
  const auto& col_struct = *reinterpret_cast<const VarDictionaryStructure*>(src);
  StringArrayViewer dict_array(col_struct.data);
  // -------------------------------------------------------------------------------------
  // Prepare output
  auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(dest);
  auto str_write_ptr = dest + sizeof(StringArrayViewer::Slot) * (tuple_count + 1);
  // -------------------------------------------------------------------------------------
  const auto codes = reinterpret_cast<const CodeType*>(col_struct.data + col_struct.codes_offset);
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    auto current_code = codes[row_i];
    auto decoded_str = dict_array(current_code);
    dest_slots[row_i].offset = str_write_ptr - dest;
    std::memcpy(str_write_ptr, decoded_str.data(), decoded_str.length());
    str_write_ptr += decoded_str.length();
  }
  dest_slots[tuple_count].offset = str_write_ptr - dest;
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy
// ------------------------------------------------------------------------------
