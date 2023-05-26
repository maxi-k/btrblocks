#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "Dictionary.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/CScheme.hpp"
#include "compression/schemes/v1/templated/VarDictionary.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v1::string {
// -------------------------------------------------------------------------------------
double Dictionary8::expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) {
  return VDictExpectedCompressionRatio<u8>(stats);
}
// -------------------------------------------------------------------------------------
u32 Dictionary8::compress(const StringArrayViewer src,
                          const BITMAP* nullmap,
                          u8* dest,
                          StringStats& stats) {
  return VDictCompressColumn<u8>(src, nullmap, dest, stats);
}
// -------------------------------------------------------------------------------------
u32 Dictionary8::getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  return VDictGetDecompressedSize<u8>(src, tuple_count);
}
// -------------------------------------------------------------------------------------
void Dictionary8::decompress(u8* dest,
                             BitmapWrapper* nullmap,
                             const u8* src,
                             u32 tuple_count,
                             u32 level) {
  return VDictDecompressColumn<u8>(dest, nullmap, src, tuple_count, level);
}

u32 Dictionary8::getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  throw Generic_Exception("not implemented");
}

// -------------------------------------------------------------------------------------
// 16 bits codes
// -------------------------------------------------------------------------------------
double Dictionary16::expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) {
  return VDictExpectedCompressionRatio<u16>(stats);
}
// -------------------------------------------------------------------------------------
u32 Dictionary16::compress(const StringArrayViewer src,
                           const BITMAP* nullmap,
                           u8* dest,
                           StringStats& stats) {
  return VDictCompressColumn<u16>(src, nullmap, dest, stats);
}
// -------------------------------------------------------------------------------------
u32 Dictionary16::getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  return VDictGetDecompressedSize<u16>(src, tuple_count);
}
// -------------------------------------------------------------------------------------
void Dictionary16::decompress(u8* dest,
                              BitmapWrapper* nullmap,
                              const u8* src,
                              u32 tuple_count,
                              u32 level) {
  return VDictDecompressColumn<u16>(dest, nullmap, src, tuple_count, level);
}

u32 Dictionary16::getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  throw Generic_Exception("not implemented");
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v1::string
// -------------------------------------------------------------------------------------
