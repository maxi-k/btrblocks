#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::int64s {
// -------------------------------------------------------------------------------------
class Truncation16 : public Int64Scheme {
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
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::TRUNCATION_16; }
  // -------------------------------------------------------------------------------------
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
  virtual bool canCompress(SInt64Stats& stats) {
    return stats.max - stats.min <= std::numeric_limits<u16>::max();
  }
};
// -------------------------------------------------------------------------------------
class Truncation8 : public Int64Scheme {
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
  inline static Int64SchemeType staticSchemeType() { return Int64SchemeType::TRUNCATION_8; }
  // -------------------------------------------------------------------------------------
  INT64 lookup(u32) override;
  void scan(Predicate, BITMAP*, const u8*, u32) override;
  virtual bool canCompress(SInt64Stats& stats) {
    return stats.max - stats.min <= std::numeric_limits<u8>::max();
  }
};
// -------------------------------------------------------------------------------------
template <typename CodeType>
struct TruncationStructure {
  INT64 base;
  CodeType truncated_values[];
};
// -------------------------------------------------------------------------------------
template <typename CodeType>
double ITruncExpectedCF(btrblocks::SInt64Stats& stats) {
  if (stats.max - stats.min <= (std::numeric_limits<CodeType>::max())) {
    return sizeof(INT64) / sizeof(CodeType);
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
template <typename CodeType>
double ITruncCompress(const INT64* src, const BITMAP* nullmap, u8* dest, SInt64Stats& stats) {
  die_if(stats.max - stats.min <= std::numeric_limits<CodeType>::max());
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<TruncationStructure<CodeType>*>(dest);
  // -------------------------------------------------------------------------------------
  // Set the base
  col_struct.base = stats.min;
  // -------------------------------------------------------------------------------------
  // Truncate each integer
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    if (nullmap == nullptr || nullmap[row_i]) {
      auto biased_value = static_cast<CodeType>(src[row_i] - col_struct.base);
      col_struct.truncated_values[row_i] = biased_value;
    }
  }
  // -------------------------------------------------------------------------------------
  return sizeof(TruncationStructure<CodeType>) + (sizeof(CodeType) * stats.tuple_count);
}
// -------------------------------------------------------------------------------------
template <typename CodeType>
void ITruncDecompress(INT64* dest,
                      BitmapWrapper* nullmap,
                      const u8* src,
                      u32 tuple_count,
                      u32 level) {
  /* As of now Truncation is unused and this part is commented out for simpler
   * refactoring */
  const auto& col_struct = *reinterpret_cast<const TruncationStructure<CodeType>*>(src);
  UNREACHABLE()
  //   //
  //   -------------------------------------------------------------------------------------
  //   if (nullmap == nullptr || nullmap->type() == BitmapType::ALLONES) {
  //       for (u32 row_i = 0; row_i < tuple_count; row_i++) {
  //           dest[row_i] = col_struct.base +
  //           col_struct.truncated_values[row_i];
  //       }
  //   } else if(nullmap->type() == BitmapType::ALLZEROS) {
  //       return;
  //   } else {
  //       for (u32 row_i = 0; row_i < tuple_count; row_i++) {
  //           if (nullmap->test(row_i)) {
  //               dest[row_i] = col_struct.base +
  //               col_struct.truncated_values[row_i];
  //           }
  //       }
  //   }
}
// -------------------------------------------------------------------------------------
} // namespace btrblocks::legacy::int64s
