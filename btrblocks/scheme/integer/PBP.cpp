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
namespace btrblocks::integers {
// -------------------------------------------------------------------------------------
u32 PBP::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  size_t compressed_codes_size = stats.tuple_count + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding;
  dest_integer = Utils::alignBy(dest_integer, 16, padding);
  col_struct.padding = padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  fast_pfor.compress(reinterpret_cast<const u32*>(src), stats.tuple_count, dest_4_aligned,
                     compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32) + 16 /*For padding */;
}
// -------------------------------------------------------------------------------------
void PBP::decompress(INTEGER* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  SIZE decompressed_codes_size = tuple_count;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (fast_pfor.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                           decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP failed");
  }
  assert(decompressed_codes_size == tuple_count);
}
// -------------------------------------------------------------------------------------
void PBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INTEGER PBP::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
// DELTA PBP
// -------------------------------------------------------------------------------------
double PBP_DELTA::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if (!stats.is_sorted) {
    return 0;
  }
  return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
}
// -------------------------------------------------------------------------------------
u32 PBP_DELTA::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  auto src_biased = std::unique_ptr<u32[]>(new u32[stats.tuple_count]);
  std::memcpy(src_biased.get(), src, stats.tuple_count * sizeof(u32));
  // TODO we may be able to get rid of this extra array and write directly to
  // dest
  FPFor::applyDelta(src_biased.get(), stats.tuple_count);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  size_t compressed_codes_size = stats.tuple_count + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding = dest_integer;
  dest_integer = (dest_integer + 3) & ~3ul;
  col_struct.padding = dest_integer - padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  fast_pfor.compress(reinterpret_cast<const u32*>(src_biased.get()), stats.tuple_count,
                     dest_4_aligned, compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32);
}
// -------------------------------------------------------------------------------------
void PBP_DELTA::decompress(INTEGER* dest,
                           BitmapWrapper*,
                           const u8* src,
                           u32 tuple_count,
                           u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FPFor codec;
  SIZE decompressed_codes_size = tuple_count;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (codec.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                       decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP Delta failed");
  }
  assert(decompressed_codes_size == tuple_count);
  FPFor::revertDelta(reinterpret_cast<u32*>(dest), decompressed_codes_size);
}
// -------------------------------------------------------------------------------------
void PBP_DELTA::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INTEGER PBP_DELTA::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
double FBP::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  if constexpr (auto_fpb) {
    return IntegerScheme::expectedCompressionRatio(stats, allowed_cascading_level);
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 FBP::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FBPImpl fast_pfor;
  size_t compressed_codes_size = stats.tuple_count + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding = dest_integer;
  dest_integer = (dest_integer + 3) & ~3ul;
  col_struct.padding = dest_integer - padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  fast_pfor.compress(reinterpret_cast<const u32*>(src), stats.tuple_count, dest_4_aligned,
                     compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32);
}
// -------------------------------------------------------------------------------------
void FBP::decompress(INTEGER* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FBPImpl codec;
  SIZE decompressed_codes_size = tuple_count;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (codec.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                       decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP failed");
  }
  assert(decompressed_codes_size == tuple_count);
}
// -------------------------------------------------------------------------------------
void FBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INTEGER FBP::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
double EXP_FBP::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  u32 b = Utils::getBitsNeeded(stats.max);
  u32 integers_in_block = 64 / b;
  u32 bytes = 8 * std::ceil(stats.tuple_count / integers_in_block);
  return 4 + bytes;
}
// -------------------------------------------------------------------------------------
u32 EXP_FBP::compress(const INTEGER* src, const BITMAP*, u8* dest, SInteger32Stats& stats, u8) {
  u32 b = Utils::getBitsNeeded(stats.max);
  u32 integers_in_block = 64 / b;
  u32 bytes = 8 * std::ceil(stats.tuple_count / integers_in_block);
  return 4 + bytes;
}
// -------------------------------------------------------------------------------------
void EXP_FBP::decompress(INTEGER* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
void EXP_FBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
}
INTEGER EXP_FBP::lookup(u32) {
  UNREACHABLE();
}
// -------------------------------------------------------------------------------------
u32 FBP64::compress(u64* src, u8* dest, u32 tuple_count) {
  auto& col_struct = *reinterpret_cast<XPBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FPFor fast_pfor;
  size_t compressed_codes_size = tuple_count * 2 + 1024;  // not really used
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding = dest_integer;
  dest_integer = (dest_integer + 3) & ~3ul;
  col_struct.padding = dest_integer - padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  fast_pfor.compress(reinterpret_cast<const u32*>(src), tuple_count, dest_4_aligned,
                     compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(XPBPStructure) + compressed_codes_size * sizeof(u32);
}
void FBP64::decompress(u8* dest, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FPFor codec;

  SIZE decompressed_codes_size = tuple_count;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  if (codec.decompress(encoded_array, col_struct.u32_count, reinterpret_cast<u32*>(dest),
                       decompressed_codes_size) != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing XPBP failed");
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::integers
// -------------------------------------------------------------------------------------
