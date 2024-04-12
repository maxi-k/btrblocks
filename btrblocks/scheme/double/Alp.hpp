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

  struct EncodingIndices {
    uint8_t exponent;
    uint8_t factor;

    EncodingIndices(uint8_t exponent, uint8_t factor) : exponent(exponent), factor(factor) {
    }

    EncodingIndices() : exponent(0), factor(0) {
    }
  };

  struct EncodingIndicesEquality {
    bool operator()(const EncodingIndices &a, const EncodingIndices &b) const {
      return a.exponent == b.exponent && a.factor == b.factor;
    }
  };

  typedef uint64_t hash_t;
  struct EncodingIndicesHash {
    hash_t operator()(const EncodingIndices &encoding_indices) const {
      uint8_t h1 = std::hash<uint8_t>{}(encoding_indices.exponent);
      uint8_t h2 = std::hash<uint8_t>{}(encoding_indices.factor);
      return h1 ^ h2;
    }
  };

  struct IndicesAppearancesCombination {
    EncodingIndices encoding_indices;
    uint64_t n_appearances;
    uint64_t estimated_compression_size;

    IndicesAppearancesCombination(EncodingIndices encoding_indices, uint64_t n_appearances, uint64_t estimated_compression_size)
        : encoding_indices(encoding_indices), n_appearances(n_appearances),
          estimated_compression_size(estimated_compression_size) {
    }
  };
};
// -------------------------------------------------------------------------------------
static constexpr u32 BATCH_SIZE = 1 << 12;

struct AlpStructure {
  // ---------------------------------------------------------------------------------
  u32 exceptions_positions_offset;
  u32 exceptions_offset;
  // ---------------------------------------------------------------------------------
  u32 encoded_count;
  u32 exceptions_count;
  u32 batch_count;
  // ---------------------------------------------------------------------------------
  std::array<Alp::EncodingIndices, (1 << 16) / BATCH_SIZE> vector_encoding_indices; // 16 is now 'magic' -> block_size / alp_block_size = 1 << 16 / 1 << 12 = 1 << 4 = 16
  std::array<u32, (1 << 16) / BATCH_SIZE> batch_encoded_count{0}; // 16 is now 'magic' -> block_size / alp_block_size = 1 << 16 / 1 << 12 = 1 << 4 = 16
  std::array<u32, (1 << 16) / BATCH_SIZE> batch_exception_count{0}; // 16 is now 'magic' -> block_size / alp_block_size = 1 << 16 / 1 << 12 = 1 << 4 = 16

  u8 encoding_scheme, exceptions_positions_scheme, exceptions_scheme;
  // ---------------------------------------------------------------------------------
  u8 data[]; // here should be values_encoded, patches, exceptions_map
};
// -------------------------------------------------------------------------------------
} // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
