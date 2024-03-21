#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
struct RLEStructure {
  u32 runs_count;
  u32 runs_count_offset;
  u8 values_scheme_code;
  u8 counts_scheme_code;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class RLE : public Int64Scheme {
 public:
  double expectedCompressionRatio(SInt64Stats& stats, u8 allowed_cascading_level) override;
  u32 compress(const INT64* src,
               const BITMAP* nullmap,
               u8* dest,
               SInt64Stats& stats,
               u8 allowed_cascading_level) override;
  u32 decompressRuns(INT64* values,
                     INTEGER* counts,
                     BitmapWrapper* nullmap,
                     const u8* src,
                     u32 tuple_count,
                     u32 level);
  void decompress(INT64* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  std::string fullDescription(const u8* src) override;
  inline Int64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::RLE; }
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::int64s
// -------------------------------------------------------------------------------------
