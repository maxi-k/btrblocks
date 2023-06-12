#pragma once
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "storage/Chunk.hpp"
#include "storage/StringArrayViewer.hpp"
// -------------------------------------------------------------------------------------
#include "extern/RoaringBitmap.hpp"
#include "stats/NumberStats.hpp"
#include "stats/StringStats.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
using UInteger32Stats = NumberStats<u32>;
using SInteger32Stats = NumberStats<s32>;
using DoubleStats = NumberStats<DOUBLE>;
// -------------------------------------------------------------------------------------
struct Predicate {};
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(IntegerSchemeType type);
string ConvertSchemeTypeToString(DoubleSchemeType type);
string ConvertSchemeTypeToString(StringSchemeType type);
// -------------------------------------------------------------------------------------
// expectedCompressionRatio should only be called at top level
class IntegerScheme {
 public:
  // -------------------------------------------------------------------------------------
  virtual double expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level);
  // -------------------------------------------------------------------------------------
  virtual u32 compress(const INTEGER* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       SInteger32Stats& stats,
                       u8 allowed_cascading_level) = 0;
  // -------------------------------------------------------------------------------------
  virtual void decompress(INTEGER* dest,
                          BitmapWrapper* nullmap,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) = 0;
  // -------------------------------------------------------------------------------------
  virtual IntegerSchemeType schemeType() = 0;
  // -------------------------------------------------------------------------------------
  virtual INTEGER lookup(u32 id) = 0;
  // -------------------------------------------------------------------------------------
  virtual void scan(Predicate, BITMAP* result, const u8* src, u32 tuple_count) = 0;
  // -------------------------------------------------------------------------------------
  inline string selfDescription() { return ConvertSchemeTypeToString(this->schemeType()); }
  virtual string fullDescription(const u8*) {
    // Default implementation for schemes that do not have nested schemes
    return this->selfDescription();
  }
  virtual bool isUsable(SInteger32Stats&) { return true; }
};
// -------------------------------------------------------------------------------------
// Double
// -------------------------------------------------------------------------------------
class DoubleScheme {
 public:
  // -------------------------------------------------------------------------------------
  virtual double expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level);
  // -------------------------------------------------------------------------------------
  virtual u32 compress(const DOUBLE* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       DoubleStats& stats,
                       u8 allowed_cascading_level) = 0;
  // -------------------------------------------------------------------------------------
  virtual void decompress(DOUBLE* dest,
                          BitmapWrapper* bitmap,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) = 0;
  // -------------------------------------------------------------------------------------
  virtual DoubleSchemeType schemeType() = 0;
  // -------------------------------------------------------------------------------------
  inline string selfDescription() { return ConvertSchemeTypeToString(this->schemeType()); }
  virtual string fullDescription(const u8*) {
    // Default implementation for schemes that do not have nested schemes
    return this->selfDescription();
  }
  virtual bool isUsable(DoubleStats&) { return true; }
};
// -------------------------------------------------------------------------------------
// String
// -------------------------------------------------------------------------------------
class StringScheme {
 public:
  // -------------------------------------------------------------------------------------
  virtual double expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) = 0;

  // -------------------------------------------------------------------------------------
  // TODO get rid of this function
  virtual bool usesFsst(const u8* src) {
    (void)src;
    return false;
  }
  // -------------------------------------------------------------------------------------
  virtual u32 compress(StringArrayViewer src,
                       const BITMAP* nullmap,
                       u8* dest,
                       StringStats& stats) = 0;
  // -------------------------------------------------------------------------------------
  virtual u32 getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) = 0;
  // -------------------------------------------------------------------------------------
  virtual u32 getDecompressedSizeNoCopy(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
    return this->getDecompressedSize(src, tuple_count, nullmap);
  }
  // -------------------------------------------------------------------------------------
  virtual u32 getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) = 0;
  // -------------------------------------------------------------------------------------
  virtual void decompress(u8* dest,
                          BitmapWrapper* nullmap,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) = 0;
  // -------------------------------------------------------------------------------------
  virtual bool decompressNoCopy(u8* dest,
                                BitmapWrapper* nullmap,
                                const u8* src,
                                u32 tuple_count,
                                u32 level) {
    // The string representation is the same in any case. Still performing the
    // copy does not lead to a wrong result.
    this->decompress(dest, nullmap, src, tuple_count, level);
    return false;
  }
  // -------------------------------------------------------------------------------------
  virtual StringSchemeType schemeType() = 0;
  // -------------------------------------------------------------------------------------
  inline string selfDescription(const u8* src = nullptr) {
    auto description = ConvertSchemeTypeToString(this->schemeType());
    // TODO clean this up once we are done
    if (this->schemeType() == StringSchemeType::DICT) {
      if (src == nullptr) {
        description += "_UNKNOWN";
      } else if (this->usesFsst(src)) {
        description += "_FSST";
      } else {
        description += "_RAW";
      }
    }
    return description;
  }
  virtual string fullDescription(const u8*) {
    // Default implementation for schemes that do not have nested schemes
    return this->selfDescription();
  }
  virtual bool isUsable(StringStats&) { return true; }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
enum { MAX_STR_LENGTH = (2048 * 4) };

/*
 *
 * General plan:
 * each scheme needs only to register its offset from the datablock
 * and it can define a structure e.g (header; slots[];..) and cast the beginning
 * pointer to its type
 *
 */
