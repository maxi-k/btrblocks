#include "Uncompressed.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::int64s {
// -------------------------------------------------------------------------------------
double Uncompressed::expectedCompressionRatio(SInt64Stats&, u8 allowed_cascading_level) {
  return 1.0;
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::compress(const INT64* src,
                           const BITMAP*,
                           u8* dest,
                           SInt64Stats& stats,
                           u8 allowed_cascading_level) {
  const u32 column_size = stats.total_size;
  std::memcpy(dest, src, column_size);
  return column_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed::decompress(INT64* dest,
                              BitmapWrapper*,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  const u32 column_size = tuple_count * sizeof(INT64);
  std::memcpy(dest, src, column_size);
}
// -------------------------------------------------------------------------------------
INT64 Uncompressed::lookup(u32) {
  UNREACHABLE();
}
void Uncompressed::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
}  // namespace btrblocks::legacy::int64s
// -------------------------------------------------------------------------------------
