#pragma once
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::strings {
// -------------------------------------------------------------------------------------
struct DynamicDictionaryStructure {
  u32 total_decompressed_size;  // i.e original input size
  bool use_fsst;
  u32 fsst_offsets_offset;
  u32 lengths_offset;
  u32 num_codes;
  u32 codes_offset;
  bool use_rle_optimized_path;
  u8 codes_scheme;
  u8 lengths_scheme;
  u8 data[];
};
// -------------------------------------------------------------------------------------
class DynamicDictionary : public StringScheme {
 public:
  double expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) override;
  bool usesFsst(const u8* src) override;
  u32 compress(StringArrayViewer src, const BITMAP* nullmap, u8* dest, StringStats& stats) override;
  std::string fullDescription(const u8* src) override;
  bool isUsable(StringStats& stats) override;
  u32 getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  u32 getDecompressedSizeNoCopy(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  u32 getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) override;
  void decompress(u8* dest,
                  BitmapWrapper* nullmap,
                  const u8* src,
                  u32 tuple_count,
                  u32 level) override;
  bool decompressNoCopy(u8* dest,
                        BitmapWrapper* nullmap,
                        const u8* src,
                        u32 tuple_count,
                        u32 level) override;
  inline StringSchemeType schemeType() override { return staticSchemeType(); }
  inline static StringSchemeType staticSchemeType() { return StringSchemeType::DICT; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::strings
// -------------------------------------------------------------------------------------
