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
struct AlpEncodingIndices {
  uint8_t exponent;
  uint8_t factor;

  AlpEncodingIndices(uint8_t exponent, uint8_t factor) : exponent(exponent), factor(factor) {
  }

  AlpEncodingIndices() : exponent(0), factor(0) {
  }
};

struct AlpEncodingIndicesEquality {
  bool operator()(const AlpEncodingIndices &a, const AlpEncodingIndices &b) const {
    return a.exponent == b.exponent && a.factor == b.factor;
  }
};

typedef uint64_t hash_t;
struct AlpEncodingIndicesHash {
  hash_t operator()(const AlpEncodingIndices &encoding_indices) const {
    uint8_t h1 = std::hash<uint8_t>{}(encoding_indices.exponent);
    uint8_t h2 = std::hash<uint8_t>{}(encoding_indices.factor);
    return h1 ^ h2;
  }
};

struct AlpStructure {
  // ---------------------------------------------------------------------------------
  u32 patches_offset;
  u32 exceptions_offset;
  // ---------------------------------------------------------------------------------
  u32 encoded_count;
  u32 exceptions_count;
  AlpEncodingIndices vector_encoding_indices;
  u8 encoding_scheme, patches_scheme, exceptions_scheme;
  // ---------------------------------------------------------------------------------
  u8 data[]; // here should be values_encoded, patches, exceptions_map
};

struct AlpCombination {
  AlpEncodingIndices encoding_indices;
  uint64_t n_appearances;
  uint64_t estimated_compression_size;

  AlpCombination(AlpEncodingIndices encoding_indices, uint64_t n_appearances, uint64_t estimated_compression_size)
      : encoding_indices(encoding_indices), n_appearances(n_appearances),
        estimated_compression_size(estimated_compression_size) {
  }
};
// -------------------------------------------------------------------------------------
class Alp : public DoubleScheme {
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
  inline static DoubleSchemeType staticSchemeType() { return DoubleSchemeType::ALP; }
};
// -------------------------------------------------------------------------------------
} // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
