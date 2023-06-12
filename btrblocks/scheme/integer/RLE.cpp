#include "RLE.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemePool.hpp"
#include "scheme/templated/RLE.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
using MyRLE = TRLE<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
// -------------------------------------------------------------------------------------
double RLE::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (stats.average_run_length < SchemeConfig::get().integers.rle_run_length_threshold) {
    return 0;
  }
  return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 RLE::compress(const INTEGER* src,
                  const BITMAP* nullmap,
                  u8* dest,
                  SInteger32Stats& stats,
                  u8 allowed_cascading_level) {
  auto& cfg = SchemeConfig::get().integers;
  return MyRLE::compressColumn(src, nullmap, dest, stats, allowed_cascading_level,
                               CB(cfg.rle_force_values_scheme), CB(cfg.rle_force_counts_scheme));
}
// -------------------------------------------------------------------------------------
void RLE::decompress(INTEGER* dest,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level) {
  return MyRLE::decompressColumn(dest, nullmap, src, tuple_count, level);
}
u32 RLE::decompressRuns(INTEGER* values,
                        INTEGER* counts,
                        BitmapWrapper* nullmap,
                        const u8* src,
                        u32 tuple_count,
                        u32 level) {
  return MyRLE::decompressRuns(values, counts, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
INTEGER RLE::lookup(u32) {
  UNREACHABLE();
}
void RLE::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}

std::string RLE::fullDescription(const u8* src) {
  return MyRLE::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
