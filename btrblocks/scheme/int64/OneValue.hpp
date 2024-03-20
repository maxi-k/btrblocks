#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::int64s {
// -------------------------------------------------------------------------------------
struct OneValueStructure {
  UINTEGER one_value;
};
// -------------------------------------------------------------------------------------
class OneValue : public Int64Scheme {
 public:
  double expectedCompressionRatio(SInt64Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const INT64* src,
               const BITMAP* nullmap,
               u8* dest,
               SInt64Stats& stats,
               u8 allowed_cascading_level) override;
  void decompress(INT64* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  inline Int64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::ONE_VALUE; }
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::int64s
// -------------------------------------------------------------------------------------
