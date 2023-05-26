#include "Uncompressed.hpp"
#include "common/Units.hpp"
#include "compression/schemes/CScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v1::integer {
// -------------------------------------------------------------------------------------
double Uncompressed::expectedCompressionRatio(SInteger32Stats&, u8 allowed_cascading_level) {
  return 1.0;
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::compress(const INTEGER* src,
                           const BITMAP*,
                           u8* dest,
                           SInteger32Stats& stats,
                           u8 allowed_cascading_level) {
  const u32 column_size = stats.total_size;
  std::memcpy(dest, src, column_size);
  return column_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed::decompress(INTEGER* dest,
                              BitmapWrapper*,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  const u32 column_size = tuple_count * sizeof(UINTEGER);
  std::memcpy(dest, src, column_size);
}
// -------------------------------------------------------------------------------------
INTEGER Uncompressed::lookup(u32) {
  UNREACHABLE();
}
void Uncompressed::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
}  // namespace btrblocks::db::v1::integer
// -------------------------------------------------------------------------------------
