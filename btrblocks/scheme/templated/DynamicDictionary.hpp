#pragma once
#include "common/Utils.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
struct __attribute__((packed)) DynamicDictionaryStructure {
  u8 codes_scheme_code;
  u32 codes_offset;
  u8 data[];
};
// -------------------------------------------------------------------------------------
// Used for integer and doubles
template <typename NumberType, typename SchemeType, typename StatsType, typename SchemeCodeType>
class TDynamicDictionary {
 public:
  // -------------------------------------------------------------------------------------
  static inline double expectedCompressionRatio(StatsType& stats, u8 allowed_cascading_level) {
    if (allowed_cascading_level <= 1) {
      return 0;
    }
    u32 after_size = ((stats.unique_count * sizeof(NumberType)) +
                      (stats.tuple_count * (Utils::getBitsNeeded(stats.unique_count) / 8.0)));
    after_size += sizeof(DynamicDictionaryStructure) + 5;  // 5 for PBP header
    after_size += (stats.tuple_count) * 2 / 128;  // TODO: find out the overhead of FastPFOR
    return CD(stats.total_size) / CD(after_size);
  }
  // -------------------------------------------------------------------------------------
  static inline u32 compressColumn(const NumberType* src,
                                   const BITMAP*,
                                   u8* dest,
                                   StatsType& stats,
                                   u8 allowed_cascading_level) {
    // Layout: DICT | CODES
    auto& col_struct = *reinterpret_cast<DynamicDictionaryStructure*>(dest);
    // -------------------------------------------------------------------------------------
    // Write dictionary
    auto dict_slots = reinterpret_cast<NumberType*>(col_struct.data);
    u32 distinct_i = 0;
    for (const auto& distinct_element : stats.distinct_values) {
      dict_slots[distinct_i] = distinct_element.first;
      distinct_i++;
    }
    // -------------------------------------------------------------------------------------
    auto dict_begin = reinterpret_cast<NumberType*>(col_struct.data);
    auto dict_end = reinterpret_cast<NumberType*>(col_struct.data) + distinct_i;
    // -------------------------------------------------------------------------------------
    vector<INTEGER> codes;
    for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
      auto it = std::lower_bound(dict_begin, dict_end, src[row_i]);
      if (it == dict_end) {
        die_if(stats.distinct_values.find(src[row_i]) != stats.distinct_values.end());
      }
      die_if(it != dict_end);
      codes.push_back(std::distance(dict_begin, it));
    }
    // -------------------------------------------------------------------------------------
    // Compress codes
    auto write_ptr = reinterpret_cast<u8*>(dict_end);
    col_struct.codes_offset = write_ptr - col_struct.data;
    u32 used_space;
    // For Number dictionaries, we only need FBP/PBP for coding, if any other
    // schemes was useful beneath, it had to be rather chosen instead of X_DICT
    // in the first place.
    IntegerSchemePicker::compress(codes.data(), nullptr, write_ptr, codes.size(),
                                  allowed_cascading_level - 1, used_space,
                                  col_struct.codes_scheme_code, CB(IntegerSchemeType::BP), "codes");
    // -------------------------------------------------------------------------------------
    Log::debug("X_DICT: codes_c = {} codes_s = {}", CI(col_struct.codes_scheme_code),
               CI(used_space));
    // -------------------------------------------------------------------------------------
    write_ptr += used_space;
    // -------------------------------------------------------------------------------------
    return write_ptr - dest;
  }
  // -------------------------------------------------------------------------------------
  static inline void decompressColumn(NumberType* dest,
                                      BitmapWrapper*,
                                      const u8* src,
                                      u32 tuple_count,
                                      u32 level) {
    auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
    // -------------------------------------------------------------------------------------
    // Decode codes
    thread_local std::vector<std::vector<INTEGER>> codes_v;
    auto codes = get_level_data(codes_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
    IntegerScheme& scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
    scheme.decompress(codes, nullptr, col_struct.data + col_struct.codes_offset, tuple_count,
                      level + 1);
    // -------------------------------------------------------------------------------------
    auto dict = reinterpret_cast<const NumberType*>(col_struct.data);
    for (u32 i = 0; i < tuple_count; i++) {
      dest[i] = dict[codes[i]];
    }
  }
  // -------------------------------------------------------------------------------------
  static inline string fullDescription(const u8* src, const string& selfDescription) {
    auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
    IntegerScheme& scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
    return selfDescription + " -> ([int] codes) " +
           scheme.fullDescription(col_struct.data + col_struct.codes_offset);
  }
};

template <>
inline void TDynamicDictionary<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>::
    decompressColumn(INTEGER* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  BTR_IFSIMD({
    static_assert(sizeof(*dest) == 4);
    static_assert(SIMD_EXTRA_BYTES >= 4 * sizeof(__m256i));
  })

  auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);

  // Decode codes
  thread_local std::vector<std::vector<INTEGER>> codes_v;
  auto codes = get_level_data(codes_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  IntegerScheme& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
  scheme.decompress(codes, nullptr, col_struct.data + col_struct.codes_offset, tuple_count,
                    level + 1);

  auto dict = reinterpret_cast<const INTEGER*>(col_struct.data);
  u32 i = 0;
#ifdef BTR_USE_SIMD
  if (tuple_count >= 32) {
    while (i < tuple_count - 31) {
      // Load codes.
      __m256i codes_0 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(codes + 0));
      __m256i codes_1 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(codes + 8));
      __m256i codes_2 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(codes + 16));
      __m256i codes_3 = _mm256_loadu_si256(reinterpret_cast<__m256i*>(codes + 24));

      // Gather values.
      __m256i values_0 = _mm256_i32gather_epi32(dict, codes_0, 4);
      __m256i values_1 = _mm256_i32gather_epi32(dict, codes_1, 4);
      __m256i values_2 = _mm256_i32gather_epi32(dict, codes_2, 4);
      __m256i values_3 = _mm256_i32gather_epi32(dict, codes_3, 4);

      // store values
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + 0), values_0);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + 8), values_1);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + 16), values_2);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest + 24), values_3);

      dest += 32;
      codes += 32;
      i += 32;
    }
  }
#endif

  while (i < tuple_count) {
    *dest++ = dict[*codes++];
    i++;
  }
}

template <>
inline void TDynamicDictionary<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>::
    decompressColumn(DOUBLE* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  BTR_IFSIMD({
    static_assert(sizeof(*dest) == 8);
    static_assert(SIMD_EXTRA_BYTES >= 4 * sizeof(__m256d));
  })

  auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);

  // Decode codes
  thread_local std::vector<std::vector<INTEGER>> codes_v;
  auto codes = get_level_data(codes_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
  IntegerScheme& scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme_code);
  scheme.decompress(codes, nullptr, col_struct.data + col_struct.codes_offset, tuple_count,
                    level + 1);

  auto dict = reinterpret_cast<const DOUBLE*>(col_struct.data);
  u32 i = 0;
#ifdef BTR_USE_SIMD
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
  if (tuple_count >= 16) {
    while (i < tuple_count - 15) {
      // Load codes
      __m128i codes_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(codes + 0));
      __m128i codes_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(codes + 4));
      __m128i codes_2 = _mm_loadu_si128(reinterpret_cast<__m128i*>(codes + 8));
      __m128i codes_3 = _mm_loadu_si128(reinterpret_cast<__m128i*>(codes + 12));

      // gather values
      __m256d values_0 = _mm256_i32gather_pd(dict, codes_0, 8);
      __m256d values_1 = _mm256_i32gather_pd(dict, codes_1, 8);
      __m256d values_2 = _mm256_i32gather_pd(dict, codes_2, 8);
      __m256d values_3 = _mm256_i32gather_pd(dict, codes_3, 8);

      // store values
      _mm256_storeu_pd(dest + 0, values_0);
      _mm256_storeu_pd(dest + 4, values_1);
      _mm256_storeu_pd(dest + 8, values_2);
      _mm256_storeu_pd(dest + 12, values_3);

      dest += 16;
      codes += 16;
      i += 16;
    }
  }
#pragma GCC diagnostic pop
#endif

  while (i < tuple_count) {
    *dest++ = dict[*codes++];
    i++;
  }
}
}  // namespace btrblocks
