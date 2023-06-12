#include "DynamicDictionary.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/DynamicDictionary.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"

// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
using MyDynamicDictionary =
    TDynamicDictionary<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
// -------------------------------------------------------------------------------------
double DynamicDictionary::expectedCompressionRatio(SInteger32Stats& stats,
                                                   u8 allowed_cascading_level) {
  return MyDynamicDictionary::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::compress(const INTEGER* src,
                                const BITMAP* nullmap,
                                u8* dest,
                                SInteger32Stats& stats,
                                u8 allowed_cascading_level) {
  return MyDynamicDictionary::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void DynamicDictionary::decompress(INTEGER* dest,
                                   BitmapWrapper* nullmap,
                                   const u8* src,
                                   u32 tuple_count,
                                   u32 level) {
  return MyDynamicDictionary::decompressColumn(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
INTEGER DynamicDictionary::lookup(u32) {
  UNREACHABLE();
}
void DynamicDictionary::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
string DynamicDictionary::fullDescription(const u8* src) {
  return MyDynamicDictionary::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
