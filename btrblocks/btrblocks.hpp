// ------------------------------------------------------------------------------
// BtrBlocks - Generic C++ interface
// ------------------------------------------------------------------------------
#ifndef BTRBLOCKS_H_
#define BTRBLOCKS_H_
// ------------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
// ------------------------------------------------------------------------------
#include "compression/Datablock.hpp"
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
  // clang-format off
  size_t block_size{65536};                            // max tuples in a single block
  uint32_t sample_size{64};                            // run size of each sample
  uint32_t sample_count{10};                           // number of samples to take

  struct {
    IntegerSchemeSet schemes{defaultIntegerSchemes()}; // enabled integer schemes
    IntegerSchemeType override_scheme{autoScheme()};   // force using this scheme for integer columns
    uint8_t max_cascade_depth{3};                      // maximum recursive compression calls
  } integers;

  struct {
    DoubleSchemeSet schemes{defaultDoubleSchemes()};   // enabled double schemes
    DoubleSchemeType override_scheme{autoScheme()};    // force using this scheme for double columns
    uint8_t max_cascade_depth{3};                      // maximum recursive compression calls
  } doubles;

  struct {
    StringSchemeSet schemes{defaultStringSchemes()};   // enabled string schemes
    StringSchemeType override_scheme{autoScheme()};    // force using this scheme for string columns
    uint8_t max_cascade_depth{3};                      // maximum recursive compression calls
  } strings;
  // clang-format on

  SchemeSelection scheme_selection{SchemeSelection::SAMPLE};  // sample or try all schemes?

  /// Get the global configuration instance.
  /// This is a singleton, so you can modify it to change the compression behaviour.
  /// Changing the set of available schemes requires using the `configure` method instead,
  /// which will re-initialize the compression scheme pool.
  ///
  /// Updating this while compression is still running on another thread is not safe.
  static BtrBlocksConfig& get() {
    static BtrBlocksConfig config;
    return config;
  }

  static BtrBlocksConfig& local() {
    thread_local BtrBlocksConfig config{get()};
    return config;
  }

  /// Call this during program startup to configure the compression interface.
  /// You can use the passed function to modify the configuration.
  /// It will set up the compression scheme pool and settings with your modified config.
  static void configure(const std::function<void(BtrBlocksConfig&)>& f = [](BtrBlocksConfig&) {});
};
// ------------------------------------------------------------------------------
// Bare-bones compression interface
// ------------------------------------------------------------------------------

// ------------------------------------------------------------------------------
// DataBlock / relation interface
// ------------------------------------------------------------------------------

}  // namespace btrblocks

#endif  // BTRBLOCKS_H_
