#include "PBP.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
#include "extern/FastPFOR.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
// whether to use the compression ratio estimation for FBP
constexpr bool auto_fpb = true;
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
u32 PBP::compress(const INT64* src, const BITMAP*, u8* dest, SInt64Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  size_t compressed_codes_size = 4 * stats.tuple_count + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding;
  dest_integer = Utils::alignBy(dest_integer, 16, padding);
  col_struct.padding = padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  // 2x tuple count because we're actually compressing longs
  fast_pfor.compress(reinterpret_cast<const u32*>(src), stats.tuple_count * 2, dest_4_aligned,
                     compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32) + 16 /*For padding */;
}
// -------------------------------------------------------------------------------------
void PBP::decompress(INT64* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  SIZE decompressed_codes_size = tuple_count * 2; // 2x tuple count because we actually compressed longs
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (fast_pfor.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                           decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP failed");
  }
  assert(decompressed_codes_size == tuple_count*2);
}
// -------------------------------------------------------------------------------------
void PBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INT64 PBP::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
double FBP::expectedCompressionRatio(SInt64Stats& stats, u8 allowed_cascading_level) {
  if constexpr (auto_fpb) {
    return Int64Scheme::expectedCompressionRatio(stats, allowed_cascading_level);
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 FBP::compress(const INT64* src, const BITMAP*, u8* dest, SInt64Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FBPImpl fast_pfor;
  size_t compressed_codes_size = 4 * stats.tuple_count + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding = dest_integer;
  dest_integer = (dest_integer + 3) & ~3ul;
  col_struct.padding = dest_integer - padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  // 2x tuple count because we're actually compressing longs
  fast_pfor.compress(reinterpret_cast<const u32*>(src), 2* stats.tuple_count, dest_4_aligned,
                     compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32);
}
// -------------------------------------------------------------------------------------
void FBP::decompress(INT64* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FBPImpl codec;
  SIZE decompressed_codes_size = tuple_count*2;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (codec.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                       decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP failed");
  }
  assert(decompressed_codes_size == tuple_count*2);
}
// -------------------------------------------------------------------------------------
void FBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INT64 FBP::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::int64s
// -------------------------------------------------------------------------------------
