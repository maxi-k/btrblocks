#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
struct XPBP64Structure {
  u64 padding = 0; // alignment is necessary for fastpfor
  u32 fastpfor_count = 0;
  u32 padded_values_offset = 0;
  u8 encoding_scheme_padded = 0;
  u8 data[];
};
struct XFBP64Structure {
  u32 u32_count;
  u8 padding; // alignment is necessary for fastpfor
  u8 data[];
};
// -------------------------------------------------------------------------------------
class PBP : public Int64Scheme {
 public:
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
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::PFOR; }
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
class FBP : public Int64Scheme {
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
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::BP; }
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::int64s
// -------------------------------------------------------------------------------------
