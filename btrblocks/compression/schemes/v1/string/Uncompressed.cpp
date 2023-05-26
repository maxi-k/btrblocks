#include "Uncompressed.hpp"
#include "common/Units.hpp"
#include "compression/schemes/CScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v1::string {
// -------------------------------------------------------------------------------------
double Uncompressed::expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) {
  return 1.0;
  // return static_cast<double>(stats.total_length) /
  // static_cast<double>(stats.total_length + sizeof(u32));// actually it is
  // just 1 !
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::compress(const StringArrayViewer src,
                           const BITMAP*,
                           u8* dest,
                           StringStats& stats) {
  auto& col_struct = *reinterpret_cast<UncompressedStructure*>(dest);
  col_struct.total_size = stats.total_size;
  std::memcpy(col_struct.data, src.slots_ptr, stats.total_size);
  return stats.total_size + sizeof(UncompressedStructure);
}
// -------------------------------------------------------------------------------------
u32 Uncompressed::getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  return reinterpret_cast<const UncompressedStructure*>(src)->total_size;
}
// -------------------------------------------------------------------------------------
void Uncompressed::decompress(u8* dest,
                              BitmapWrapper* nullmap,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  auto& col_struct = *reinterpret_cast<const UncompressedStructure*>(src);
  std::memcpy(dest, col_struct.data, col_struct.total_size);
}

u32 Uncompressed::getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  auto& col_struct = *reinterpret_cast<const UncompressedStructure*>(src);
  return col_struct.total_size - ((tuple_count + 1) * sizeof(StringArrayViewer::Slot));
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v1::string
// -------------------------------------------------------------------------------------
