// -------------------------------------------------------------------------------------
#include "OneValue.hpp"
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::doubles {
// -------------------------------------------------------------------------------------
double OneValue::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue::compress(const DOUBLE* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       DoubleStats& stats,
                       u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<OneValueStructure*>(dest);
  if (src != nullptr) {
    col_struct.one_value = stats.distinct_values.begin()->first;
  } else {
    col_struct.one_value = NULL_CODE;
  }
  return sizeof(DOUBLE);
}
// -------------------------------------------------------------------------------------
void OneValue::decompress(DOUBLE* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  const auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {  // can be further optimized probably
    dest[row_i] = col_struct.one_value;
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::doubles
// -------------------------------------------------------------------------------------
