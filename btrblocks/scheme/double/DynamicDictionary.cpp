#include "DynamicDictionary.hpp"
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/DynamicDictionary.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------+------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
#include <iomanip>
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
using MyDynamicDictionary = TDynamicDictionary<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>;
// -------------------------------------------------------------------------------------
double DynamicDictionary::expectedCompressionRatio(btrblocks::DoubleStats& stats,
                                                   u8 allowed_cascading_level) {
  return MyDynamicDictionary::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::compress(const DOUBLE* src,
                                const BITMAP* nullmap,
                                u8* dest,
                                DoubleStats& stats,
                                u8 allowed_cascading_level) {
  return MyDynamicDictionary::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void DynamicDictionary::decompress(DOUBLE* dest,
                                   BitmapWrapper* nullmap,
                                   const u8* src,
                                   u32 tuple_count,
                                   u32 level) {
  return MyDynamicDictionary::decompressColumn(dest, nullmap, src, tuple_count, level);
}

string DynamicDictionary::fullDescription(const u8* src) {
  return MyDynamicDictionary::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
