// ------------------------------------------------------------------------------
#include "Pseudodecimal.hpp"
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
// -------------------------------------------------------------------------------------
namespace btrblocks::doubles {

const u32 max_exponent = 22;
const u8 exponent_exception_code = 23;
const u8 decimal_index_mask = 0x1F;
static const double exact_fractions_of_ten[] = {
    1.0,
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
    0.00000000000000000001,
    0.000000000000000000001,
    0.0000000000000000000001,
};
static_assert(sizeof(exact_fractions_of_ten) == sizeof(double) * 23);

// Naive implementation
/*
 * habe deine Methode erst in scalar ausprobiert und es funktioniert. wir können
doubles mit 9 significant digits (manchmal 10 auch) in 32 uinteger + 4 bits
(oder byte) for exponent, speichern (oder 4 bits vom uint32 für exponent
ausleihen und nur bis 8 significant digits unterstützen, ich habe noch nicht
genau überlegt wie wir es weiter optimieren können) und im Dataset gibt es
anscheinend Spalten wo die doubles nur in diesem Range liegen.
 */
// -------------------------------------------------------------------------------------
u32 Decimal::compress(const DOUBLE* src,
                      const BITMAP*,
                      u8* dest,
                      DoubleStats& stats,
                      u8 allowed_cascading_level) {
  auto significant_digits_bits_limit =
      SchemeConfig::get().doubles.pseudodecimal_significant_digit_bits_limits;
  // Layout : Header | numbers_v | exponent_v | patches_v
  // ignore bitmap
  auto& col_struct = *reinterpret_cast<DecimalStructure*>(dest);
  col_struct.variant_selector = 0;
  vector<INTEGER> numbers_v;
  vector<INTEGER> exponent_v;
  vector<DOUBLE> patches_v;  // patches

  u32 exception_count = 0;
  u8 run_count = 0;

  Roaring exceptions_bitmap;
  const u32 num_blocks = (stats.tuple_count + (block_size - 1)) / block_size;
  for (u32 block_i = 0; block_i < num_blocks; block_i++) {
    bool block_has_exception = false;

    const u32 row_start_i = block_i * block_size;
    const u32 row_end_i = std::min(row_start_i + block_size, stats.tuple_count);
    for (u32 row_i = row_start_i; row_i < row_end_i; row_i++) {
      DOUBLE current_double = src[row_i];

      bool convertable = false;
      u32 exponent;
      u64 converted_number;
      if (current_double == -0.0 && std::signbit(current_double)) {
        // Special case -0.0 is handled as exception
        exponent = exponent_exception_code;
      } else {
        // Attempt conversion
        for (exponent = 0; exponent <= max_exponent; exponent++) {
          DOUBLE cd = current_double / exact_fractions_of_ten[exponent];
          cd = std::round(cd);
          converted_number = static_cast<u64>(cd);
          DOUBLE if_converted_back =
              static_cast<DOUBLE>(converted_number) * exact_fractions_of_ten[exponent];
          if (if_converted_back == current_double &&
              ((std::floor(std::log2(converted_number)) + 1) <= significant_digits_bits_limit)) {
            convertable = true;
            break;
          }
        }
      }

      // Write result
      if (convertable) {
        die_if((std::floor(std::log2(converted_number)) + 1) <= 31);
        exponent_v.push_back(static_cast<INTEGER>(exponent));
        numbers_v.push_back(static_cast<INTEGER>(converted_number));
      } else {
        block_has_exception = true;
        exception_count++;
        if (exception_count > stats.tuple_count / 2) {
          // This is a hacky way to avoid using Decimal in columns where there
          // are many exceptions Return a big number will make the selection
          // process select uncompressed rather than Decimal
          return stats.total_size + 1000;
        }
        exponent_v.push_back(exponent_exception_code);
        patches_v.push_back(src[row_i]);
      }
    }

    if (block_has_exception) {
      run_count = 0;
      exceptions_bitmap.add(block_i);
    } else {
      run_count++;
      col_struct.variant_selector |= do_iteration;
      if (run_count >= 4) {
        col_struct.variant_selector |= do_unroll;
      }
    }
  }

  col_struct.converted_count = numbers_v.size();
  auto write_ptr = col_struct.data;

  // Compress significant digits
  if (!numbers_v.empty()) {
    u32 used_space;
    IntegerSchemePicker::compress(numbers_v.data(), nullptr, write_ptr, numbers_v.size(),
                                  allowed_cascading_level - 1, used_space,
                                  col_struct.numbers_scheme, autoScheme(), "significant digits");
    write_ptr += used_space;
    Log::debug("Decimal: sd_c = {} sd_s = {}", CI(col_struct.numbers_scheme), CI(used_space));
  }

  // Compress exponents
  {
    col_struct.exponents_offset = write_ptr - col_struct.data;
    u32 used_space;
    SInteger32Stats e_stats =
        SInteger32Stats::generateStats(exponent_v.data(), nullptr, exponent_v.size());
    // cout << e_stats.min << '\t' << e_stats.max << endl;
    IntegerSchemePicker::compress(exponent_v.data(), nullptr, write_ptr, exponent_v.size(),
                                  allowed_cascading_level - 1, used_space,
                                  col_struct.exponents_scheme, autoScheme(), "exponents");
    write_ptr += used_space;
    Log::debug("Decimal: e_c = {} e_s = {}", CI(col_struct.exponents_scheme), CI(used_space));
  }

  // Compress patches
  {
    col_struct.patches_offset = write_ptr - col_struct.data;
    u32 used_space;
    DoubleSchemePicker::compress(patches_v.data(), nullptr, write_ptr, patches_v.size(),
                                 allowed_cascading_level - 1, used_space, col_struct.patches_scheme,
                                 autoScheme(), "patches");
    write_ptr += used_space;
    Log::debug("Decimal: p_c = {} p_s = {}", CI(col_struct.patches_scheme), CI(used_space));
  }

  // Write exceptions bitmap
  {
    col_struct.exceptions_map_offset = write_ptr - col_struct.data;
    exceptions_bitmap.runOptimize();
    exceptions_bitmap.setCopyOnWrite(true);
    write_ptr += exceptions_bitmap.write(reinterpret_cast<char*>(write_ptr), false);
  }

  return write_ptr - dest;
}

struct DecimalIterateParam {
  u32 next_block_i;
  u32 tuple_count;
  DOUBLE* write_ptr;
  INTEGER* exponents_ptr;
  INTEGER* numbers_ptr;
  DOUBLE* patches_ptr;
};

static inline void decompressExceptionBlock(DecimalIterateParam* param) {
  u32 row_start_i = param->next_block_i * block_size;
  u32 row_end_i = std::min(row_start_i + block_size, param->tuple_count);
  for (u32 row_i = row_start_i; row_i < row_end_i; row_i++) {
    INTEGER exponent = *param->exponents_ptr++;
    if (exponent == exponent_exception_code) {
      *param->write_ptr++ = *param->patches_ptr++;
    } else {
      auto number = *param->numbers_ptr++;
      u8 exponent_index = exponent & decimal_index_mask;
      DOUBLE original_double = static_cast<DOUBLE>(number) * exact_fractions_of_ten[exponent_index];
      *param->write_ptr++ = original_double;
    }
  }
  param->next_block_i++;
}

#ifdef BTR_USE_SIMD
static inline void decompressAVXBlock4(DecimalIterateParam* param) {
  // Load numbers and convert to double
  __m128i numbers_int_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 0);
  __m128i numbers_int_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 1);
  __m128i numbers_int_2 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 2);
  __m128i numbers_int_3 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 3);

  __m256d numbers_double_0 = _mm256_cvtepi32_pd(numbers_int_0);
  __m256d numbers_double_1 = _mm256_cvtepi32_pd(numbers_int_1);
  __m256d numbers_double_2 = _mm256_cvtepi32_pd(numbers_int_2);
  __m256d numbers_double_3 = _mm256_cvtepi32_pd(numbers_int_3);

  // Load exponents and gather the power of ten
  __m128i exponents_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 0);
  __m128i exponents_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 1);
  __m128i exponents_2 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 2);
  __m128i exponents_3 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 3);

  // Gather powers
  __m256d powers_0 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_0, 8);
  __m256d powers_1 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_1, 8);
  __m256d powers_2 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_2, 8);
  __m256d powers_3 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_3, 8);

  // Perform division
  __m256d results_0 = _mm256_mul_pd(numbers_double_0, powers_0);
  __m256d results_1 = _mm256_mul_pd(numbers_double_1, powers_1);
  __m256d results_2 = _mm256_mul_pd(numbers_double_2, powers_2);
  __m256d results_3 = _mm256_mul_pd(numbers_double_3, powers_3);

  // Store result
  _mm256_storeu_pd(param->write_ptr + 0, results_0);
  _mm256_storeu_pd(param->write_ptr + 4, results_1);
  _mm256_storeu_pd(param->write_ptr + 8, results_2);
  _mm256_storeu_pd(param->write_ptr + 12, results_3);

  param->write_ptr += 16;
  param->exponents_ptr += 16;
  param->numbers_ptr += 16;
  param->next_block_i += 4;
}

static inline void decompressAVXBlock2(DecimalIterateParam* param) {
  // Load numbers and convert to double
  __m128i numbers_int_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 0);
  __m128i numbers_int_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr) + 1);

  __m256d numbers_double_0 = _mm256_cvtepi32_pd(numbers_int_0);
  __m256d numbers_double_1 = _mm256_cvtepi32_pd(numbers_int_1);

  // Load exponents and gather the power of ten
  __m128i exponents_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 0);
  __m128i exponents_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->exponents_ptr) + 1);

  // Gather powers
  __m256d powers_0 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_0, 8);
  __m256d powers_1 = _mm256_i32gather_pd(exact_fractions_of_ten, exponents_1, 8);

  // Perform division
  __m256d results_0 = _mm256_mul_pd(numbers_double_0, powers_0);
  __m256d results_1 = _mm256_mul_pd(numbers_double_1, powers_1);

  // Store result
  _mm256_storeu_pd(param->write_ptr + 0, results_0);
  _mm256_storeu_pd(param->write_ptr + 4, results_1);

  param->write_ptr += 8;
  param->exponents_ptr += 8;
  param->numbers_ptr += 8;
  param->next_block_i += 2;
}

static inline void decompressAVXBlock1(DecimalIterateParam* param) {
  // Load numbers and convert to double
  __m128i numbers_int = _mm_loadu_si128(reinterpret_cast<__m128i*>(param->numbers_ptr));
  __m256d numbers_double = _mm256_cvtepi32_pd(numbers_int);

  // Load exponents and gather the power of ten
  //__m128i exponents = _mm_loadu_si128(reinterpret_cast<__m128i
  //*>(param->exponents_ptr));
  // gather seems to be the bottleneck
  // _mm256_exp10_pd (SVML: Only supported by Intel Compiler) (Sequential!)
  // _mm256_round_pd
  //__m256d powers = _mm256_i32gather_pd(exact_fractions_of_ten, exponents,
  // sizeof(*exact_fractions_of_ten));

  // Use a simple set instead of load + gather
  __m256d powers = _mm256_set_pd(exact_fractions_of_ten[param->exponents_ptr[3]],
                                 exact_fractions_of_ten[param->exponents_ptr[2]],
                                 exact_fractions_of_ten[param->exponents_ptr[1]],
                                 exact_fractions_of_ten[param->exponents_ptr[0]]);

  // Perform division
  __m256d results = _mm256_mul_pd(numbers_double, powers);

  // Store result
  _mm256_storeu_pd(param->write_ptr, results);

  param->write_ptr += 4;
  param->exponents_ptr += 4;
  param->numbers_ptr += 4;
  param->next_block_i++;
}

static inline void decompressAVXBlockUnroll(DecimalIterateParam* param, uint32_t limit) {
#if 1
  auto unroll_limit = limit < 3 ? 0 : limit - 3;
  while (param->next_block_i < unroll_limit) {
    decompressAVXBlock4(param);
  }
#else
  auto unroll_limit = limit < 1 ? 0 : limit - 1;
  while (param->next_block_i < unroll_limit) {
    decompressAVXBlock2(param);
  }
#endif

  while (param->next_block_i < limit) {
    decompressAVXBlock1(param);
  }

  // Write block with exception
  decompressExceptionBlock(param);
}

static inline void decompressAVXBlock(DecimalIterateParam* param, uint32_t limit) {
  while (param->next_block_i < limit) {
    decompressAVXBlock1(param);
  }

  // Write block with exception
  decompressExceptionBlock(param);
}
#endif  // BTR_USE_SIMD

void Decimal::decompress(DOUBLE* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  // idea save exceptions in roaring in blocks of 4.
  // iterate over roaring. for everything up to the value us vectorized
  // implementation for anything value itself use non-vectorized impl don't
  // forget last block

  const auto& col_struct = *reinterpret_cast<const DecimalStructure*>(src);
  thread_local std::vector<std::vector<INTEGER>> numbers_v;
  auto numbers_ptr =
      get_level_data(numbers_v, col_struct.converted_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  thread_local std::vector<std::vector<INTEGER>> exponents_v;
  auto exponents_ptr =
      get_level_data(exponents_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  thread_local std::vector<std::vector<DOUBLE>> patches_v;
  auto patches_ptr = get_level_data(
      patches_v, tuple_count - col_struct.converted_count + SIMD_EXTRA_ELEMENTS(DOUBLE), level);
  Roaring exceptions_bitmap = Roaring::read(
      reinterpret_cast<const char*>(col_struct.data + col_struct.exceptions_map_offset), false);

  if (col_struct.converted_count > 0) {
    IntegerScheme& numbers_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.numbers_scheme);
    numbers_scheme.decompress(numbers_v[level].data(), nullptr, col_struct.data,
                              col_struct.converted_count, level + 1);
  }
  IntegerScheme& exponents_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.exponents_scheme);
  exponents_scheme.decompress(exponents_v[level].data(), nullptr,
                              col_struct.data + col_struct.exponents_offset, tuple_count,
                              level + 1);

  DoubleScheme& patches_scheme =
      DoubleSchemePicker::MyTypeWrapper::getScheme(col_struct.patches_scheme);
  patches_scheme.decompress(patches_v[level].data(), nullptr,
                            col_struct.data + col_struct.patches_offset,
                            tuple_count - col_struct.converted_count, level + 1);

#ifdef BTR_USE_SIMD
  if (col_struct.variant_selector & do_iteration) {
    struct DecimalIterateParam param = {
        .next_block_i = 0,
        .tuple_count = tuple_count,
        .write_ptr = dest,
        .exponents_ptr = exponents_ptr,
        .numbers_ptr = numbers_ptr,
        .patches_ptr = patches_ptr,
    };
    const u32 num_avx_blocks =
        tuple_count / block_size;  // The number of blocks that are complete (have 4 values)

    // if (col_struct.variant_selector & do_unroll) {
    if (col_struct.variant_selector & do_unroll) {
      exceptions_bitmap.iterate(
          [](uint32_t value, void* param_void) {
            auto param = reinterpret_cast<struct DecimalIterateParam*>(param_void);
            decompressAVXBlockUnroll(param, value);
            return true;
          },
          &param);

      // Write remaining blocks
      decompressAVXBlockUnroll(&param, num_avx_blocks);

    } else {
      exceptions_bitmap.iterate(
          [](uint32_t value, void* param_void) {
            auto param = reinterpret_cast<struct DecimalIterateParam*>(param_void);
            decompressAVXBlock(param, value);
            return true;
          },
          &param);

      // Write remaining blocks
      decompressAVXBlock(&param, num_avx_blocks);
    }
  } else {
    auto write_ptr = dest;
    for (u32 row_i = 0; row_i < tuple_count; row_i++) {
      INTEGER exponent = *exponents_ptr++;
      if (exponent == exponent_exception_code) {
        *write_ptr++ = *patches_ptr++;
      } else {
        auto number = *numbers_ptr++;
        u8 exponent_index = exponent & decimal_index_mask;
        DOUBLE original_double =
            static_cast<DOUBLE>(number) * exact_fractions_of_ten[exponent_index];
        *write_ptr++ = original_double;
      }
    }
  }
#else  // don't use SIMD
  auto write_ptr = dest;
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    INTEGER exponent = *exponents_ptr++;
    if (exponent == exponent_exception_code) {
      *write_ptr++ = *patches_ptr++;
    } else {
      auto number = *numbers_ptr++;
      u8 exponent_index = exponent & decimal_index_mask;
      DOUBLE original_double = static_cast<DOUBLE>(number) * exact_fractions_of_ten[exponent_index];
      *write_ptr++ = original_double;
    }
  }
#endif
}

string Decimal::fullDescription(const u8* src) {
  const auto& col_struct = *reinterpret_cast<const DecimalStructure*>(src);
  string result = this->selfDescription();

  if (col_struct.converted_count > 0) {
    IntegerScheme& sd_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.numbers_scheme);
    result += "\n\t-> ([int] significant digits) " + sd_scheme.fullDescription(col_struct.data);
  }

  IntegerScheme& e_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.exponents_scheme);
  result += "\n\t-> ([int] exponents) " +
            e_scheme.fullDescription(col_struct.data + col_struct.exponents_offset);

  DoubleScheme& p_scheme = DoubleSchemePicker::MyTypeWrapper::getScheme(col_struct.patches_scheme);
  result += "\n\t-> ([double] patches) " +
            p_scheme.fullDescription(col_struct.data + col_struct.patches_offset);

  return result;
}

bool Decimal::isUsable(DoubleStats& stats) {
  double unique_ratio =
      static_cast<double>(stats.unique_count) / static_cast<double>(stats.tuple_count);
  if (unique_ratio < 0.1) {
    return false;
  }
  return true;
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::doubles
// -------------------------------------------------------------------------------------
