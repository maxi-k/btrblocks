#include "btrblocks.hpp"
// ------------------------------------------------------------------------------
#include "Fsst.hpp"
// ------------------------------------------------------------------------------
#include "common/Log.hpp"
#include "compression/SchemePicker.hpp"
// ------------------------------------------------------------------------------
namespace btrblocks::strings {
// ------------------------------------------------------------------------------
double Fsst::expectedCompressionRatio(StringStats& stats, u8) {
  // TODO estimating the expected compression ratio for FSST from statistics is
  // not trivial. This works for now b/c the only other contender is dictionary
  // encoding.
  return 1.0;
}

u32 Fsst::compress(const btrblocks::StringArrayViewer src,
                   const BITMAP*,
                   u8* dest,
                   btrblocks::StringStats& stats) {
  // TODO: For now we consider null values to be empty strings. Maybe it would
  // be faster to not compress them and use roaring bitmap iterate to fill the
  // StringArrayView on decompression
  auto& col_struct = *reinterpret_cast<FsstStructure*>(dest);
  col_struct.total_decompressed_size = stats.total_size;
  auto write_ptr = col_struct.data;

  auto& cfg = SchemeConfig::get().strings;

  // This was mostly adapted from what the DynamicDictionary used before.
  auto input_string_buffers = std::unique_ptr<u8*[]>(new u8*[stats.tuple_count]);
  auto input_string_lengths = std::unique_ptr<u64[]>(new u64[stats.tuple_count]);
  auto output_string_buffers = std::unique_ptr<u8*[]>(new u8*[stats.tuple_count]);
  auto output_string_lengths = std::unique_ptr<u64[]>(new u64[stats.tuple_count]);

  // Prepare necessary arrays
  for (u32 str_i = 0; str_i < stats.tuple_count; str_i++) {
    input_string_buffers[str_i] =
        const_cast<u8*>(reinterpret_cast<const u8*>(src.get_pointer(str_i)));
    input_string_lengths[str_i] = src.size(str_i);
  }

  // Prepare decoder and write header
  fsst_encoder_t* encoder =
      fsst_create(stats.tuple_count, input_string_lengths.get(), input_string_buffers.get(), 0);
  Utils::defer destory_encoder([&encoder]() { fsst_destroy(encoder); });
  die_if(fsst_export(encoder, write_ptr) > 0);
  auto fsst_table_used_space = FSST_MAXHEADER;
  write_ptr += fsst_table_used_space;
  col_struct.strings_offset = write_ptr - col_struct.data;

  // Compress strings
  // TODO whyever this is fake(?), fix it.
  const u64 output_buffer_size = 7 + 4 * stats.total_length;  // fake
  if (fsst_compress(encoder, stats.tuple_count, input_string_lengths.get(),
                    input_string_buffers.get(), output_buffer_size, write_ptr,
                    output_string_lengths.get(),
                    output_string_buffers.get()) != stats.tuple_count) {
    throw Generic_Exception("FSST Compression failed !");
  }
  u64 fsst_strings_used_space =
      output_string_lengths[stats.tuple_count - 1] +
      (output_string_buffers[stats.tuple_count - 1] - output_string_buffers[0]);

  Log::debug("FSST: strings_s = {}", static_cast<s64>(fsst_strings_used_space));
  Log::debug("FSST: string_size : before = {} after = {}", static_cast<s64>(stats.total_length),
             static_cast<s64>(fsst_table_used_space + fsst_strings_used_space));

  col_struct.compressed_strings_size = fsst_strings_used_space;
  write_ptr += fsst_strings_used_space;
  col_struct.offsets_offset = write_ptr - col_struct.data;

  u32 used_space;
  auto forced_scheme = cfg.fsst_force_codes_scheme;
  IntegerSchemePicker::compress(reinterpret_cast<const INTEGER*>(src.slots_ptr), nullptr, write_ptr,
                                stats.tuple_count + 1, cfg.fsst_codes_max_cascade_depth, used_space,
                                col_struct.offsets_scheme, CB(forced_scheme));
  write_ptr += used_space;

  return write_ptr - dest;
}

u32 Fsst::getDecompressedSize(const u8* src, u32, BitmapWrapper*) {
  auto& col_struct = *reinterpret_cast<const FsstStructure*>(src);
  return col_struct.total_decompressed_size;
}

void Fsst::decompress(u8* dest, BitmapWrapper*, const u8* src, u32 tuple_count, u32 level) {
  auto& col_struct = *reinterpret_cast<const FsstStructure*>(src);
  auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(dest);
  auto dest_write_ptr = dest + sizeof(StringArrayViewer::Slot) * (tuple_count + 1);

  // Decompress offsets
  auto compressed_offsets = col_struct.data + col_struct.offsets_offset;
  IntegerScheme& offsets_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.offsets_scheme);
  offsets_scheme.decompress(reinterpret_cast<INTEGER*>(dest_slots), nullptr, compressed_offsets,
                            tuple_count + 1, level + 1);

  // Decompress strings
  fsst_decoder_t decoder;
  die_if(fsst_import(&decoder, const_cast<u8*>(col_struct.data)) > 0) auto compressed_strings =
      const_cast<u8*>(col_struct.data + col_struct.strings_offset);
  auto decompressed_strings_size =
      col_struct.total_decompressed_size - ((tuple_count + 1) * sizeof(INTEGER));
  decompressed_strings_size += 4096;
  dest_write_ptr += fsst_decompress(&decoder, col_struct.compressed_strings_size,
                                    compressed_strings, decompressed_strings_size, dest_write_ptr);
  die_if(dest_write_ptr - dest == col_struct.total_decompressed_size)
}

std::string Fsst::fullDescription(const u8* src) {
  auto& col_struct = *reinterpret_cast<const FsstStructure*>(src);
  IntegerScheme& offsets_scheme =
      IntegerSchemePicker::MyTypeWrapper::getScheme(col_struct.offsets_scheme);
  return this->selfDescription(src) + " -> ([int] offsets) " +
         offsets_scheme.fullDescription(col_struct.data + col_struct.offsets_offset);
}

bool Fsst::isUsable(StringStats& stats) {
  u32 non_null_count = stats.tuple_count - stats.null_count;
  u32 unique_count = stats.unique_count;
  // Null may actually count as one unique string in the form of an empty string
  if (unique_count < non_null_count / 2) {
    return false;
  }
  return stats.total_length > SchemeConfig::FSST_THRESHOLD;
}

u32 Fsst::getTotalLength(const u8* src, u32 tuple_count, BitmapWrapper*) {
  auto& col_struct = *reinterpret_cast<const FsstStructure*>(src);
  return col_struct.total_decompressed_size - ((tuple_count + 1) * sizeof(StringArrayViewer::Slot));
}
// ------------------------------------------------------------------------------
}  // namespace btrblocks::strings
// ------------------------------------------------------------------------------
