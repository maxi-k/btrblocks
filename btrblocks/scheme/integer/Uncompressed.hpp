#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::integers {
// -------------------------------------------------------------------------------------
class Uncompressed : public IntegerScheme {
 public:
  double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const INTEGER* src,
               const BITMAP* nullmap,
               u8* dest,
               SInteger32Stats& stats,
               u8 allowed_cascading_level) override;
  void decompress(INTEGER* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  inline IntegerSchemeType schemeType() override { return staticSchemeType(); }
  inline static IntegerSchemeType staticSchemeType() { return IntegerSchemeType::UNCOMPRESSED; }
  // -------------------------------------------------------------------------------------
  INTEGER lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::integers
// -------------------------------------------------------------------------------------
