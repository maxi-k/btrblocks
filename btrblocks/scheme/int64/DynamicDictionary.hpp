#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::int64s {
// -------------------------------------------------------------------------------------
struct DynamicDictionaryStructure {
  u8 codes_scheme_code;
  u32 codes_offset;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class DynamicDictionary : public Int64Scheme {
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
  std::string fullDescription(const u8* src) override;
  inline Int64SchemeType schemeType() override { return staticSchemeType(); }
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::DICT; }
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
};
// -------------------------------------------------------------------------------------
} // namespace btrblocks::int64s
// -------------------------------------------------------------------------------------
