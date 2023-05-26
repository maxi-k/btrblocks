// -------------------------------------------------------------------------------------
#include "RLE.hpp"
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "compression/schemes/CScheme.hpp"
#include "compression/schemes/CSchemePicker.hpp"
#include "compression/schemes/v2/templated/RLE.hpp"
// -------------------------------------------------------+------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
DEFINE_uint32(d_rle_force_values_scheme, btrblocks::AUTO_SCHEME, "");
DEFINE_uint32(d_rle_force_counts_scheme, btrblocks::AUTO_SCHEME, "");
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v2::d {
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
  return MyRLE::compressColumn(src, nullmap, dest, stats, allowed_cascading_level,
                               FLAGS_d_rle_force_values_scheme, FLAGS_d_rle_force_counts_scheme);
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
}  // namespace btrblocks::db::v2::d
// -------------------------------------------------------------------------------------
