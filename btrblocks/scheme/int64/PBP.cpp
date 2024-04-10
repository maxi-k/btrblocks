#include "PBP.hpp"
#include "common/Units.hpp"
#include "common/Utils.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
#include "compression/SchemePicker.hpp"
#include "extern/FastPFOR.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
// whether to use the compression ratio estimation for FBP
constexpr bool auto_fpb = true;
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
u32 PBP::compress(const INT64* src, const BITMAP*, u8* dest, SInt64Stats& stats, u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<XPBP64Structure*>(dest);
  // -------------------------------------------------------------------------------------
  FPFor64Impl fast_pfor;
  size_t compressed_codes_size;  // not really used
  // -------------------------------------------------------------------------------------
  size_t fast_pfor_compressed_count = stats.tuple_count & ~127ul;
  auto write_ptr = reinterpret_cast<u8*>(col_struct.data);
  // -------------------------------------------------------------------------------------
  // TODO: FastPFOR wants multiple of 128/256 of numbers -> so what to do with the rest?
  //       1. just store them without further compression
  //       2. compress them further -> but i dont think this would do a lot
  if (fast_pfor_compressed_count > 0) {
    auto dest_integer = reinterpret_cast<intptr_t>(write_ptr);
    u64 padding;
    dest_integer = Utils::alignBy(dest_integer, 16, padding);
    col_struct.padding = padding;
    auto dest_4_aligned = reinterpret_cast<u32*>(dest_integer);

    fast_pfor.compress(reinterpret_cast<const FPFor64Impl::data_t*>(src), fast_pfor_compressed_count,
                       reinterpret_cast<u32*>(dest_4_aligned),
                       compressed_codes_size);
    write_ptr = reinterpret_cast<u8*>(dest_4_aligned) + compressed_codes_size * sizeof(INT64);
    col_struct.fastpfor_count = fast_pfor_compressed_count;
  }
  // -------------------------------------------------------------------------------------
  // store the padded rest of the data
  if ((stats.tuple_count & 255) > 0 && false) {
    u32 used_space;
    col_struct.padded_values_offset = write_ptr - dest;

    Int64SchemePicker::compress(
        src + fast_pfor_compressed_count, nullptr, write_ptr, stats.tuple_count & 127, allowed_cascading_level - 1,
        used_space, col_struct.encoding_scheme_padded, autoScheme(), "padded numbers");

    write_ptr += used_space;
  }

  // -------------------------------------------------------------------------------------
  return dest - write_ptr;
}
// -------------------------------------------------------------------------------------
void PBP::decompress(INT64* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XPBP64Structure*>(src);
  // -------------------------------------------------------------------------------------
  FPFor64Impl fast_pfor;
  SIZE decompressed_codes_size = tuple_count; // 2x tuple count because we actually compressed longs
  if (col_struct.fastpfor_count) {
    auto encoded_array =
        reinterpret_cast<const FPFor64Impl::data_t*>(col_struct.data + col_struct.padding);
    if (fast_pfor.decompress(reinterpret_cast<const u32*>(encoded_array), col_struct.fastpfor_count,
                             reinterpret_cast<FPFor64Impl::data_t*>(dest),
                             decompressed_codes_size) != reinterpret_cast<const u32*>(encoded_array + col_struct.fastpfor_count)) {
      throw Generic_Exception("Decompressing XPBP failed");
    }
  }

  if (col_struct.fastpfor_count < tuple_count) {
    auto padded_encoded_array =
        reinterpret_cast<const FPFor64Impl::data_t*>(col_struct.data + col_struct.padding) + col_struct.fastpfor_count;
    Int64Scheme& padded_scheme =
        Int64SchemePicker::MyTypeWrapper::getScheme(col_struct.encoding_scheme_padded);
    padded_scheme.decompress((INT64*)padded_encoded_array, nullptr, col_struct.data + col_struct.padded_values_offset,
                                          tuple_count - col_struct.fastpfor_count, level + 1);
  }
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
  auto& col_struct = *reinterpret_cast<XFBP64Structure*>(dest);
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
  return sizeof(XFBP64Structure) + compressed_codes_size * sizeof(u32);
}
// -------------------------------------------------------------------------------------
void FBP::decompress(INT64* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const XFBP64Structure*>(src);
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
