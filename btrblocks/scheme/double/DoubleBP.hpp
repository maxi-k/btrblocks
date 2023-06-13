#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
struct DoubleBPStructure {
  u32 u32_count;  // number of 4 bytes written by FastBP
  u8 padding;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class DoubleBP : public DoubleScheme {
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
  inline static DoubleSchemeType staticSchemeType() { return DoubleSchemeType::DOUBLE_BP; }
  DOUBLE lookup(u32);
  void scan(Predicate, BITMAP*, const u8*, u32);
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
