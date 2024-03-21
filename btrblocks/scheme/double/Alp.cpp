#include "Alp.hpp"
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
#include <immintrin.h>
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
bool Alp::isUsable(DoubleStats& stats) {
  // TODO
  return true;
}
// -------------------------------------------------------------------------------------
string Alp::fullDescription(const u8* src) {
  string result = this->selfDescription();
  // const auto& col_struct = *reinterpret_cast<const AlpRDStructure*>(src);
  // TODO print child schemes as well
  return result;
}
// -------------------------------------------------------------------------------------
// constants functions
// Largest double which fits into an int64
static constexpr double ENCODING_UPPER_LIMIT = 9223372036854774784;
static constexpr double ENCODING_LOWER_LIMIT = -9223372036854774784;

static constexpr uint8_t SAMPLING_EARLY_EXIT_THRESHOLD = 2;
static constexpr uint8_t EXCEPTION_POSITION_SIZE = sizeof(uint16_t);
static constexpr uint8_t EXACT_TYPE_BITSIZE = sizeof(double) * 8;

static constexpr double MAGIC_NUMBER = 6755399441055744.0; //! 2^51 + 2^52
static constexpr uint8_t MAX_EXPONENT = 18;                //! 10^18 is the maximum int64
static constexpr uint8_t MAX_COMBINATIONS = 5;

static constexpr uint32_t RG_SAMPLES = 8;
static constexpr uint16_t SAMPLES_PER_VECTOR = 32;

static constexpr const int64_t FACT_ARR[] = {1,
                                             10,
                                             100,
                                             1000,
                                             10000,
                                             100000,
                                             1000000,
                                             10000000,
                                             100000000,
                                             1000000000,
                                             10000000000,
                                             100000000000,
                                             1000000000000,
                                             10000000000000,
                                             100000000000000,
                                             1000000000000000,
                                             10000000000000000,
                                             100000000000000000,
                                             1000000000000000000};

static constexpr const double EXP_ARR[] = {1.0,
                                           10.0,
                                           100.0,
                                           1000.0,
                                           10000.0,
                                           100000.0,
                                           1000000.0,
                                           10000000.0,
                                           100000000.0,
                                           1000000000.0,
                                           10000000000.0,
                                           100000000000.0,
                                           1000000000000.0,
                                           10000000000000.0,
                                           100000000000000.0,
                                           1000000000000000.0,
                                           10000000000000000.0,
                                           100000000000000000.0,
                                           1000000000000000000.0,
                                           10000000000000000000.0,
                                           100000000000000000000.0,
                                           1000000000000000000000.0,
                                           10000000000000000000000.0,
                                           100000000000000000000000.0};

static constexpr double FRAC_ARR[] = {1.0,
                                      0.1,
                                      0.01,
                                      0.001,
                                      0.0001,
                                      0.00001,
                                      0.000001,
                                      0.0000001,
                                      0.00000001,
                                      0.000000001,
                                      0.0000000001,
                                      0.00000000001,
                                      0.000000000001,
                                      0.0000000000001,
                                      0.00000000000001,
                                      0.000000000000001,
                                      0.0000000000000001,
                                      0.00000000000000001,
                                      0.000000000000000001,
                                      0.0000000000000000001,
                                      0.00000000000000000001};
// -------------------------------------------------------------------------------------
// helper functions
// what we need to do here is getting the correct exponent and factor like in duckdb
// here basic helper functions

/*
	 * Check for special values which are impossible for ALP to encode
	 * because they cannot be cast to int64 without an undefined behaviour
 */
static bool IsImpossibleToEncode(double n) {
  return (std::isnan(n) || std::isinf(n)) || n > ENCODING_UPPER_LIMIT ||
         n < ENCODING_LOWER_LIMIT || (n == 0.0 && std::signbit(n)); //! Verification for -0.0
}

/*
	 * Conversion from a Floating-Point number to Int64 without rounding
 */
static int64_t NumberToInt64(double n) {
  if (IsImpossibleToEncode(n)) {
    return ENCODING_UPPER_LIMIT;
  }
  n = n + MAGIC_NUMBER - MAGIC_NUMBER;
  return static_cast<int64_t>(n);
}

/*
	 * Encoding a single value with ALP
 */
static int64_t EncodeValue(DOUBLE value, Alp::EncodingIndices encoding_indices) {
  DOUBLE tmp_encoded_value = value * EXP_ARR[encoding_indices.exponent] *
                        FRAC_ARR[encoding_indices.factor];
  int64_t encoded_value = NumberToInt64(tmp_encoded_value);
  return encoded_value;
}

/*
	 * Decoding a single value with ALP
 */
static DOUBLE DecodeValue(int64_t encoded_value, Alp::EncodingIndices encoding_indices) {
  //! The cast to T is needed to prevent a signed integer overflow
  DOUBLE decoded_value = static_cast<DOUBLE>(encoded_value * FACT_ARR[encoding_indices.factor]) *
                    FRAC_ARR[encoding_indices.exponent];
  return decoded_value;
}

inline __m256d int64_to_double256(__m256i x){                   /*  Mysticial's fast int64_to_double. Works for inputs in the range: (-2^51, 2^51)  */
  x = _mm256_add_epi64(x, _mm256_castpd_si256(_mm256_set1_pd(0x0018000000000000)));
  return _mm256_sub_pd(_mm256_castsi256_pd(x), _mm256_set1_pd(0x0018000000000000));
}

static __m256d DecodeValue(__m256i encoded_value, Alp::EncodingIndices encoding_indices) {
  // Load factor and exponent from encoding_indices into SIMD registers
  __m256d factor = _mm256_set1_pd(static_cast<double>(FACT_ARR[encoding_indices.factor]));
  __m256d exponent = _mm256_set1_pd(FRAC_ARR[encoding_indices.exponent]);

  // Convert encoded_value to double precision floating point SIMD register
  __m256d encoded_double = int64_to_double256(encoded_value);

  // Multiply encoded_value with factorClang-Tidy: '_mm256_mul_pd' is a non-portable x86_64 intrinsic function
  __m256d scaled_value = _mm256_mul_pd(encoded_double, factor);

  // Multiply scaled_value with exponent
  return _mm256_mul_pd(scaled_value, exponent);
}

/*
	 * Return TRUE if c1 is a better combination than c2
	 * First criteria is number of times it appears as best combination
	 * Second criteria is the estimated compression size
	 * Third criteria is bigger exponent
	 * Fourth criteria is bigger factor
 */
static bool CompareALPCombinations(const Alp::IndicesAppearancesCombination &c1, const Alp::IndicesAppearancesCombination &c2) {
  return (c1.n_appearances > c2.n_appearances) ||
         (c1.n_appearances == c2.n_appearances &&
          (c1.estimated_compression_size < c2.estimated_compression_size)) ||
         ((c1.n_appearances == c2.n_appearances &&
           c1.estimated_compression_size == c2.estimated_compression_size) &&
          (c2.encoding_indices.exponent < c1.encoding_indices.exponent)) ||
         ((c1.n_appearances == c2.n_appearances &&
           c1.estimated_compression_size == c2.estimated_compression_size &&
           c2.encoding_indices.exponent == c1.encoding_indices.exponent) &&
          (c2.encoding_indices.factor < c1.encoding_indices.factor));
}

/*
	 * Dry compress a vector (ideally a sample) to estimate ALP compression size given a exponent and factor
 */
template <bool PENALIZE_EXCEPTIONS>
static uint64_t DryCompressToEstimateSize(const vector<DOUBLE> &input_vector, Alp::EncodingIndices encoding_indices) {
  size_t n_values = input_vector.size();
  size_t exceptions_count = 0;
  size_t non_exceptions_count = 0;
  uint32_t estimated_bits_per_value;
  uint64_t estimated_compression_size = 0;
  int64_t max_encoded_value = std::numeric_limits<int64_t>::lowest();
  int64_t min_encoded_value = std::numeric_limits<int64_t>::max();

  for (const DOUBLE &value : input_vector) {
    int64_t encoded_value = EncodeValue(value, encoding_indices);
    DOUBLE decoded_value = DecodeValue(encoded_value, encoding_indices);
    if (decoded_value == value) {
      non_exceptions_count++;
      max_encoded_value = std::max(encoded_value, max_encoded_value);
      min_encoded_value = std::min(encoded_value, min_encoded_value);
      continue;
    }
    exceptions_count++;
  }

  // We penalize combinations which yields to almost all exceptions
  if (PENALIZE_EXCEPTIONS && non_exceptions_count < 2) {
    return std::numeric_limits<int64_t>::max();
  }

  // Evaluate factor/exponent compression size (we optimize for FOR)
  uint64_t delta = (static_cast<uint64_t>(max_encoded_value) - static_cast<uint64_t>(min_encoded_value));
  estimated_bits_per_value = std::ceil(std::log2(delta + 1));
  estimated_compression_size += n_values * estimated_bits_per_value;
  estimated_compression_size +=
      exceptions_count * (sizeof(double) * 8 + (sizeof(uint16_t) * 8));
  return estimated_compression_size;
}

/*
	 * Find the best combinations of factor-exponent from each vector sampled from a rowgroup
	 * This function is called once per segment
	 * This operates over ALP first level samples
 */
static vector<Alp::IndicesAppearancesCombination> FindTopKCombinations(const vector<vector<DOUBLE>> &vectors_sampled) {
  unordered_map<Alp::EncodingIndices, uint64_t, Alp::EncodingIndicesHash, Alp::EncodingIndicesEquality>
      best_k_combinations_hash;
  // For each vector sampled
  for (auto &sampled_vector : vectors_sampled) {
    size_t n_samples = sampled_vector.size();
    Alp::EncodingIndices best_encoding_indices = {MAX_EXPONENT,
                                                MAX_EXPONENT};

    //! We start our optimization with the worst possible total bits obtained from compression
    size_t best_total_bits = (n_samples * (EXACT_TYPE_BITSIZE + EXCEPTION_POSITION_SIZE * 8)) +
                             (n_samples * EXACT_TYPE_BITSIZE);

    // N of appearances is irrelevant at this phase; we search for the best compression for the vector
    Alp::IndicesAppearancesCombination best_combination = {best_encoding_indices, 0, best_total_bits};
    //! We try all combinations in search for the one which minimize the compression size
    for (int8_t exp_idx = MAX_EXPONENT; exp_idx >= 0; exp_idx--) {
      for (int8_t factor_idx = exp_idx; factor_idx >= 0; factor_idx--) {
        Alp::EncodingIndices current_encoding_indices = {static_cast<uint8_t>(exp_idx), static_cast<uint8_t>(factor_idx)};
        uint64_t estimated_compression_size =
            DryCompressToEstimateSize<true>(sampled_vector, current_encoding_indices);
        Alp::IndicesAppearancesCombination current_combination = {current_encoding_indices, 0, estimated_compression_size};
        if (CompareALPCombinations(current_combination, best_combination)) {
          best_combination = current_combination;
        }
      }
    }
    best_k_combinations_hash[best_combination.encoding_indices]++;
  }

  // Convert our hash to a Combination vector to be able to sort
  // Note that this vector is always small (< 10 combinations)
  vector<Alp::IndicesAppearancesCombination> best_k_combinations;
  for (auto const &combination : best_k_combinations_hash) {
    best_k_combinations.emplace_back(
        combination.first,  // Encoding Indices
        combination.second, // N of times it appeared (hash value)
        0 // Compression size is irrelevant at this phase since we compare combinations from different vectors
    );
  }
  sort(best_k_combinations.begin(), best_k_combinations.end(), CompareALPCombinations);

  // Save k' best combinations
  vector<Alp::IndicesAppearancesCombination> max_best_k_combinations;

  for (size_t i = 0; i < std::min(MAX_COMBINATIONS, static_cast<uint8_t>(best_k_combinations.size())); i++) {
    max_best_k_combinations.push_back(best_k_combinations[i]);
  }

  return max_best_k_combinations;
}

/*
	 * Find the best combination of factor-exponent for a vector from within the best k combinations
	 * This is ALP second level sampling
 */
static Alp::EncodingIndices FindBestFactorAndExponent(const DOUBLE *input_vector, size_t n_values, vector<Alp::IndicesAppearancesCombination>& best_k_combinations) {
  //! We sample equidistant values within a vector; to do this we skip a fixed number of values
  vector<DOUBLE> vector_sample;
  uint32_t idx_increments = std::max(1, static_cast<int32_t>(std::ceil(static_cast<double>(n_values) / SAMPLES_PER_VECTOR)));
  for (size_t i = 0; i < n_values; i += idx_increments) {
    vector_sample.push_back(input_vector[i]);
  }

  Alp::EncodingIndices best_encoding_indices = {0, 0};
  uint64_t best_total_bits = std::numeric_limits<int64_t>::max();
  size_t worse_total_bits_counter = 0;

  //! We try each K combination in search for the one which minimize the compression size in the vector
  for (auto &combination : best_k_combinations) {
    uint64_t estimated_compression_size =
        DryCompressToEstimateSize<false>(vector_sample, combination.encoding_indices);

    // If current compression size is worse (higher) or equal than the current best combination
    if (estimated_compression_size >= best_total_bits) {
      worse_total_bits_counter += 1;
      // Early exit strategy
      if (worse_total_bits_counter == SAMPLING_EARLY_EXIT_THRESHOLD) {
        break;
      }
      continue;
    }
    // Otherwise we replace the best and continue trying with the next combination
    best_total_bits = estimated_compression_size;
    best_encoding_indices = combination.encoding_indices;
    worse_total_bits_counter = 0;
  }
  return best_encoding_indices;
}

static vector<Alp::IndicesAppearancesCombination> getTopKCombinations(btrblocks::DoubleStats& stats) {
  tuple<vector<DOUBLE>, vector<BITMAP>> sample_tuple = stats.samples(RG_SAMPLES, SAMPLES_PER_VECTOR);
  vector<DOUBLE>& sampleCon = std::get<0>(sample_tuple);

  vector<vector<DOUBLE>> vectors_sampled;

  for (size_t t = 0; t < sampleCon.size();) {
    size_t upper_limit = t == RG_SAMPLES - 1 ? sampleCon.size() : t + SAMPLES_PER_VECTOR;
    vectors_sampled.emplace_back(&sampleCon[t], &sampleCon[upper_limit]);
    t += SAMPLES_PER_VECTOR;
  }

  return FindTopKCombinations(vectors_sampled);
}

// -------------------------------------------------------------------------------------
// compression
// -------------------------------------------------------------------------------------
u32 Alp::compress(const DOUBLE* src,
             const BITMAP* nullmap,
             u8* dest,
             DoubleStats& stats,
             u8 allowed_cascading_level) {
  auto& col_struct = *reinterpret_cast<AlpStructure*>(dest);

  vector<int64_t> encoded_integers(stats.tuple_count);
  vector<INTEGER> exceptions_positions;
  // first we need to find the best exponent and factor
  auto best_k_combinations = getTopKCombinations(stats);
  col_struct.vector_encoding_indices = FindBestFactorAndExponent(src, stats.tuple_count, best_k_combinations);
  Alp::EncodingIndices& vector_encoding_indices = col_struct.vector_encoding_indices;

  std::cout << "vector_encoding_indices exp: " << static_cast<unsigned>(vector_encoding_indices.exponent) << " factor: " << static_cast<unsigned>(vector_encoding_indices.factor) << '\n';


  // Encoding Floating-Point to Int64
  //! We encode all the values regardless of their correctness to recover the original floating-point
  for (size_t i = 0; i < stats.tuple_count; i++) {
    DOUBLE actual_value = src[i];
    int64_t encoded_value = EncodeValue(actual_value, vector_encoding_indices);
    DOUBLE decoded_value = DecodeValue(encoded_value, vector_encoding_indices);
    // TODO: THIS DOESNT SEEM TO BE VERY EFFICIENT (DUCKDB DOES THIS BETTER)
    encoded_integers[i] = decoded_value != actual_value ? 0 : encoded_value;
    //! We detect exceptions using a predicated comparison
    auto is_exception = (decoded_value != actual_value);
    if (is_exception) {
      exceptions_positions.push_back(static_cast<INTEGER>(i));
    }
  }
  col_struct.encoded_count = stats.tuple_count;

  // Finding first non exception value
  int64_t a_non_exception_value = 0;
  for (size_t i = 0; i < exceptions_positions.size(); i++) {
    if (i != exceptions_positions[i]) {
      a_non_exception_value = encoded_integers[i];
      break;
    }
  }

  vector<DOUBLE> exceptions(exceptions_positions.size());

  // Replacing that first non exception value on the vector exceptions
  for (size_t i = 0; i < exceptions.size(); i++) {
    size_t exception_pos = exceptions_positions[i];
    DOUBLE actual_value = src[exception_pos];
    encoded_integers[exception_pos] = a_non_exception_value;
    exceptions[i] = actual_value;
  }

  col_struct.exceptions_count = exceptions.size();

  auto write_ptr = col_struct.data;

  // compress encoded_integers with special shit
  if  (!encoded_integers.empty()) {
    u32 used_space;
    /// statically setting bitpacking here; test better options
    Int64SchemePicker::compress(
        reinterpret_cast<int64_t*>(encoded_integers.data()), nullptr, write_ptr, encoded_integers.size(), allowed_cascading_level - 1,
        used_space, col_struct.encoding_scheme, autoScheme(), "alp encoded integers");
    write_ptr += used_space;
    Log::debug("Alp right c = {} s = {}", CI(col_struct.encoding_scheme), CI(used_space));
  }

  // compress patches
  col_struct.exceptions_positions_offset = write_ptr - col_struct.data;
  {
    u32 used_space;
    IntegerSchemePicker::compress(reinterpret_cast<INTEGER*>(exceptions_positions.data()), nullptr,
                                  write_ptr, exceptions_positions.size(), allowed_cascading_level - 1,
                                  used_space, col_struct.exceptions_positions_scheme, autoScheme(),
                                  "patches");
    write_ptr += used_space;
    Log::debug("Alp patches c = {} s = {}", CI(col_struct.exceptions_positions_scheme), CI(used_space));
  }

  col_struct.exceptions_offset = write_ptr - col_struct.data;
  if  (!exceptions.empty()) {
    u32 used_space;
    /// statically setting bitpacking here; test better options
    DoubleSchemePicker::compress(
        reinterpret_cast<DOUBLE*>(exceptions.data()), nullptr, write_ptr, exceptions.size(), allowed_cascading_level - 1,
        used_space, col_struct.exceptions_scheme, autoScheme(), "alp encoded integers");
    write_ptr += used_space;
    Log::debug("Alp exceptions c = {} s = {}", CI(col_struct.exceptions_scheme), CI(used_space));
  }

  return write_ptr - dest;
}
// -------------------------------------------------------------------------------------
// decompression
// -------------------------------------------------------------------------------------
void Alp::decompress(DOUBLE* dest,
                BitmapWrapper* bitmap,
                const u8* src,
                u32 tuple_count,
                u32 level) {
  const auto& col_struct = *reinterpret_cast<const AlpStructure*>(src);
  auto* exact_dest = reinterpret_cast<DOUBLE*>(dest);

  // decompression memory
  thread_local vector<vector<int64_t>> encoded_integer_v;
  thread_local vector<vector<DOUBLE>> exceptions_v;
  thread_local vector<vector<INTEGER>> patches_v;
  auto encoded_integer_ptr = get_level_data(encoded_integer_v, col_struct.encoded_count, level);
  auto exceptions_ptr = get_level_data(exceptions_v, col_struct.exceptions_count, level);
  auto patches_ptr = get_level_data(patches_v, col_struct.exceptions_count, level);

  Alp::EncodingIndices encodingIndices = col_struct.vector_encoding_indices;

  // unconditionally decompress dictionary and right part
  Int64Scheme& encoded_scheme =
      Int64SchemePicker::MyTypeWrapper::getScheme(col_struct.encoding_scheme);
  encoded_scheme.decompress(encoded_integer_v[level].data(), nullptr, col_struct.data,
                            col_struct.encoded_count, level + 1);

  IntegerScheme& exception_positions_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.exceptions_positions_scheme);
  exception_positions_scheme.decompress(patches_v[level].data(), nullptr, col_struct.data + col_struct.exceptions_positions_offset,
                                        col_struct.exceptions_count, level + 1);

  DoubleScheme& exceptions_scheme =
      DoubleSchemePicker::MyTypeWrapper::getScheme(col_struct.exceptions_scheme);
  exceptions_scheme.decompress(exceptions_v[level].data(), nullptr, col_struct.data + col_struct.exceptions_offset,
                               col_struct.exceptions_count, level + 1);

  // TODO: THIS SHOULD ALL BE SIMDED
  // https://godbolt.org/z/83YWW6K7x

  // static DOUBLE DecodeValue(int64_t encoded_value, Alp::EncodingIndices encoding_indices) {
  //   //! The cast to T is needed to prevent a signed integer overflow
  //   DOUBLE decoded_value = static_cast<DOUBLE>(encoded_value * FACT_ARR[encoding_indices.factor]) *
  //                          FRAC_ARR[encoding_indices.exponent];
  //   return decoded_value;
  // }

  // decompress left part if not everything is a patch
  if (col_struct.encoded_count > 0) {
    // TODO: MAYBE THINK ABOUT USING ALIGNED LOADS
    // Decode
    auto write_ptr = dest;
    auto read_ptr = encoded_integer_ptr;
    auto target_ptr = exact_dest + col_struct.encoded_count;

    while (write_ptr < target_ptr) {
      __m256i encoded_integers = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(read_ptr));
      __m256d decoded_values = DecodeValue(encoded_integers, encodingIndices);
      _mm256_storeu_pd(write_ptr, decoded_values);

      write_ptr += 4;
      read_ptr += 4;
    }
  }

  // patches
  // TODO: SCATTER/GATHER WOULD BE NEEDED HERE NOT AVAILABLE IN AVX2
  for (u32 i = 0; i != col_struct.exceptions_count; i++) {
    exact_dest[patches_ptr[i]] = exceptions_ptr[i];
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
