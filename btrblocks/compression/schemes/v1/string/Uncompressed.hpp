#pragma once
// -------------------------------------------------------------------------------------
#include "compression/schemes/CScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v1::string {
// -------------------------------------------------------------------------------------
struct UncompressedStructure {
  u32 total_size;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class Uncompressed : public StringScheme {
 public:
  double expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) override;
  u32 compress(const StringArrayViewer src,
               const BITMAP* bitmap,
               u8* dest,
               StringStats& stats) override;
  u32 getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  u32 getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  void decompress(u8* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  inline StringSchemeType schemeType() override { return staticSchemeType(); }
  inline static StringSchemeType staticSchemeType() { return StringSchemeType::UNCOMPRESSED; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v1::string
// -------------------------------------------------------------------------------------
