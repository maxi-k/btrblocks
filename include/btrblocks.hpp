// ------------------------------------------------------------------------------
// BtrBlocks - Generic C++ interface
// ------------------------------------------------------------------------------
#ifndef BTRBLOCKS_H_
#define BTRBLOCKS_H_
// ------------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <type_traits>
// ------------------------------------------------------------------------------
namespace btrblocks {

// ------------------------------------------------------------------------------
// Bare-bones compression interface
// ------------------------------------------------------------------------------


// ------------------------------------------------------------------------------
enum class DataType : uint8_t { Integer, Double, String };
// ------------------------------------------------------------------------------
enum class IntegerSchemeType : uint8_t {
  UNCOMPRESSED     = 0,
  ONE_VALUE        = 1,
  DICT             = 2,
  RLE              = 3,
  FREQUENCY        = 4,
  PBP,
  PBP_DELTA,
  FBP,
  FOR,
  // legacy schemes
  TRUNCATION_8,
  TRUNCATION_16,
  DICTIONARY_8,
  DICTIONARY_16,
  MAX_SCHEME_COUNT = 64
};
// ------------------------------------------------------------------------------
enum class DoubleSchemeType : uint8_t {
  UNCOMPRESSED     = 0,
  ONE_VALUE        = 1,
  DICT             = 2,
  RLE              = 3,
  FREQUENCY        = 4,
  DECIMAL,
  // legacy schemes
  DOUBLE_BP,
  FOR,
  DICTIONARY_8,
  DICTIONARY_16,
  MAX_SCHEME_COUNT = 64
};
// ------------------------------------------------------------------------------
enum class StringSchemeType : uint8_t {
  UNCOMPRESSED     = 0,
  ONE_VALUE        = 1,
  DICT             = 2,
  FSST,
  // legacy schemes
  DICTIONARY_8,
  DICTIONARY_16,
  MAX_SCHEME_COUNT = 64
};
// ------------------------------------------------------------------------------
using SchemeList = uint64_t;
// ------------------------------------------------------------------------------

size_t compress(DataType type, uint8_t* input, size_t length);
// ------------------------------------------------------------------------------


// ------------------------------------------------------------------------------
// Global configuation used by the compression interface
// ------------------------------------------------------------------------------


struct BtrBlocksConfig {
    size_t block_size; // maximum number of tuples in a block
    uint32_t cascade_depth; // maximum number of recursive compression calls
};

BtrBlocksConfig& globalConfig();


// ------------------------------------------------------------------------------
// DataBlock / relation interface
// ------------------------------------------------------------------------------


}  // namespace btrblocks

#endif  // BTRBLOCKS_H_
