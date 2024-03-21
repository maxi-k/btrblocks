#include "RLE.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemePool.hpp"
#include "scheme/templated/RLE.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
using MyRLE = TRLE<INT64, Int64Scheme, SInt64Stats, Int64SchemeType>;
// -------------------------------------------------------------------------------------
double RLE::expectedCompressionRatio(SInt64Stats& stats, u8 allowed_cascading_level) {
  if (stats.average_run_length < SchemeConfig::get().integers.rle_run_length_threshold) {
    return 0;
  }
  return Int64Scheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 RLE::compress(const INT64* src,
                  const BITMAP* nullmap,
                  u8* dest,
                  SInt64Stats& stats,
                  u8 allowed_cascading_level) {
  auto& cfg = SchemeConfig::get().integers;
  return MyRLE::compressColumn(src, nullmap, dest, stats, allowed_cascading_level,
                               CB(cfg.rle_force_values_scheme), CB(cfg.rle_force_counts_scheme));
}
// -------------------------------------------------------------------------------------
void RLE::decompress(INT64* dest,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level) {
  return MyRLE::decompressColumn(dest, nullmap, src, tuple_count, level);
}
u32 RLE::decompressRuns(INT64* values,
                        INTEGER* counts,
                        BitmapWrapper* nullmap,
                        const u8* src,
                        u32 tuple_count,
                        u32 level) {
  return MyRLE::decompressRuns(values, counts, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
INT64 RLE::lookup(u32) {
  UNREACHABLE();
}
void RLE::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}

std::string RLE::fullDescription(const u8* src) {
  return MyRLE::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::int64s
// -------------------------------------------------------------------------------------
