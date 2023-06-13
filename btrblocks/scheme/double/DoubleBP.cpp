#include "DoubleBP.hpp"
#include "common/Exceptions.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "extern/FastPFOR.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
u32 DoubleBP::compress(const DOUBLE* src,
                       const BITMAP*,
                       u8* dest,
                       DoubleStats& stats,
                       [[maybe_unused]] u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<DoubleBPStructure*>(dest);
  // -------------------------------------------------------------------------------------
  FBPImpl codec;
  // -------------------------------------------------------------------------------------
  auto dest_integer = reinterpret_cast<u64>(col_struct.data);
  u64 padding = dest_integer;
  dest_integer = (dest_integer + 3) & ~3ul;
  col_struct.padding = dest_integer - padding;
  auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);
  // -------------------------------------------------------------------------------------
  size_t compressed_codes_size =
      4 * stats.tuple_count + 1024;  // give FBP a large enough output buffer
  // 2x tuple count because we're actually compressing doubles
  codec.compress(reinterpret_cast<const u32*>(src), stats.tuple_count * 2, dest_4_aligned,
                 compressed_codes_size);
  col_struct.u32_count = compressed_codes_size;
  // -------------------------------------------------------------------------------------
  return sizeof(DoubleBPStructure) + compressed_codes_size * sizeof(u32);
}
// -------------------------------------------------------------------------------------
void DoubleBP::decompress(DOUBLE* dest,
                          [[maybe_unused]] BitmapWrapper*,
                          const u8* src,
                          u32 tuple_count,
                          [[maybe_unused]] u32 level) {
  auto& col_struct = *reinterpret_cast<const DoubleBPStructure*>(src);
  // -------------------------------------------------------------------------------------
  FBPImpl codec;
  SIZE decompressed_codes_size;
  auto encoded_array =
      const_cast<u32*>(reinterpret_cast<const u32*>(col_struct.data + col_struct.padding));
  const u32* dst_ptr = codec.decompress(encoded_array, col_struct.u32_count,
                                        reinterpret_cast<u32*>(dest), decompressed_codes_size);
  if (dst_ptr != encoded_array + col_struct.u32_count) {
    throw Generic_Exception("Decompressing DoubleBP failed");
  }
}
// -------------------------------------------------------------------------------------
DOUBLE DoubleBP::lookup(u32) {
  UNREACHABLE();
};
// -------------------------------------------------------------------------------------
void DoubleBP::scan(Predicate, BITMAP*, const u8*, u32) {
  UNREACHABLE();
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
