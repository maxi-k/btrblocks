#include "FOR.hpp"
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"

// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
double FOR::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  return 0;
}
// -------------------------------------------------------------------------------------
u32 FOR::compress(const INTEGER* src,
                  const BITMAP* nullmap,
                  u8* dest,
                  SInteger32Stats& stats,
                  u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<FORStructure*>(dest);
  vector<INTEGER> biased_output;
  // -------------------------------------------------------------------------------------
  col_struct.bias = stats.min;
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    if (nullmap == nullptr || nullmap[row_i]) {
      biased_output.push_back(src[row_i] - col_struct.bias);
    } else {
      biased_output.push_back(src[row_i]);
    }
  }
  // -------------------------------------------------------------------------------------
  // Next Level compression
  auto write_ptr = col_struct.data;
  u32 used_space;
  IntegerSchemePicker::compress(biased_output.data(), nullmap, write_ptr, biased_output.size(),
                                allowed_cascading_level - 1, used_space, col_struct.next_scheme,
                                autoScheme(), "for_next_level");
  write_ptr += used_space;
  // -------------------------------------------------------------------------------------
  return write_ptr - dest;
}
// -------------------------------------------------------------------------------------
void FOR::decompress(INTEGER* dest,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level) {
  const auto& col_struct = *reinterpret_cast<const FORStructure*>(src);
  // -------------------------------------------------------------------------------------
  IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.next_scheme)
      .decompress(dest, nullmap, col_struct.data, tuple_count, level + 1);
  // -------------------------------------------------------------------------------------
  if (nullmap != nullptr && nullmap->type() == BitmapType::ALLZEROS) {
    // Everything is null, no point in writing anything
    return;
  }

  // In any case we add the bias. The result for null values does not matter
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    dest[row_i] += col_struct.bias;
  }
}
// -------------------------------------------------------------------------------------
INTEGER FOR::lookup(u32) {
  UNREACHABLE();
}
void FOR::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}

std::string FOR::fullDescription(const u8* src) {
  const auto& col_struct = *reinterpret_cast<const FORStructure*>(src);
  auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.next_scheme);
  return this->selfDescription() + " -> ([int] biased) " + scheme.fullDescription(col_struct.data);
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
