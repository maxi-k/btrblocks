#include "Frequency.hpp"
#include "common/Units.hpp"
#include "compression/schemes/CScheme.hpp"
#include "compression/schemes/CSchemePicker.hpp"
#include "compression/schemes/v2/templated/Frequency.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
DECLARE_uint32(frequency_threshold_pct);
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v2::integer {
// -------------------------------------------------------------------------------------
using MyFrequency = TFrequency<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
// -------------------------------------------------------------------------------------
double Frequency::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (CD(stats.unique_count) * 100.0 / CD(stats.tuple_count) > FLAGS_frequency_threshold_pct) {
    return 0;
  }
  return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 Frequency::compress(const INTEGER* src,
                        const BITMAP* nullmap,
                        u8* dest,
                        SInteger32Stats& stats,
                        u8 allowed_cascading_level) {
  return MyFrequency::compressColumn(src, nullmap, dest, stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
void Frequency::decompress(INTEGER* dest,
                           BitmapWrapper* nullmap,
                           const u8* src,
                           u32 tuple_count,
                           u32 level) {
  return MyFrequency::decompressColumn(dest, nullmap, src, tuple_count, level);
}
// -------------------------------------------------------------------------------------
INTEGER Frequency::lookup(u32) {
  UNREACHABLE();
}
void Frequency::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}

std::string Frequency::fullDescription(const u8* src) {
  return MyFrequency::fullDescription(src, this->selfDescription());
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v2::integer
// -------------------------------------------------------------------------------------
