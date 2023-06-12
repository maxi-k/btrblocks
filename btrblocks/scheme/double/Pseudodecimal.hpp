#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
#include <roaring/roaring.hh>
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
const u32 block_size = 4;  // Block size adjusted for AVX2
const u8 do_iteration = (1 << 0);
const u8 do_unroll = (1 << 1);
// -------------------------------------------------------------------------------------
struct DecimalStructure {
  struct __attribute__((packed)) Slot {
    u8 d1 : 4;
    u8 d2 : 4;
  };
  static_assert(sizeof(Slot) == 1);
  // -------------------------------------------------------------------------------------
  u32 converted_count;
  // -------------------------------------------------------------------------------------
  u8 numbers_scheme;
  u8 exponents_scheme;
  u8 patches_scheme;
  u8 variant_selector;
  // -------------------------------------------------------------------------------------
  u32 exponents_offset;
  u32 patches_offset;
  u32 exceptions_map_offset;
  // -------------------------------------------------------------------------------------
  u8 data[];
};
// -------------------------------------------------------------------------------------
class Decimal : public DoubleScheme {
 public:
  u32 compress(const DOUBLE* src,
               const BITMAP* nullmap,
               u8* dest,
               DoubleStats& stats,
               u8 allowed_cascading_level) override;
  void decompress(DOUBLE* dest,
                  BitmapWrapper* bitmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  bool isUsable(DoubleStats& stats) override;
  std::string fullDescription(const u8* src) override;
  inline DoubleSchemeType schemeType() override { return staticSchemeType(); }
  inline static DoubleSchemeType staticSchemeType() { return DoubleSchemeType::PSEUDODECIMAL; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
