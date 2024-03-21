#include "OneValue.hpp"
#include "common/Units.hpp"
#include "scheme/CompressionScheme.hpp"
#include "storage/Chunk.hpp"
#include "storage/StringPointerArrayViewer.hpp"
// -------------------------------------------------------------------------------------
#include "common/Utils.hpp"

// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks::legacy::strings {
// -------------------------------------------------------------------------------------
double OneValue::expectedCompressionRatio(StringStats& stats, u8 allowed_cascading_level) {
  if (stats.distinct_values.size() <= 1) {
    return stats.tuple_count;
  } else {
    return 0;
  }
}
// -------------------------------------------------------------------------------------
u32 OneValue::compress(const StringArrayViewer,
                       const BITMAP* bitmap,
                       u8* dest,
                       StringStats& stats) {
  auto& col_struct = *reinterpret_cast<OneValueStructure*>(dest);
  const auto one_value = *stats.distinct_values.begin();
  col_struct.length = one_value.length();
  std::memcpy(col_struct.data, one_value.data(), col_struct.length);
  return col_struct.length + sizeof(OneValueStructure);
}
// -------------------------------------------------------------------------------------
u32 OneValue::getDecompressedSizeNoCopy(const u8* src, u32 tuple_count, BitmapWrapper*) {
  auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  u32 total_size = tuple_count * sizeof(StringPointerArrayViewer::View);
  total_size += col_struct.length;
  return total_size;
}
// -------------------------------------------------------------------------------------
u32 OneValue::getDecompressedSize(const u8* src, u32 tuple_count, BitmapWrapper* nullmap) {
  /*
   * IMPORTANT: We use a custom decompressNoCopy. This is probably dead code for
   * now, but could be reused for actually copying every individual string
   */
  auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  u32 total_size = (tuple_count + 1) * sizeof(StringArrayViewer::Slot);
  total_size += nullmap->cardinality() * col_struct.length;
  return total_size;
}
// -------------------------------------------------------------------------------------
void OneValue::decompress(u8* dest,
                          BitmapWrapper* nullmap,
                          const u8* src,
                          u32 tuple_count,
                          u32 level) {
  /*
   * IMPORTANT: We use a custom decompressNoCopy. This is probably dead code for
   * now, but could be reused for actually copying every individual string
   */
  auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(dest);
  auto dest_strings = dest + ((tuple_count + 1) * sizeof(StringArrayViewer::Slot));
  // -------------------------------------------------------------------------------------
  u32 write_offset = (tuple_count + 1) * sizeof(StringArrayViewer::Slot);

  if (nullmap == nullptr || nullmap->type() == BitmapType::ALLONES) {
    Utils::writeOffsetsU32(reinterpret_cast<u32*>(dest), write_offset, col_struct.length,
                           tuple_count);
    Utils::multiplyString(reinterpret_cast<char*>(dest_strings),
                          reinterpret_cast<const char*>(col_struct.data), col_struct.length,
                          tuple_count, 1);
    write_offset += tuple_count * col_struct.length;
  } else if (nullmap->type() == BitmapType::ALLZEROS) {
    // Everything is null. content of values does not matter
    // Utils::multiplyU32(reinterpret_cast<u32 *>(dest_slots), &write_offset,
    // tuple_count);
    return;
  } else {
    /*
     * TODO the code here needs more testing and investigation.
     */
    Roaring& r = nullmap->roaring();
    if (nullmap->type() == BitmapType::REGULAR) {
      std::tuple<StringArrayViewer::Slot*, u32, u32> param = {dest_slots, write_offset,
                                                              col_struct.length};
      r.iterate(
          [](uint32_t value, void* param) {
            auto p = reinterpret_cast<std::tuple<StringArrayViewer::Slot*, u32, u32>*>(param);
            // TODO this actually looks wrong. Length calculation will probably
            // not work afterwards Set offset of string at value to current
            // offset
            std::get<0>(*p)[value].offset = std::get<1>(*p);
            // Advance offset by string length
            std::get<1>(*p) += std::get<2>(*p);
            // Set offset of next string to the advanced offset.
            // In case the string null this is necessary because the offset
            // would otherwise not be set and calculating the length of the
            // string at value would not yield a correct result.
            std::get<0>(*p)[value + 1].offset = std::get<1>(*p);
            return true;
          },
          &param);
    } else {  // FLIPPED
              // The roaring map is inverted
              // Every value iterate return is actually a null value.
      // We therefore write offsets from the last index we know is not null to
      // the index that holds a null value. The last index is the set to one
      // after the null value, so wie do effectively skip it.
      std::tuple<StringArrayViewer::Slot*, u32, u32, u32> param = {dest_slots, write_offset,
                                                                   col_struct.length, 0};
      r.iterate(
          [](uint32_t value, void* param) {
            auto p = reinterpret_cast<std::tuple<StringArrayViewer::Slot*, u32, u32, u32>*>(param);
            // Calculate how many offsets we need to fill (value - last non-null
            // index) + 1 +1 for the null value so string length calculations
            // don't break for non-null values
            u32 n = value - std::get<3>(*p) + 1;
            Utils::writeOffsetsU32(reinterpret_cast<u32*>(std::get<0>(*p) + std::get<3>(*p)),
                                   std::get<1>(*p), std::get<2>(*p), n);

            // We only wrote n-1 actual string (because the last one was a null
            // value)
            n--;
            // Advance write_offset by number of strings written time string
            // length
            std::get<1>(*p) += n * std::get<2>(*p);
            // Adjust last non null index.
            std::get<3>(*p) = value + 1;
            return true;
          },
          &param);

      // Write non null offsets at the end.
      // offset at tuple_count will be written at end of function
      u32 n = tuple_count - std::get<3>(param);
      Utils::writeOffsetsU32(reinterpret_cast<u32*>(std::get<0>(param) + std::get<3>(param)),
                             std::get<1>(param), std::get<2>(param), n);
      write_offset = std::get<1>(param) + n * std::get<2>(param);
    }

    Utils::multiplyString(reinterpret_cast<char*>(dest_strings),
                          reinterpret_cast<const char*>(col_struct.data), col_struct.length,
                          nullmap->cardinality(), 1);
  }

  dest_slots[tuple_count].offset = write_offset;
}

bool OneValue::decompressNoCopy(u8* dest,
                                BitmapWrapper* nullmap,
                                const u8* src,
                                u32 tuple_count,
                                u32) {
  if (nullmap && nullmap->type() == BitmapType::ALLZEROS) {
    return true;
  }

  auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);

  // We only have one string write the same offset everywhere
  auto dest_views = reinterpret_cast<StringPointerArrayViewer::View*>(dest);
  StringPointerArrayViewer::View view = {
      .length = col_struct.length,
      .offset = static_cast<u32>(tuple_count * sizeof(StringPointerArrayViewer::View))};

#ifdef BTR_USE_SIMD
  auto dest_view_simd = reinterpret_cast<__m256i*>(dest_views);
  auto* data = reinterpret_cast<long long*>(&view);
  __m256i data_v = _mm256_set1_epi64x(*data);
  for (u32 idx = 0; idx < tuple_count; idx += 16) {
    _mm256_storeu_si256(dest_view_simd, data_v);
    _mm256_storeu_si256(dest_view_simd + 1, data_v);
    _mm256_storeu_si256(dest_view_simd + 2, data_v);
    _mm256_storeu_si256(dest_view_simd + 3, data_v);
    dest_view_simd += 4;
  }
#else
  std::fill_n(dest_views, tuple_count, view);
#endif

  auto dest_strings = reinterpret_cast<u8*>(dest_views + tuple_count);
  std::memcpy(dest_strings, col_struct.data, col_struct.length);
  return true;
}

u32 OneValue::getTotalLength(const u8* src, u32, BitmapWrapper* nullmap) {
  auto& col_struct = *reinterpret_cast<const OneValueStructure*>(src);
  return nullmap->cardinality() * col_struct.length;
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::strings
// -------------------------------------------------------------------------------------
