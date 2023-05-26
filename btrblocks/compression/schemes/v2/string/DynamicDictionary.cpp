#include "DynamicDictionary.hpp"
#include "common/Units.hpp"
#include "compression/schemes/CSchemePicker.hpp"
#include "compression/schemes/v2/integer/PBP.hpp"
#include "compression/schemes/v2/integer/RLE.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
#include "fsst.h"
#include "gflags/gflags.h"
#include "storage/StringPointerArrayViewer.hpp"
// -------------------------------------------------------------------------------------
#include <cmath>
// -------------------------------------------------------------------------------------
DECLARE_string(fsst_stats);
namespace btrblocks::db::v2::string {
// -------------------------------------------------------------------------------------
DEFINE_bool(string_dictionary_use_fsst, true, "");
DEFINE_uint32(fsst_force_codes_scheme, AUTO_SCHEME, "");
DEFINE_uint32(fsst_input_size_threshold, FSST_THRESHOLD, "");
DEFINE_uint32(fsst_codes_max_cascading_level, 2, "");
// -------------------------------------------------------------------------------------
/*
 * Plan:
 * Output only 32-bits codes, hence no need for templates
 * compress distinct strings with fsst
 */
// -------------------------------------------------------------------------------------
double DynamicDictionary::expectedCompressionRatio(StringStats& stats, u8) {
  u32 bits_per_code = std::floor(std::log2(stats.distinct_values.size())) + 1;
  u32 after_size = sizeof(DynamicDictionaryStructure);
  after_size += FSST_MAXHEADER;
  after_size += stats.tuple_count * (CD(bits_per_code) / 8.0);
  after_size += sizeof(StringArrayViewer::Slot) * (1 + stats.distinct_values.size());
  if (stats.total_unique_length >= FLAGS_fsst_input_size_threshold) {    // Threshold
    after_size += 0.5 * static_cast<double>(stats.total_unique_length);  // 0.5 for fsst
  } else {
    after_size += static_cast<double>(stats.total_unique_length);
  }
  return CD(stats.total_size) / CD(after_size);
}
// -------------------------------------------------------------------------------------
bool DynamicDictionary::usesFsst(const u8* src) {
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
  return col_struct.use_fsst;
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::compress(const btrblocks::StringArrayViewer src,
                                const BITMAP*,
                                u8* dest,
                                btrblocks::db::StringStats& stats) {
  // Layout: FSST_DICT | FSST_STRINGS | FSST_OFFSETS@from FSST_STRINGS
  // BEGINNING@ | Compressed(Codes) OR :    DICT | OFFSETS | Compressed(Codes)
  // -------------------------------------------------------------------------------------
  die_if(stats.unique_count <= std::numeric_limits<u32>::max());
  // -------------------------------------------------------------------------------------
  auto& col_struct = *reinterpret_cast<DynamicDictionaryStructure*>(dest);
  col_struct.use_fsst = FLAGS_string_dictionary_use_fsst &&
                        (stats.total_unique_length >= FLAGS_fsst_input_size_threshold);
  if (FLAGS_fsst_stats != "") {
    col_struct.use_fsst = true;  // we are experimeneting with FSST
  }
  col_struct.total_decompressed_size = stats.total_size;
  col_struct.num_codes = stats.distinct_values.size();
  auto write_ptr = col_struct.data;
  // IDEA: sort distinct_values ascending by number of occurences to reduce the
  // numbers of bits required for codes
  vector<str> distinct_values(stats.distinct_values.begin(), stats.distinct_values.end());
  // -------------------------------------------------------------------------------------
  // FSST Compression
  // -------------------------------------------------------------------------------------
  Log::debug("FSST: using_fsst = {}", static_cast<s64>(col_struct.use_fsst));
  // -------------------------------------------------------------------------------------
  u64 fsst_strings_used_space;
  if (col_struct.use_fsst) {
    ThreadCache::setFsst();
    // -------------------------------------------------------------------------------------
    const auto fsst_n = stats.unique_count;
    auto input_string_buffers = std::unique_ptr<u8*[]>(new u8*[fsst_n]);
    auto input_string_lengths = std::unique_ptr<u64[]>(new u64[fsst_n]);
    auto output_string_buffers = std::unique_ptr<u8*[]>(new u8*[fsst_n]);
    auto output_string_lengths = std::unique_ptr<u64[]>(new u64[fsst_n]);
    // -------------------------------------------------------------------------------------
    u32 str_i = 0;
    for (const auto& distinct_str : distinct_values) {
      input_string_buffers[str_i] =
          const_cast<u8*>(reinterpret_cast<const u8*>(distinct_str.data()));
      input_string_lengths[str_i] = distinct_str.size();
      str_i++;
    }
    // -------------------------------------------------------------------------------------
    // Encoder
    // TODO: use memcpy instead of export/import (Note: I still use
    // FSST_MAXHEADER ~2KiB )
    fsst_encoder_t* encoder =
        fsst_create(fsst_n, input_string_lengths.get(), input_string_buffers.get(), 0);
    die_if(fsst_export(encoder, write_ptr) > 0);
    auto fsst_table_used_space = FSST_MAXHEADER;
    // -------------------------------------------------------------------------------------
    Log::debug("FSST: dict_s = {}", static_cast<s64>(fsst_table_used_space));
    // -------------------------------------------------------------------------------------
    write_ptr += fsst_table_used_space;
    // -------------------------------------------------------------------------------------
    // Compress
    const u64 output_buffer_size = 7 + 4 * stats.total_unique_length;  // fake
    if (fsst_compress(encoder, fsst_n, input_string_lengths.get(), input_string_buffers.get(),
                      output_buffer_size, write_ptr, output_string_lengths.get(),
                      output_string_buffers.get()) != fsst_n) {
      throw Generic_Exception("FSST Compression failed !");
    }
    fsst_strings_used_space = output_string_lengths[fsst_n - 1] +
                              (output_string_buffers[fsst_n - 1] - output_string_buffers[0]);
    // -------------------------------------------------------------------------------------
    Log::debug("FSST: strings_s = {}", static_cast<s64>(fsst_strings_used_space));
    // -------------------------------------------------------------------------------------
    Log::debug("FSST: string_pool_size : before = {} after = {}",
               static_cast<s64>(stats.total_unique_length),
               static_cast<s64>(fsst_table_used_space + fsst_strings_used_space));
    // -------------------------------------------------------------------------------------
    write_ptr += fsst_strings_used_space;
    // -------------------------------------------------------------------------------------
    // Convert destLen to offsets
    col_struct.fsst_offsets_offset = write_ptr - col_struct.data;
    auto fsst_offsets = reinterpret_cast<u32*>(write_ptr);
    u32 last_offset = 0;
    for (u32 row_i = 0; row_i < fsst_n; row_i++) {
      fsst_offsets[row_i] = last_offset;
      last_offset += output_string_lengths[row_i];
    }
    fsst_offsets[fsst_n] = last_offset;
    write_ptr += (fsst_n + 1) * sizeof(u32);
    // -------------------------------------------------------------------------------------
    // Expiremental part; check if all offsets are equal TODO
    // auto fsst_length_stats = NumberStats<u64>::generateStats(const_cast<const
    // u64*>(output_string_lengths.get()), nullptr, fsst_n); u32 min_length =
    // std::numeric_limits<u32>::max(), max_length= 0; for ( u32 row_i = 1;
    // row_i < fsst_n; row_i++ ) {
    //     if(output_string_lengths[row_i] < min_length) {
    //         min_length = output_string_lengths[row_i];
    //     } else if(output_string_lengths[row_i] > max_length) {
    //         max_length = output_string_lengths[row_i];
    //     }
    // }
    // cout <<"v -> c " << endl;
    // for(const auto &t : fsst_length_stats.distinct_values) {
    // cout << t.first << '\t' << t.second << endl;
    // }
    std::vector<INTEGER> uncompressed_lengths;
    for (u32 i = 0; i < fsst_n; i++) {
      uncompressed_lengths.push_back(static_cast<INTEGER>(input_string_lengths[i]));
    }
    col_struct.lengths_offset = write_ptr - col_struct.data;
    u32 used_space;
    IntegerSchemePicker::compress(uncompressed_lengths.data(), nullptr, write_ptr,
                                  uncompressed_lengths.size(), FLAGS_fsst_codes_max_cascading_level,
                                  used_space, col_struct.lengths_scheme,
                                  static_cast<u8>(AUTO_SCHEME));
    write_ptr += used_space;
  } else {
    auto dest_slot_ptr = reinterpret_cast<StringArrayViewer::Slot*>(col_struct.data);
    u8* str_write_ptr =
        col_struct.data + ((distinct_values.size() + 1) * sizeof(StringArrayViewer::Slot));
    for (const auto& distinct_str : distinct_values) {
      dest_slot_ptr++->offset =
          str_write_ptr - col_struct.data;  // Note, string offset is relative to the first slot
      std::memcpy(str_write_ptr, distinct_str.data(), distinct_str.length());
      str_write_ptr += distinct_str.length();
    }
    dest_slot_ptr->offset = str_write_ptr - col_struct.data;
    write_ptr = str_write_ptr;
  }
  // -------------------------------------------------------------------------------------
  // Codes
  {
    s32 run_count = 0;
    s32 current_code = -1;
    vector<INTEGER> codes;
    for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
      const str& current_value = src(row_i);
      auto it = std::lower_bound(distinct_values.begin(), distinct_values.end(), current_value);
      assert(it != distinct_values.end());
      auto new_code = static_cast<INTEGER>(std::distance(distinct_values.begin(), it));
      codes.push_back(new_code);
      if (new_code != current_code) {
        run_count++;
        current_code = new_code;
      }
    }
    double avg_run_length = static_cast<double>(stats.tuple_count) / static_cast<double>(run_count);
    auto forced_scheme = static_cast<IntegerSchemeType>(FLAGS_fsst_force_codes_scheme);
    col_struct.use_rle_optimized_path = false;
    if (avg_run_length > 3.0) {
      col_struct.use_rle_optimized_path = true;
      forced_scheme = IntegerSchemeType::RLE;
    }
    // -------------------------------------------------------------------------------------
    // Compress codes
    col_struct.codes_offset = write_ptr - col_struct.data;
    u32 used_space;
    IntegerSchemePicker::compress(codes.data(), nullptr, write_ptr, codes.size(),
                                  FLAGS_fsst_codes_max_cascading_level, used_space,
                                  col_struct.codes_scheme, static_cast<u8>(forced_scheme));
    write_ptr += used_space;
    // -------------------------------------------------------------------------------------
    Log::debug("FSST: codes_c = {} codes_s = {}", static_cast<s64>(col_struct.codes_scheme),
               static_cast<s64>(used_space));
    // -------------------------------------------------------------------------------------
  }
  // -------------------------------------------------------------------------------------
  u32 after_size = write_ptr - dest;
  ThreadCache::dumpFsst(stats.total_length, stats.total_unique_length,
                        fsst_strings_used_space + FSST_MAXHEADER, after_size);
  return after_size;
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  return reinterpret_cast<const DynamicDictionaryStructure*>(src)->total_decompressed_size;
}
// -------------------------------------------------------------------------------------
u32 DynamicDictionary::getDecompressedSizeNoCopy(const u8* src,
                                                 u32 tuple_count,
                                                 BitmapWrapper* nullmap) {
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
  // total_decompressed_size = slots_size + strings_size
  // We need to get rid of the (tuple_count + 1) slots
  auto strings_size =
      col_struct.total_decompressed_size - ((tuple_count + 1) * sizeof(StringArrayViewer::Slot));
  auto views_size = (tuple_count + 4) * sizeof(StringPointerArrayViewer::View);
  return strings_size + views_size;
}
// -------------------------------------------------------------------------------------
void DynamicDictionary::decompress(u8* dest,
                                   BitmapWrapper*,
                                   const u8* src,
                                   u32 tuple_count,
                                   u32 level) {
  // -------------------------------------------------------------------------------------
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
  const u8* compressed_codes_ptr = col_struct.data + col_struct.codes_offset;
  auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(dest);
  u32 start = (sizeof(StringArrayViewer::Slot) * (tuple_count + 1)) + SIMD_EXTRA_BYTES;
  auto dest_write_ptr = dest + start;
  // -------------------------------------------------------------------------------------
  // Decode codes
  if (col_struct.use_rle_optimized_path) {
    IntegerScheme& codes_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme);
    auto& rle = dynamic_cast<btrblocks::db::v2::integer::RLE&>(codes_scheme);

    thread_local std::vector<std::vector<INTEGER>> values_v;
    auto values_ptr = get_level_data(values_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);

    thread_local std::vector<std::vector<INTEGER>> counts_v;
    auto counts_ptr = get_level_data(counts_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);

    u32 runs_count = rle.decompressRuns(values_ptr, counts_ptr, nullptr, compressed_codes_ptr,
                                        tuple_count, level + 1);

    auto offsets_ptr = reinterpret_cast<u32*>(dest_slots);

    // IDEA:
    // Making all the cache arrays a single array (either u64 or some tuple)
    // could improve cache locality since the caches are always accessed at the
    // same index all the time.
    thread_local std::vector<std::vector<const char*>> cached_strings_v;
    auto cached_strings_ptr = get_level_data(cached_strings_v, col_struct.num_codes, level);
    // There might be old data in here, 0 it out
    std::fill_n(cached_strings_v[level].begin(), col_struct.num_codes, nullptr);
    thread_local std::vector<std::vector<u32>> cached_run_lengths_v;
    auto cached_run_lengths_ptr = get_level_data(cached_run_lengths_v, col_struct.num_codes, level);

    thread_local std::vector<std::vector<u32>> cached_str_lengths_v;
    auto cached_str_lengths_ptr = get_level_data(cached_str_lengths_v, col_struct.num_codes, level);

    if (!col_struct.use_fsst) {
      StringArrayViewer dict_array(col_struct.data);

      // Fill str_length cache
      for (u32 idx = 0; idx < col_struct.num_codes; idx++) {
        cached_str_lengths_ptr[idx] = dict_array.size(idx);
      }

      // writeOffsetsU32 may write extra elements which in turn may overwrite
      // strings if not used in correct order. Therefore we write all the
      // offsets first and then write the strings.
      for (u32 run = 0; run < runs_count; run++) {
        u32 code = values_ptr[run];
        u32 run_length = counts_ptr[run];
        u32 str_length = cached_str_lengths_ptr[code];
        Utils::writeOffsetsU32(offsets_ptr, start, str_length, run_length);
        offsets_ptr += run_length;
        start += run_length * str_length;
      }
      *offsets_ptr = start;

      for (u32 run = 0; run < runs_count; run++) {
        u32 code = values_ptr[run];
        u32 run_length = counts_ptr[run];
        u32 str_length = cached_str_lengths_ptr[code];
        const char* str_src;
        u32 src_length;
        if (cached_strings_ptr[code] == nullptr) {
          str_src = dict_array.get_pointer(code);
          src_length = 1;
          cached_strings_ptr[code] = reinterpret_cast<char*>(dest_write_ptr);
          cached_run_lengths_ptr[code] = run_length;
        } else {
          str_src = cached_strings_ptr[code];
          src_length = cached_run_lengths_ptr[code];
          if (run_length > src_length) {
            cached_strings_ptr[code] = reinterpret_cast<char*>(dest_write_ptr);
            cached_run_lengths_ptr[code] = run_length;
          }
        }
        Utils::multiplyString(reinterpret_cast<char*>(dest_write_ptr), str_src, str_length,
                              run_length, src_length);
        dest_write_ptr += run_length * str_length;
      }
    } else {
      fsst_decoder_t decoder;
      const u32 fsst_dict_size = FSST_MAXHEADER;
      die_if(fsst_import(&decoder, const_cast<u8*>(col_struct.data)) > 0);
      auto fsst_offsets =
          reinterpret_cast<const u32*>(col_struct.data + col_struct.fsst_offsets_offset);
      auto fsst_compressed_buf = col_struct.data + fsst_dict_size;

      /*
       * For fsst we do not know the string lengths before decompression, so we
       * cannot write the offsets upfront.
       */
      for (u32 run = 0; run < runs_count; run++) {
        u32 code = values_ptr[run];
        u32 run_length = counts_ptr[run];
        u32 run_length_left = run_length;
        const char* str_src;
        u32 src_length;
        u32 str_length;
        if (cached_strings_ptr[code] == nullptr) {
          // We need to decompress the string
          auto compressed_str_length = fsst_offsets[code + 1] - fsst_offsets[code];
          auto compressed_str_ptr = fsst_compressed_buf + fsst_offsets[code];
          str_length =
              fsst_decompress(&decoder, compressed_str_length, const_cast<u8*>(compressed_str_ptr),
                              MAX_STR_LENGTH, dest_write_ptr);

          // Write cache
          cached_strings_ptr[code] = reinterpret_cast<char*>(dest_write_ptr);
          cached_str_lengths_ptr[code] = str_length;
          // We will have written run_length strings in the end, but for now
          // have only decompressed a single one.
          cached_run_lengths_ptr[code] = run_length;

          src_length = 1;
          str_src = reinterpret_cast<char*>(dest_write_ptr);
          run_length_left--;

          // Advance write pointer for utils functions
          dest_write_ptr += str_length;
        } else {
          // String has already been decompressed

          // Get values from cache
          str_length = cached_str_lengths_ptr[code];
          str_src = cached_strings_ptr[code];
          src_length = cached_run_lengths_ptr[code];

          // Update cache if necessary
          if (run_length > src_length) {
            cached_strings_ptr[code] = reinterpret_cast<char*>(dest_write_ptr);
            cached_run_lengths_ptr[code] = run_length;
          }
        }

        // Write offsets
        Utils::writeOffsetsU32(offsets_ptr, start, str_length, run_length);
        offsets_ptr += run_length;
        start += run_length * str_length;

        // Write strings
        Utils::multiplyString(reinterpret_cast<char*>(dest_write_ptr), str_src, str_length,
                              run_length_left, src_length);
        dest_write_ptr += run_length_left * str_length;
      }
      *offsets_ptr = start;
    }
  } else {
    thread_local std::vector<std::vector<INTEGER>> decompressed_codes_v;
    auto decompressed_codes =
        get_level_data(decompressed_codes_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
    IntegerScheme& codes_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme);
    codes_scheme.decompress(decompressed_codes, nullptr, compressed_codes_ptr, tuple_count,
                            level + 1);
    // -------------------------------------------------------------------------------------
    if (col_struct.use_fsst) {
      fsst_decoder_t decoder;
      const u32 fsst_dict_size = FSST_MAXHEADER;
      die_if(fsst_import(&decoder, const_cast<u8*>(col_struct.data)) > 0);
      auto fsst_offsets =
          reinterpret_cast<const u32*>(col_struct.data + col_struct.fsst_offsets_offset);
      auto fsst_compressed_buf = col_struct.data + fsst_dict_size;
      for (u32 row_i = 0; row_i < tuple_count; row_i++) {
        dest_slots[row_i].offset = dest_write_ptr - dest;
        auto code = decompressed_codes[row_i];
        auto compressed_str_length = fsst_offsets[code + 1] - fsst_offsets[code];
        auto compressed_str_ptr = fsst_compressed_buf + fsst_offsets[code];
        dest_write_ptr +=
            fsst_decompress(&decoder, compressed_str_length, const_cast<u8*>(compressed_str_ptr),
                            MAX_STR_LENGTH, dest_write_ptr);
      }
    } else {
      StringArrayViewer dict_array(col_struct.data);
      for (u32 row_i = 0; row_i < tuple_count; row_i++) {
        dest_slots[row_i].offset = dest_write_ptr - dest;
        auto current_code = decompressed_codes[row_i];
        u32 length = dict_array.size(current_code);
        const char* string = dict_array.get_pointer(current_code);
        std::memcpy(dest_write_ptr, string, length);
        dest_write_ptr += length;
      }
    }
    dest_slots[tuple_count].offset = dest_write_ptr - dest;
    die_if(dest_write_ptr - dest - SIMD_EXTRA_BYTES == col_struct.total_decompressed_size);
  }
}

bool DynamicDictionary::decompressNoCopy(u8* dest,
                                         BitmapWrapper*,
                                         const u8* src,
                                         u32 tuple_count,
                                         u32 level) {
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);

  // Build views
  thread_local std::vector<std::vector<StringPointerArrayViewer::View>> views_v;
  auto views_ptr = get_level_data(views_v, col_struct.num_codes, level);

  auto dest_views = reinterpret_cast<StringPointerArrayViewer::View*>(dest);
  auto current_offset = (tuple_count + 4) * sizeof(StringPointerArrayViewer::View);

  // Ideas for better performance
  // - save dictionary strings as StringPointerArrayView straight away
  // - decompress all fsst_strings at once and save sting lengths separately
  // - optimize the loop at the with avx gather or cache miss hiding through
  // vectorization

  // Copy strings to destination
  if (col_struct.use_fsst) {
    // Decompress lengths
    thread_local std::vector<std::vector<INTEGER>> uncompressed_lengths_v;
    auto uncompressed_lengths_ptr = get_level_data(
        uncompressed_lengths_v, col_struct.num_codes + SIMD_EXTRA_ELEMENTS(INTEGER), level);
    IntegerScheme& lengths_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.lengths_scheme);
    lengths_scheme.decompress(uncompressed_lengths_ptr, nullptr,
                              col_struct.data + col_struct.lengths_offset, col_struct.num_codes,
                              level + 1);

    // Fill lengths and offsets
    u32 start_offset = current_offset;
    for (u32 c = 0; c < col_struct.num_codes; c++) {
      views_ptr[c].offset = current_offset;
      views_ptr[c].length = uncompressed_lengths_ptr[c];
      current_offset += uncompressed_lengths_ptr[c];
    }
    u32 total_length = current_offset - start_offset;

    // Decompress all strings in one go
    fsst_decoder_t decoder;
    const u32 fsst_dict_size = FSST_MAXHEADER;
    die_if(fsst_import(&decoder, const_cast<u8*>(col_struct.data)) > 0);
    auto fsst_compressed_buf = col_struct.data + fsst_dict_size;
    u32 total_compressed_length = col_struct.fsst_offsets_offset - fsst_dict_size;
    fsst_decompress(&decoder, total_compressed_length, const_cast<u8*>(fsst_compressed_buf),
                    total_length + 4096, reinterpret_cast<unsigned char*>(dest) + start_offset);
  } else {
    auto start_offset = current_offset;
    StringArrayViewer dict_array(col_struct.data);
    for (u32 c = 0; c < col_struct.num_codes; c++) {
      views_ptr[c].length = dict_array.size(c);
      views_ptr[c].offset = current_offset;
      current_offset += views_ptr[c].length;
    }

    std::memcpy(reinterpret_cast<char*>(dest) + start_offset, dict_array.get_pointer(0),
                current_offset - start_offset);
  }

  const u8* compressed_codes_ptr = col_struct.data + col_struct.codes_offset;
  if (col_struct.use_rle_optimized_path) {
    IntegerScheme& codes_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme);
    auto& rle = dynamic_cast<btrblocks::db::v2::integer::RLE&>(codes_scheme);

    thread_local std::vector<std::vector<INTEGER>> values_v;
    auto values_ptr = get_level_data(values_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);

    thread_local std::vector<std::vector<INTEGER>> counts_v;
    auto counts_ptr = get_level_data(counts_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);

    u32 runs_count = rle.decompressRuns(values_ptr, counts_ptr, nullptr, compressed_codes_ptr,
                                        tuple_count, level + 1);

    static_assert(sizeof(StringPointerArrayViewer::View) == 8);
#ifdef BTR_USE_SIMD
    for (u32 run = 0; run < runs_count; run++) {
      INTEGER code = values_ptr[run];
      auto* data = reinterpret_cast<long long*>(views_ptr + code);
      __m256i data_v = _mm256_set1_epi64x(*data);
      INTEGER run_length = counts_ptr[run];
      auto dest_view_simd = reinterpret_cast<__m256i*>(dest_views);
      for (INTEGER repeat = 0; repeat < run_length; repeat += 4) {
        _mm256_storeu_si256(dest_view_simd, data_v);
        dest_view_simd++;
      }
      dest_views += run_length;
    }
#else
    for (u32 run = 0; run < runs_count; run++) {
      INTEGER code = values_ptr[run];
      for (INTEGER repeat = 0; repeat < counts_ptr[run]; ++repeat) {
        *dest_views++ = views_ptr[code];
      }
    }
#endif
  } else {
    // Decompress codes
    thread_local std::vector<std::vector<INTEGER>> decompressed_codes_v;
    auto decompressed_codes =
        get_level_data(decompressed_codes_v, tuple_count + SIMD_EXTRA_ELEMENTS(INTEGER), level);
    IntegerScheme& codes_scheme =
        IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme);
    codes_scheme.decompress(decompressed_codes, nullptr, compressed_codes_ptr, tuple_count,
                            level + 1);

    u32 row_i = 0;
#ifdef BTR_USE_SIMD
    static_assert(sizeof(*views_ptr) == 8);
    static_assert(SIMD_EXTRA_BYTES >= 4 * sizeof(__m256i));
    if (tuple_count >= 16) {
      while (row_i < tuple_count - 15) {
        // We cannot write out of bounds here like we do for other dict
        // implementations because it would destroy the string data.

        // Load codes.
        __m128i codes_0 = _mm_loadu_si128(reinterpret_cast<__m128i*>(decompressed_codes + 0));
        __m128i codes_1 = _mm_loadu_si128(reinterpret_cast<__m128i*>(decompressed_codes + 4));
        __m128i codes_2 = _mm_loadu_si128(reinterpret_cast<__m128i*>(decompressed_codes + 8));
        __m128i codes_3 = _mm_loadu_si128(reinterpret_cast<__m128i*>(decompressed_codes + 12));

        // Gather values.
        __m256i values_0 =
            _mm256_i32gather_epi64(reinterpret_cast<long long*>(views_ptr), codes_0, 8);
        __m256i values_1 =
            _mm256_i32gather_epi64(reinterpret_cast<long long*>(views_ptr), codes_1, 8);
        __m256i values_2 =
            _mm256_i32gather_epi64(reinterpret_cast<long long*>(views_ptr), codes_2, 8);
        __m256i values_3 =
            _mm256_i32gather_epi64(reinterpret_cast<long long*>(views_ptr), codes_3, 8);

        // Store values.
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest_views + 0), values_0);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest_views + 4), values_1);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest_views + 8), values_2);
        _mm256_storeu_si256(reinterpret_cast<__m256i*>(dest_views + 12), values_3);

        decompressed_codes += 16;
        dest_views += 16;
        row_i += 16;
      }
    }
#endif

    // Write remaining values (up to 15)
    while (row_i < tuple_count) {
      *dest_views++ = views_ptr[*decompressed_codes++];
      row_i++;
    }
  }

  return true;
}

std::string DynamicDictionary::fullDescription(const u8* src) {
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
  IntegerScheme& codes_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.codes_scheme);
  return this->selfDescription(src) + " -> ([int] codes) " +
         codes_scheme.fullDescription(col_struct.data + col_struct.codes_offset);
}

bool DynamicDictionary::isUsable(StringStats& stats) {
  // This is just a hacky way around, so we can properly test the improvement
  // from not having raw fsst to having the raw fsst scheme.
  bool has_raw_fsst = false;
  for (const auto& scheme : CSchemePool::available_schemes->string_schemes) {
    if (scheme.first == StringSchemeType::FSST) {
      has_raw_fsst = true;
      break;
    }
  }

  if (!has_raw_fsst) {
    return true;
  }

  u32 non_null_count = stats.tuple_count - stats.null_count;
  u32 unique_count = stats.unique_count;
  // Null may actually count as one unique string in the form of an empty string
  return unique_count < non_null_count / 2;
}

u32 DynamicDictionary::getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  const auto& col_struct = *reinterpret_cast<const DynamicDictionaryStructure*>(src);
  return col_struct.total_decompressed_size - ((tuple_count + 1) * sizeof(StringArrayViewer::Slot));
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v2::string
// -------------------------------------------------------------------------------------
