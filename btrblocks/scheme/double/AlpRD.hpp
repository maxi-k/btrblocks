#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
// based on
// - https://github.com/cwida/ALP/
// - https://ir.cwi.nl/pub/33334
// - https://github.com/duckdb/duckdb/pull/9635/
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
struct AlpRDStructure {
  // ---------------------------------------------------------------------------------
  u32 left_part_offset, right_part_offset;
  u32 patches_offset;
  u32 exceptions_map_offset;
  // ---------------------------------------------------------------------------------
  u32 converted_count;
  u8 left_bitwidth, right_bitwidth;
  u8 left_parts_scheme, right_parts_scheme, patches_scheme;
  // ---------------------------------------------------------------------------------
  u8 data[];
};
// -------------------------------------------------------------------------------------
class AlpRD : public DoubleScheme {
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
  inline static DoubleSchemeType staticSchemeType() { return DoubleSchemeType::ALP_RD; }
};
// -------------------------------------------------------------------------------------
} // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------

