#ifndef BTRBLOCKS_SCHEMECONFIG_H_
#define BTRBLOCKS_SCHEMECONFIG_H_
// ------------------------------------------------------------------------------
// Scheme-specific configuration options. You probably don't want to change
// these unless you really know what you are doing.
// ------------------------------------------------------------------------------
#include <cstdint>
#include "scheme/SchemeType.hpp"
// ------------------------------------------------------------------------------
namespace btrblocks {
// ------------------------------------------------------------------------------
struct SchemeConfig {
  // ------------------------------------------------------------------------------
  struct {
    // maximum percentage of unique elements in a block
    // for which frequency compression will be enabled
    uint32_t frequency_threshold_pct{50};
    // the average run length has to be higher than this for integer RLE to be
    // condierered
    uint32_t rle_run_length_threshold{2};
    // in integer RLE, override the scheme used for values with
    // this scheme instead of using the scheme picking algorithm
    IntegerSchemeType rle_force_values_scheme{autoScheme()};
    // in integer RLE, override the scheme used for run lengths with
    // this scheme instead of using the scheme picking algorithm
    IntegerSchemeType rle_force_counts_scheme{autoScheme()};
  } integers;
  // ------------------------------------------------------------------------------
  struct {
    // maximum percentage of unique elements in a block
    // for which frequency compression will be enabled
    uint32_t frequency_threshold_pct{50};
    // in double RLE, override the scheme used for values with
    // this scheme instead of using the scheme picking algorithm
    DoubleSchemeType rle_force_values_scheme{autoScheme()};
    // in double RLE, override the scheme used for run lengths with
    // this scheme instead of using the scheme picking algorithm
    IntegerSchemeType rle_force_counts_scheme{autoScheme()};
    // in pseudodecimal encoding, treat a value as an outlier and patch
    // it if it requires more than this many significant digit bits
    uint32_t pseudodecimal_significant_digit_bits_limits{31};
  } doubles;
  // ------------------------------------------------------------------------------
  static constexpr size_t FSST_THRESHOLD = 16ul * 1024;
  struct {
    // in fused dictionary + fsst encoding, the dictionary needs
    // to have at least this size before we try to use FSST
    size_t dict_fsst_input_size_threshold{FSST_THRESHOLD};
    // whether to consider using FSST for dictionary encoding (does not
    // force FSST usage, merely allows it)
    bool dict_allow_fsst{true};
    // whether to force FSST usage for the dictionary (useful for
    // experimentation)
    bool dict_force_fsst{false};
    // in fsst encoding, allow this many recursive
    // compression calls for the fsst codes
    uint8_t fsst_codes_max_cascade_depth{2};
    // in FSST, override the scheme used for the codes with
    // this scheme instead of using the scheme picking algorithm
    IntegerSchemeType fsst_force_codes_scheme{autoScheme()};
  } strings;
  // ------------------------------------------------------------------------------
  static SchemeConfig& get() {
    static SchemeConfig instance;
    return instance;
  }
  // ------------------------------------------------------------------------------
};
// ------------------------------------------------------------------------------
}  // namespace btrblocks
// ------------------------------------------------------------------------------
#endif
