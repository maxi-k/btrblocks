#include "AlpRD.hpp"
#include "scheme/CompressionScheme.hpp"
// ------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "compression/SchemePicker.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
#include <iomanip>
#include <roaring/roaring.hh>
// -------------------------------------------------------------------------------------
using namespace std;
// -------------------------------------------------------------------------------------
// based on
// - https://github.com/cwida/ALP/
// - https://ir.cwi.nl/pub/33334
// - https://github.com/duckdb/duckdb/pull/9635/
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {
// -------------------------------------------------------------------------------------
// make future generalization to floats easier
using T = DOUBLE;
using ExactType = uint64_t;
// -------------------------------------------------------------------------------------
bool AlpRD::isUsable(DoubleStats& stats) {
  // TODO
  return true;
}
// -------------------------------------------------------------------------------------
string AlpRD::fullDescription(const u8* src) {
  string result = this->selfDescription();
  // const auto& col_struct = *reinterpret_cast<const AlpRDStructure*>(src);
  // TODO print child schemes as well
  return result;
}
// -------------------------------------------------------------------------------------
// helper functions

using DictKey = uint16_t;
constexpr uint8_t DICTIONARY_BW = 3;
constexpr uint8_t DICTIONARY_SIZE = (1 << DICTIONARY_BW);  // 8
static constexpr uint8_t DICTIONARY_SIZE_BYTES = DICTIONARY_SIZE * sizeof(DictKey);
constexpr uint8_t CUTTING_LIMIT = 16;
constexpr uint8_t EXCEPTION_SIZE = sizeof(uint16_t);

static double estimate_compressed_size(uint8_t right_bw,
                                       uint8_t left_bw,
                                       uint16_t exceptions_count,
                                       uint64_t sample_count) {
  double exceptions_size = exceptions_count * ((sizeof(u32) + EXCEPTION_SIZE) * 8);
  double estimated_size = right_bw + left_bw + (exceptions_size / sample_count);
  return estimated_size;
}

struct CompressionDictionary {
  DictKey dict[DICTIONARY_SIZE]; // what is actually written out
  unordered_map<DictKey, DictKey> dict_map; // runtime state for compression

  static_assert(sizeof(dict) == DICTIONARY_SIZE_BYTES);
};

struct DictionaryAnalysis {
  unordered_map<ExactType, s32> left_parts_hash;
  vector<pair<s32, ExactType>> left_parts_sorted_repetitions;
  u8 right_bw, left_bw;
  double est_size;

  DictionaryAnalysis()
      : right_bw(sizeof(ExactType) * 8)
        , left_bw(DICTIONARY_BW)
        , est_size(std::numeric_limits<double>::max()) {}
  DictionaryAnalysis(const ExactType* values, u32 value_count, uint8_t right_bw, uint8_t left_bw)
      : right_bw(right_bw)
        , left_bw(left_bw)
        , est_size(estimate(values, value_count)) {}
  DictionaryAnalysis(const DictionaryAnalysis& o) = delete;
  DictionaryAnalysis& operator=(DictionaryAnalysis&& o) noexcept {
    left_parts_hash = std::move(o.left_parts_hash);
    left_parts_sorted_repetitions = std::move(o.left_parts_sorted_repetitions);
    right_bw = o.right_bw;
    left_bw = o.left_bw;
    est_size = o.est_size;
    o.est_size = std::numeric_limits<double>::max();
    return *this;
  }

  bool operator <(const DictionaryAnalysis& o) {
    return est_size < o.est_size;
  }

  double estimate(const ExactType* values, u32 value_count) {
    // Building a hash for all the left parts and how many times they appear
    for (u32 i = 0; i < value_count; i++) {
      auto left_tmp = values[i] >> right_bw;
      left_parts_hash[left_tmp]++;
    }

    // We build a vector from the hash to be able to sort it by repetition count
    left_parts_sorted_repetitions.reserve(left_parts_hash.size());
    for (auto& hash_pair : left_parts_hash) {
      left_parts_sorted_repetitions.emplace_back(hash_pair.second, hash_pair.first);
    }
    sort(left_parts_sorted_repetitions.begin(), left_parts_sorted_repetitions.end(),
         [](const pair<uint16_t, uint64_t>& a, const pair<uint16_t, uint64_t>& b) {
           return a.first > b.first;
         });

    // Exceptions are left parts which do not fit in the fixed dictionary size
    uint32_t exceptions_count = 0;
    for (u32 i = DICTIONARY_SIZE; i < left_parts_sorted_repetitions.size(); i++) {
      exceptions_count += left_parts_sorted_repetitions[i].first;
    }

    return estimate_compressed_size(right_bw, DICTIONARY_BW, exceptions_count, value_count);
  }

  CompressionDictionary build() {
    CompressionDictionary result;
    u32 dict_idx = 0;
    u32 size = std::min<uint64_t>(DICTIONARY_SIZE, left_parts_sorted_repetitions.size());
    while (dict_idx < size) {
      //! The dict keys are mapped to the left part themselves
      result.dict[dict_idx] = left_parts_sorted_repetitions[dict_idx].second;
      result.dict_map.insert({result.dict[dict_idx], dict_idx});
      dict_idx++;
    }
    // quickly resolve exceptions during encoding
    for (u32 i = dict_idx + 1; i < left_parts_sorted_repetitions.size(); i++) {
      result.dict_map.insert({left_parts_sorted_repetitions[i].second, i});
    }
    return result;
  }
};

CompressionDictionary find_and_set_bitwidths(AlpRDStructure& dest, const DOUBLE* src, DoubleStats& stats) {
  DictionaryAnalysis best_dict;
  auto* exact_src = reinterpret_cast<const ExactType*>(src);
  //! Finding the best position to CUT the values
  for (u32 i = 1; i <= CUTTING_LIMIT; i++) {
    uint8_t candidate_l_bw = i;
    uint8_t candidate_r_bw = (sizeof(ExactType) * 8) - i;
    DictionaryAnalysis analysis(exact_src, stats.tuple_count, candidate_r_bw, candidate_l_bw);
    if (analysis < best_dict) {
      best_dict = std::move(analysis);
    }
  }
  dest.left_bitwidth = DICTIONARY_BW;
  dest.right_bitwidth = best_dict.right_bw;
  return best_dict.build();
}

// -------------------------------------------------------------------------------------
// compression
// -------------------------------------------------------------------------------------
u32 AlpRD::compress(const DOUBLE* src,
             const BITMAP* nullmap,
             u8* dest,
             DoubleStats& stats,
             u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<AlpRDStructure*>(dest);
  auto cfg = find_and_set_bitwidths(col_struct, src, stats);
  die_if(col_struct.left_bitwidth > 0 && col_struct.left_bitwidth <= CUTTING_LIMIT &&
         col_struct.right_bitwidth > 0);

  /// XXX better allocation
  vector<ExactType> right_parts(stats.tuple_count);
  // TODO alp uses 16 bit here
  vector<u32> left_parts(stats.tuple_count);
  Roaring exceptions_bitmap;
  vector<u32> patches;

  const auto* exact_src = reinterpret_cast<const ExactType*>(src);

  // split double values
  // TODO null values
  for (u32 idx = 0; idx != stats.tuple_count; ++idx) {
    ExactType tmp = exact_src[idx];
    right_parts[idx] = tmp & ((1ULL << col_struct.right_bitwidth) - 1);
    left_parts[idx] = (tmp >> col_struct.right_bitwidth);
  }

  // build dictionary
  for (u32 i = 0; i < stats.tuple_count; i++) {
    auto dictionary_key = left_parts[i];
    auto it = cfg.dict_map.find(dictionary_key);
    uint16_t dictionary_index = it == cfg.dict_map.end()
                                    ? DICTIONARY_SIZE
                                    : it->second;

    left_parts[i] = dictionary_index;

    //! Left parts not found in the dictionary are stored as exceptions
    if (dictionary_index >= DICTIONARY_SIZE) {
      patches.push_back(dictionary_key);
      exceptions_bitmap.add(i);
    }
  }
  col_struct.converted_count = stats.tuple_count - patches.size();

  auto write_ptr = col_struct.data;

  /// write the dictionary
  /// XXX compress as well? it's probably to small to make sense
  {
    memcpy(write_ptr, cfg.dict, DICTIONARY_SIZE_BYTES);
    write_ptr += sizeof(cfg.dict);
  }

  // XXX alp use bitpacking, we use our scheme picker - warn if the chosen scheme was not bitpacking?
  col_struct.left_part_offset = write_ptr - col_struct.data;

  if  (!left_parts.empty()) {
    u32 used_space;
    IntegerSchemePicker::compress(reinterpret_cast<INTEGER*>(left_parts.data()), nullptr, write_ptr, left_parts.size(),
                                  allowed_cascading_level - 1, used_space,
                                  col_struct.left_parts_scheme, autoScheme(),
                                  "alp left parts");
    write_ptr += used_space;
    Log::debug("AlpRD left c = {} s = {}", CI(col_struct.left_parts_scheme), CI(used_space));
  }

  // compress right parts
  col_struct.right_part_offset = write_ptr - col_struct.data;
  {
    u32 used_space;
    /// statically setting bitpacking here; test better options
    Int64SchemePicker::compress(
        reinterpret_cast<int64_t*>(right_parts.data()), nullptr, write_ptr, right_parts.size(), allowed_cascading_level - 1,
        used_space, col_struct.right_parts_scheme, autoScheme(), "alp right parts");
    write_ptr += used_space;
    Log::debug("AlpRD right c = {} s = {}", CI(col_struct.right_parts_scheme), CI(used_space));
  }

  // compress patches
  col_struct.patches_offset = write_ptr - col_struct.data;
  {
    u32 used_space;
    IntegerSchemePicker::compress(reinterpret_cast<INTEGER*>(patches.data()), nullptr,
                                  write_ptr, patches.size(), allowed_cascading_level - 1,
                                  used_space, col_struct.patches_scheme, autoScheme(),
                                  "patches");
    write_ptr += used_space;
    Log::debug("AlpRD patches c = {} s = {}", CI(col_struct.right_parts_scheme), CI(used_space));
  }

  // compress exception bitmap
  col_struct.exceptions_map_offset = write_ptr - col_struct.data;
  {
    exceptions_bitmap.runOptimize();
    exceptions_bitmap.setCopyOnWrite(true);
    write_ptr += exceptions_bitmap.write(reinterpret_cast<char*>(write_ptr), false);
  }

  return write_ptr - dest;
}
// -------------------------------------------------------------------------------------
// decompression
// -------------------------------------------------------------------------------------
void AlpRD::decompress(DOUBLE* dest, BitmapWrapper* bitmap, const u8* src, u32 tuple_count, u32 level) {
  const auto& col_struct = *reinterpret_cast<const AlpRDStructure*>(src);
  auto* exact_dest = reinterpret_cast<ExactType*>(dest);

  // decompression memory
  thread_local vector<vector<u32>> left_part_v;
  thread_local vector<vector<INT64>> right_part_v;
  thread_local vector<vector<u32>> patches_v;
  auto left_part_ptr = get_level_data(left_part_v, tuple_count, level);
  auto right_part_ptr = get_level_data(right_part_v, tuple_count, level);
  auto patches_ptr = get_level_data(patches_v, tuple_count, level);


  // unconditionally decompress dictionary and right part
  DictKey left_dict[DICTIONARY_SIZE];
  memcpy(left_dict, col_struct.data, DICTIONARY_SIZE_BYTES);

  Int64Scheme& right_scheme =
      Int64SchemePicker::MyTypeWrapper::getScheme(col_struct.right_parts_scheme);
  right_scheme.decompress(right_part_v[level].data(), nullptr, col_struct.data + col_struct.right_part_offset,
                          tuple_count, level + 1);

  // decompress left part if not everything is a patch
  if (col_struct.converted_count > 0) {
    IntegerScheme& left_scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.left_parts_scheme);
    left_scheme.decompress(reinterpret_cast<INTEGER*>(left_part_v[level].data()), nullptr, col_struct.data + col_struct.left_part_offset, tuple_count, level + 1);


    // Decode
    for (u32 i = 0; i != tuple_count; ++i) {
      DictKey left = left_dict[left_part_ptr[i]];
      ExactType right = right_part_ptr[i];
      exact_dest[i] = (static_cast<ExactType>(left) << col_struct.right_bitwidth) | right;
    }
  }

  // patch exceptions
  auto exception_count = tuple_count - col_struct.converted_count;
  if (exception_count) {
    IntegerScheme& patch_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.patches_scheme);
    patch_scheme.decompress(reinterpret_cast<INTEGER*>(patches_v[level].data()), nullptr,
                            col_struct.data + col_struct.patches_offset, exception_count, level + 1);

    Roaring exceptions_bitmap =
        Roaring::read(reinterpret_cast<const char*>(col_struct.data) + col_struct.exceptions_map_offset, false);

    auto param = std::make_tuple(exact_dest, right_part_ptr, patches_ptr, 0ul, col_struct.right_bitwidth);
    exceptions_bitmap.iterate(
        [](uint32_t bitmap_index, void* cursor) {
          auto& [dest, rightv, patches, cnt, right_bit_width] = *reinterpret_cast<decltype(param)*>(cursor);
          ExactType right = rightv[bitmap_index];
          uint16_t left = patches[cnt++];
          dest[bitmap_index] = (static_cast<ExactType>(left) << right_bit_width) | right;
          return true;
        },
        &param);
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
