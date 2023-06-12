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
#include "scheme/SchemeConfig.hpp"
#include "scheme/SchemeType.hpp"
// ------------------------------------------------------------------------------
namespace btrblocks {
// ------------------------------------------------------------------------------
// Global configuation used by the compression interface
// ------------------------------------------------------------------------------
enum class SchemeSelection : uint8_t { SAMPLE, TRY_ALL };
// ------------------------------------------------------------------------------
struct BtrBlocksConfig {
  size_t block_size{65536};   // max tuples in a single block
  uint32_t sample_size{64};   // run size of each sample
  uint32_t sample_count{10};  // number of samples to take

  struct {
    IntegerSchemeSet schemes{defaultIntegerSchemes()};  // enabled integer schemes
    IntegerSchemeType override_scheme{autoScheme()};  // force using this scheme for integer columns
    uint8_t max_cascade_depth{3};                     // maximum recursive compression calls
  } integers;

  struct {
    DoubleSchemeSet schemes{defaultDoubleSchemes()};  // enabled double schemes
    DoubleSchemeType override_scheme{autoScheme()};   // force using this scheme for double columns
    uint8_t max_cascade_depth{3};                     // maximum recursive compression calls
  } doubles;

  struct {
    StringSchemeSet schemes{defaultStringSchemes()};  // enabled string schemes
    StringSchemeType override_scheme{autoScheme()};   // force using this scheme for string columns
    uint8_t max_cascade_depth{3};                     // maximum recursive compression calls
  } strings;

  SchemeSelection scheme_selection{SchemeSelection::SAMPLE};  // sample or try all schemes?

  static BtrBlocksConfig& get() {
    static BtrBlocksConfig config;
    return config;
  }

  static BtrBlocksConfig& local() {
    thread_local BtrBlocksConfig config{get()};
    return config;
  }
};
// ------------------------------------------------------------------------------
// Bare-bones compression interface
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
// DataBlock / relation interface
// ------------------------------------------------------------------------------

}  // namespace btrblocks

#endif  // BTRBLOCKS_H_
