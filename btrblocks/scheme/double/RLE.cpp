// -------------------------------------------------------------------------------------
#include "RLE.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/templated/RLE.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
using MyRLE = TRLE<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>;
// -------------------------------------------------------------------------------------
double RLE::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  if (stats.average_run_length < 2) {
    return 0;
  }
  return DoubleScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 RLE::compress(const DOUBLE* src,
                  const BITMAP* nullmap,
                  u8* dest,
                  DoubleStats& stats,
                  u8 allowed_cascading_level) {
  auto& cfg = SchemeConfig::get();
  return MyRLE::compressColumn(src, nullmap, dest, stats, allowed_cascading_level,
                               CB(cfg.doubles.rle_force_values_scheme),
                               CB(cfg.doubles.rle_force_counts_scheme));
}
// -------------------------------------------------------------------------------------
void RLE::decompress(DOUBLE* dest,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level) {
  return MyRLE::decompressColumn(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
string RLE::fullDescription(const u8* src) {
  return MyRLE::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
