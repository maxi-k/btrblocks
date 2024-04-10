#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
struct XPBP64Structure {  // need to be aligned by 4 because of
                        // FastPFor encodeArray
  u32 fastpfor_count;
  u8 padding;
  u8 encoding_scheme_padded;
  u32 padded_values_offset;
  u8 data[];
};
struct XFBP64Structure {  // need to be aligned by 4 because of
                        // FastPFor encodeArray
  u32 u32_count;        // number of 4 bytes written by FastPFor
  u8 padding;
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
