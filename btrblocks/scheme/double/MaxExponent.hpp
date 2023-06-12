#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::doubles {
// -------------------------------------------------------------------------------------
struct MaxExponentStructure {
  u32 negatives_bitmap_offset;
  u32 exceptions_offset;
  u8 max_exponent;
  // -------------------------------------------------------------------------------------
  u32 converted_count;
  // -------------------------------------------------------------------------------------
  u8 sd_scheme;
  u8 p_scheme;
  // -------------------------------------------------------------------------------------
  u32 sd_offset;
  u32 p_offset;
  // -------------------------------------------------------------------------------------
  u8 data[];
};
// -------------------------------------------------------------------------------------
class MaxExponent : public DoubleScheme {
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
  inline DoubleSchemeType schemeType() override { return staticSchemeType(); }
  inline static DoubleSchemeType staticSchemeType() { return DoubleSchemeType::PSEUDODECIMAL; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::doubles
// -------------------------------------------------------------------------------------
