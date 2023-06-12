#include "Frequency.hpp"
// ------------------------------------------------------------------------------
#include "btrblocks.hpp"
// ------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/Frequency.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------+------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
#include <iomanip>
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::doubles {
// -------------------------------------------------------------------------------------
using MyFrequency = TFrequency<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>;
// -------------------------------------------------------------------------------------
double Frequency::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  if (CD(stats.unique_count) * 100.0 / CD(stats.tuple_count) >
      SchemeConfig::get().doubles.frequency_threshold_pct) {
    return 0;
  }
  return DoubleScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 Frequency::compress(const DOUBLE* src,
                        const BITMAP* nullmap,
                        u8* dest,
                        DoubleStats& stats,
                        u8 allowed_cascading_level) {
  return MyFrequency::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void Frequency::decompress(DOUBLE* dest,
                           BitmapWrapper* nullmap,
                           const u8* src,
                           u32 tuple_count,
                           u32 level) {
  return MyFrequency::decompressColumn(dest, nullmap, src, tuple_count, level);
}

string Frequency::fullDescription(const u8* src) {
  return MyFrequency::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::doubles
// -------------------------------------------------------------------------------------
