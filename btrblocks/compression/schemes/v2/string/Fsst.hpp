#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/CScheme.hpp"
// -------------------------------------------------------------------------------------
#include <fsst.h>
// -------------------------------------------------------------------------------------

namespace btrblocks::db::v2::string {
struct FsstStructure {
  u32 total_decompressed_size;
  u32 compressed_strings_size;
  u32 strings_offset;
  u32 offsets_offset;
  u8 offsets_scheme;
  u8 data[];
};

class Fsst : public StringScheme {
 public:
  double expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) override;
  u32 compress(StringArrayViewer src, const BITMAP* nullmap, u8* dest, StringStats& stats) override;
  u32 getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  u32 getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  void decompress(u8* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  bool isUsable(StringStats& stats) override;
  std::string fullDescription(const u8* src) override;
  inline StringSchemeType schemeType() override { return staticSchemeType(); }
  inline static StringSchemeType staticSchemeType() { return StringSchemeType::FSST; }
};
}  // namespace btrblocks::db::v2::string
