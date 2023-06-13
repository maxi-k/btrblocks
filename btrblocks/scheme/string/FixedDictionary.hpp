#pragma once
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::strings {
class Dictionary8 : public StringScheme {
 public:
  double expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) override;
  u32 compress(const StringArrayViewer src,
               const BITMAP* nullmap,
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
  inline static StringSchemeType staticSchemeType() { return StringSchemeType::DICTIONARY_8; }
};
// -------------------------------------------------------------------------------------
class Dictionary16 : public StringScheme {
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
  inline static StringSchemeType staticSchemeType() { return StringSchemeType::DICTIONARY_16; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::strings
// -------------------------------------------------------------------------------------
