// -------------------------------------------------------------------------------------
#include "Uncompressed.hpp"
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/CScheme.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v1::d {
// -------------------------------------------------------------------------------------
double Uncompressed::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  return 1.0;
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::compress(const DOUBLE* src,
                           const BITMAP* nullmap,
                           u8* dest,
                           DoubleStats& stats,
                           u8 allowed_cascading_level) {
  std::memcpy(dest, src, stats.total_size);
  return stats.total_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed::decompress(DOUBLE* dest,
                              BitmapWrapper*,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  std::memcpy(dest, src, tuple_count * sizeof(DOUBLE));
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v1::d
// -------------------------------------------------------------------------------------
