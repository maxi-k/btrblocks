// -------------------------------------------------------------------------------------
#include "Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------------------------------------
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "compression/cache/ThreadCache.hpp"
#include "compression/schemes/CSchemePicker.hpp"
#include "compression/schemes/CSchemePool.hpp"
#include "schemes/CScheme.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v1/double/OneValue.hpp"
#include "compression/schemes/v1/double/Uncompressed.hpp"
#include "compression/schemes/v1/integer/Dictionary.hpp"
#include "compression/schemes/v1/integer/OneValue.hpp"
#include "compression/schemes/v1/integer/Truncation.hpp"
#include "compression/schemes/v1/integer/Uncompressed.hpp"
#include "compression/schemes/v1/string/Dictionary.hpp"
#include "compression/schemes/v1/string/OneValue.hpp"
#include "compression/schemes/v1/string/Uncompressed.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v2/bitmap/RoaringBitmap.hpp"
#include "compression/schemes/v2/string/DynamicDictionary.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
#include <algorithm>
#include <exception>
#include <fstream>
#include <limits>
#include <numeric>
#include <roaring/roaring.hh>
#include <set>
// -------------------------------------------------------------------------------------
DEFINE_uint32(doubles_max_cascading_level, 3, "");
DEFINE_uint32(integers_max_cascading_level, 3, "");
DEFINE_uint32(strings_max_cascading_level, 3, "");
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
// -------------------------------------------------------------------------------------
Datablock::Datablock(const Relation& relation) : CMachine(relation) {}
u32 Datablock::writeMetadata(const std::string& path,
                             std::vector<ColumnType> types,
                             vector<u32> part_counters,
                             u32 num_chunks) {
  std::ofstream metadata_file(path, std::ios::out | std::ios::binary);
  if (!metadata_file.good()) {
    throw Generic_Exception("Opening metadata output file failed");
  }

  u32 bytes_written = 0;
  FileMetadata metadata{.num_columns = static_cast<u32>(types.size()),
                        .num_chunks = static_cast<u32>(num_chunks)};

  metadata_file.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));
  bytes_written += sizeof(metadata);

  for (u32 column = 0; column < metadata.num_columns; column++) {
    ColumnPartInfo info{.type = types[column],
                        .num_parts = static_cast<u32>(part_counters[column])};
    metadata_file.write(reinterpret_cast<const char*>(&info), sizeof(info));
    bytes_written += sizeof(info);
  }

  metadata_file.close();
  return bytes_written;
}
// -------------------------------------------------------------------------------------
std::vector<u8> Datablock::compress(const InputChunk& input_chunk) {
  // We do not now the exact output size. Therefore we allocate too much and
  // then simply make the space smaller afterwards
  const u32 size =
      sizeof(ColumnChunkMeta) + 10 * input_chunk.size + sizeof(BITMAP) * input_chunk.tuple_count;
  std::vector<u8> output(size);
  auto meta = reinterpret_cast<ColumnChunkMeta*>(output.data());
  meta->tuple_count = input_chunk.tuple_count;
  meta->type = input_chunk.type;

  auto output_data = meta->data;

  switch (input_chunk.type) {
    case ColumnType::INTEGER: {
      IntegerSchemePicker::compress(reinterpret_cast<INTEGER*>(input_chunk.data.get()),
                                    input_chunk.nullmap.get(), output_data, input_chunk.tuple_count,
                                    FLAGS_integers_max_cascading_level, meta->nullmap_offset,
                                    meta->compression_type);
      break;
    }
    case ColumnType::DOUBLE: {
      // -------------------------------------------------------------------------------------
      DoubleSchemePicker::compress(reinterpret_cast<DOUBLE*>(input_chunk.data.get()),
                                   input_chunk.nullmap.get(), output_data, input_chunk.tuple_count,
                                   FLAGS_doubles_max_cascading_level, meta->nullmap_offset,
                                   meta->compression_type);
      // -------------------------------------------------------------------------------------
      break;
    }
    case ColumnType::STRING: {
      // -------------------------------------------------------------------------------------
      // Collect stats
      StringStats stats = StringStats::generateStats(StringArrayViewer(input_chunk.data.get()),
                                                     input_chunk.nullmap.get(),
                                                     input_chunk.tuple_count, input_chunk.size);
      // -------------------------------------------------------------------------------------
      // Make decisions
      StringScheme& preferred_scheme =
          StringSchemePicker::chooseScheme(stats, FLAGS_strings_max_cascading_level);
      // Update meta data
      meta->compression_type = static_cast<u8>(preferred_scheme.schemeType());
      // -------------------------------------------------------------------------------------
      // Compression
      ThreadCache::get().compression_level++;
      const StringArrayViewer str_viewer(input_chunk.data.get());
      u32 after_column_size =
          preferred_scheme.compress(str_viewer, input_chunk.nullmap.get(), output_data, stats);
      meta->nullmap_offset = after_column_size;
      // -------------------------------------------------------------------------------------
      for (u8 i = 0; i < 5 - FLAGS_strings_max_cascading_level; i++) {
        ThreadCache::get() << "\t";
      }
      ThreadCache::get() << "for : ? - scheme = " +
                                ConvertSchemeTypeToString(preferred_scheme.schemeType());
      +" before = " + std::to_string(stats.total_size) +
          " after = " + std::to_string(after_column_size) +
          " gain = " + std::to_string(CD(stats.total_size) / CD(after_column_size)) + '\n';

      double estimated_cf =
          CD(stats.total_size) / CD(after_column_size);  // we only have one scheme for strings
                                                         // (beside ONE_VALUE)
      ThreadCache::dumpPush(ConvertSchemeTypeToString(preferred_scheme.schemeType()) +
                                (ThreadCache::hasUsedFsst() ? "_FSST" : ""),
                            estimated_cf, stats.total_size, after_column_size, stats.unique_count,
                            "?");
      ThreadCache::get().compression_level--;
      // -------------------------------------------------------------------------------------
      break;
    }
    default:
      throw Generic_Exception("Type not supported");
  }

  // Compress bitmap
  auto [nullmap_size, bitmap_type] = v2::bitmap::RoaringBitmap::compress(
      input_chunk.nullmap.get(), output_data + meta->nullmap_offset, input_chunk.tuple_count);
  meta->nullmap_type = bitmap_type;
  u32 total_size = sizeof(*meta) + meta->nullmap_offset + nullmap_size;
  // Resize the output vector to the actual used size
  output.resize(total_size);

  // Print decision tree
  ThreadCache::get() << " type = " + ConvertTypeToString(input_chunk.type) +
                            " before = " + std::to_string(input_chunk.size) +
                            " after = " + std::to_string(total_size) +
                            " gain = " + std::to_string(CD(input_chunk.size) / CD(total_size)) +
                            "\n\n\n";
  return output;
}

bool Datablock::decompress(const u8* data_in, BitmapWrapper** bitmap_out, u8* data_out) {
  // TODO this code is unused
  auto meta = reinterpret_cast<const ColumnChunkMeta*>(data_in);

  // Decompress bitmap
  *bitmap_out =
      new BitmapWrapper(meta->data + meta->nullmap_offset, meta->nullmap_type, meta->tuple_count);

  // Decompress data, we assume that data_out already has the correct size
  // allocated
  bool requires_copy_out;
  switch (meta->type) {
    case ColumnType::INTEGER: {
      auto& scheme = CSchemePool::available_schemes
                         ->integer_schemes[static_cast<IntegerSchemeType>(meta->compression_type)];
      scheme->decompress(reinterpret_cast<INTEGER*>(data_out), *bitmap_out, meta->data,
                         meta->tuple_count, 0);
      requires_copy_out = false;
      break;
    }
    case ColumnType::DOUBLE: {
      auto& scheme = CSchemePool::available_schemes
                         ->double_schemes[static_cast<DoubleSchemeType>(meta->compression_type)];
      scheme->decompress(reinterpret_cast<DOUBLE*>(data_out), *bitmap_out, meta->data,
                         meta->tuple_count, 0);
      requires_copy_out = false;
      break;
    }
    case ColumnType::STRING: {
      auto& scheme = CSchemePool::available_schemes
                         ->string_schemes[static_cast<StringSchemeType>(meta->compression_type)];
      requires_copy_out =
          scheme->decompressNoCopy(data_out, *bitmap_out, meta->data, meta->tuple_count, 0);
      break;
    }
    default:
      throw Generic_Exception("Type not supported");
  }

  return requires_copy_out;
}

OutputBlockStats Datablock::compress(const Chunk& input_chunk, BytesArray& output_block) {
  // -------------------------------------------------------------------------------------
  const u32 db_meta_buffer_size =
      sizeof(DatablockMeta) + (relation.columns.size() * sizeof(ColumnMeta));
  u32 input_chunk_total_data_size = 0;
  // Reserve memory for output (datablock)
  if (!output_block) {
    for (u32 column_i = 0; column_i < relation.columns.size(); column_i++) {
      input_chunk_total_data_size += input_chunk.size(column_i);
    }
    const u32 output_block_size =
        (db_meta_buffer_size + input_chunk_total_data_size) * 10 +
        (relation.columns.size() * sizeof(BITMAP) * input_chunk.tuple_count);
    output_block = makeBytesArray(output_block_size);
  }
  // -------------------------------------------------------------------------------------
  assert(output_block);
  // -------------------------------------------------------------------------------------
  auto db_meta = reinterpret_cast<DatablockMeta*>(output_block.get());
  db_meta->count = input_chunk.tuple_count;
  db_meta->column_count = relation.columns.size();
  // -------------------------------------------------------------------------------------
  OutputBlockStats output_db_stats = {
      .data_sizes = vector<SIZE>(relation.columns.size(), 0),
      .nullmap_sizes = vector<SIZE>(relation.columns.size(), 0),
      .used_compression_schemes = vector<u8>(relation.columns.size(), 255),
      .total_data_size = 0,
      .total_nullmap_size = 0,
      .total_db_size = db_meta_buffer_size};  // we are going to determine the
                                              // size during columns analysis
  u32 db_write_offset = db_meta_buffer_size;
  // -------------------------------------------------------------------------------------
  u32 after_column_size = 0;
  for (u32 column_i = 0; column_i < relation.columns.size(); column_i++) {
    auto& column = relation.columns[column_i];
    auto& column_meta = db_meta->attributes_meta[column_i];
    column_meta.column_type = column.type;
    // -------------------------------------------------------------------------------------
    Log::info("DB: compressing column : {}", column.name);
    ThreadCache::dumpSet(relation.name, column.name, ConvertTypeToString(column.type));
    // -------------------------------------------------------------------------------------
    switch (column.type) {
      case ColumnType::INTEGER: {
        // First: apply FOR to remove negative numbers and decrease the range
        // -------------------------------------------------------------------------------------
        const BITMAP* nullmap = input_chunk.nullmap(column_i);
        // -------------------------------------------------------------------------------------
        // Compression
        IntegerSchemePicker::compress(input_chunk.array<INTEGER>(column_i), nullmap,
                                      output_block.get() + db_write_offset, input_chunk.tuple_count,
                                      FLAGS_integers_max_cascading_level, after_column_size,
                                      column_meta.compression_type);
        after_column_size += sizeof(column_meta.bias);
        // -------------------------------------------------------------------------------------
        break;
      }
      case ColumnType::DOUBLE: {
        // -------------------------------------------------------------------------------------
        DoubleSchemePicker::compress(
            input_chunk.array<DOUBLE>(column_i), input_chunk.nullmap(column_i),
            output_block.get() + db_write_offset, input_chunk.tuple_count,
            FLAGS_doubles_max_cascading_level, after_column_size, column_meta.compression_type);
        // -------------------------------------------------------------------------------------
        break;
      }
      case ColumnType::STRING: {
        // -------------------------------------------------------------------------------------
        // Collect stats
        StringStats stats = StringStats::generateStats(
            StringArrayViewer(input_chunk.array<const u8>(column_i)), input_chunk.nullmap(column_i),
            input_chunk.tuple_count, input_chunk.size(column_i));
        // -------------------------------------------------------------------------------------
        // Make decisions
        StringScheme& preferred_scheme =
            StringSchemePicker::chooseScheme(stats, FLAGS_strings_max_cascading_level);
        // Update meta data
        column_meta.compression_type = static_cast<u8>(preferred_scheme.schemeType());
        // -------------------------------------------------------------------------------------
        // Compression
        ThreadCache::get().compression_level++;
        const StringArrayViewer str_viewer(input_chunk.array<u8>(column_i));
        after_column_size = preferred_scheme.compress(str_viewer, input_chunk.nullmap(column_i),
                                                      output_block.get() + db_write_offset, stats);
        // -------------------------------------------------------------------------------------
        for (u8 i = 0; i < 5 - FLAGS_strings_max_cascading_level; i++) {
          ThreadCache::get() << "\t";
        }
        ThreadCache::get() << "for : ? - scheme = " +
                                  ConvertSchemeTypeToString(preferred_scheme.schemeType());
        +" before = " + std::to_string(stats.total_size) +
            " after = " + std::to_string(after_column_size) +
            " gain = " + std::to_string(CD(stats.total_size) / CD(after_column_size)) + '\n';

        double estimated_cf =
            CD(stats.total_size) / CD(after_column_size);  // we only have one scheme for strings
                                                           // (beside ONE_VALUE)
        ThreadCache::dumpPush(ConvertSchemeTypeToString(preferred_scheme.schemeType()) +
                                  (ThreadCache::hasUsedFsst() ? "_FSST" : ""),
                              estimated_cf, stats.total_size, after_column_size, stats.unique_count,
                              "?");
        ThreadCache::get().compression_level--;
        // -------------------------------------------------------------------------------------
        break;
      }
      default:
        throw Generic_Exception("Type not supported");
    }
    // -------------------------------------------------------------------------------------
    // Update offsets
    column_meta.offset = db_write_offset;
    db_write_offset += after_column_size;
    // -------------------------------------------------------------------------------------
    // Compress bitmap
    column_meta.nullmap_offset = db_write_offset;
    auto [nullmap_size, bitmap_type] = v2::bitmap::RoaringBitmap::compress(
        input_chunk.nullmap(column_i), output_block.get() + db_write_offset,
        input_chunk.tuple_count);
    column_meta.bitmap_type = bitmap_type;
    db_write_offset += nullmap_size;
    output_db_stats.nullmap_sizes[column_i] = nullmap_size;
    // -------------------------------------------------------------------------------------
    // Update output db stats
    output_db_stats.used_compression_schemes[column_i] = column_meta.compression_type;
    output_db_stats.data_sizes[column_i] = after_column_size;
    // -------------------------------------------------------------------------------------
    // Print decision tree
    ThreadCache::get() << "name = " + column.name + " type = " + ConvertTypeToString(column.type) +
                              " before = " + std::to_string(input_chunk.size(column_i)) +
                              " after = " + std::to_string(after_column_size) + " gain = " +
                              std::to_string(CD(input_chunk.size(column_i)) /
                                             CD(after_column_size)) +
                              "\n\n\n";
  }
  // -------------------------------------------------------------------------------------
  // We don't really have to calculate total size and compression ratio here :-S
  for (u32 column_i = 0; column_i < relation.columns.size(); column_i++) {
    output_db_stats.total_data_size += output_db_stats.data_sizes[column_i];
    output_db_stats.total_nullmap_size += output_db_stats.nullmap_sizes[column_i];
  }
  output_db_stats.total_db_size +=
      output_db_stats.total_nullmap_size + output_db_stats.total_data_size;
  assert(db_write_offset <= output_db_stats.total_db_size);
  output_db_stats.compression_ratio = static_cast<double>(input_chunk_total_data_size) /
                                      static_cast<double>(output_db_stats.total_data_size);
  // -------------------------------------------------------------------------------------
  db_meta->size = output_db_stats.total_db_size;
  // -------------------------------------------------------------------------------------
  return output_db_stats;
}
// -------------------------------------------------------------------------------------
btrblocks::Chunk Datablock::decompress(const BytesArray& input_db) {
  // TODO this function shoiuld not rely on the presence of the relation
  auto db_meta = reinterpret_cast<DatablockMeta*>(input_db.get());
  const u32 tuple_count = db_meta->count;
  // -------------------------------------------------------------------------------------
  auto columns =
      std::unique_ptr<std::unique_ptr<u8[]>[]>(new std::unique_ptr<u8[]>[relation.columns.size()]);
  std::unique_ptr<bool[]> column_requires_copy(new bool[relation.columns.size()]);
  auto bitmaps = std::unique_ptr<std::unique_ptr<BITMAP[]>[]>(
      new std::unique_ptr<BITMAP[]>[relation.columns.size()]);
  auto sizes = std::unique_ptr<size_t[]>(new size_t[relation.columns.size()]);
  // -------------------------------------------------------------------------------------
  for (u32 column_i = 0; column_i < relation.columns.size(); column_i++) {
    auto& column = relation.columns[column_i];
    auto& column_meta = db_meta->attributes_meta[column_i];
    bitmaps[column_i] = std::unique_ptr<BITMAP[]>(new BITMAP[tuple_count]);
    // -------------------------------------------------------------------------------------
    // Decompress bitmap if necessary
    auto bitmap =
        BitmapWrapper(reinterpret_cast<const u8*>(input_db.get() + column_meta.nullmap_offset),
                      column_meta.bitmap_type, tuple_count);
    bitmap.writeBITMAP(bitmaps[column_i].get());
    // -------------------------------------------------------------------------------------
    switch (column.type) {
      case ColumnType::INTEGER: {
        // -------------------------------------------------------------------------------------
        sizes[column_i] = sizeof(INTEGER) * tuple_count;
        columns[column_i] = makeBytesArray(sizeof(INTEGER) * tuple_count + SIMD_EXTRA_BYTES);
        // -------------------------------------------------------------------------------------
        auto destination_array = reinterpret_cast<INTEGER*>(columns[column_i].get());
        auto& scheme = IntegerSchemePicker::MyTypeWrapper::getScheme(column_meta.compression_type);
        // -------------------------------------------------------------------------------------
        scheme.decompress(destination_array, &bitmap, input_db.get() + column_meta.offset,
                          tuple_count, 0);
        column_requires_copy[column_i] = false;
        break;
      }
      case ColumnType::DOUBLE: {
        // -------------------------------------------------------------------------------------
        sizes[column_i] = sizeof(DOUBLE) * tuple_count;
        columns[column_i] = makeBytesArray(sizeof(DOUBLE) * tuple_count + SIMD_EXTRA_BYTES);
        // -------------------------------------------------------------------------------------
        auto column_dest_double_array = reinterpret_cast<DOUBLE*>(columns[column_i].get());
        const auto used_compression_scheme =
            static_cast<DoubleSchemeType>(column_meta.compression_type);
        auto& scheme = CSchemePool::available_schemes->double_schemes[used_compression_scheme];
        // -------------------------------------------------------------------------------------
        scheme->decompress(column_dest_double_array, &bitmap, input_db.get() + column_meta.offset,
                           tuple_count, 0);
        column_requires_copy[column_i] = false;
        break;
      }
      case ColumnType::STRING: {
        // -------------------------------------------------------------------------------------
        const auto used_compression_scheme =
            static_cast<StringSchemeType>(column_meta.compression_type);
        auto& scheme = CSchemePool::available_schemes->string_schemes[used_compression_scheme];
        // -------------------------------------------------------------------------------------
        sizes[column_i] = scheme->getDecompressedSizeNoCopy(input_db.get() + column_meta.offset,
                                                            tuple_count, &bitmap);
        // TODO The 4096 is temporary until I figure out why FSST is returning
        // bigger numbers
        columns[column_i] = makeBytesArray(sizes[column_i] + 8 + SIMD_EXTRA_BYTES +
                                           4096);  // +8 because of 8 fsst decompression
        // -------------------------------------------------------------------------------------
        column_requires_copy[column_i] = scheme->decompressNoCopy(
            columns[column_i].get(), &bitmap, input_db.get() + column_meta.offset, tuple_count, 0);
        break;
      }
      default:
        throw Generic_Exception("Type not supported");
        break;
    }
  }
  // -------------------------------------------------------------------------------------
  return Chunk(std::move(columns), std::move(bitmaps), std::move(column_requires_copy), tuple_count,
               relation, std::move(sizes));
}
// -------------------------------------------------------------------------------------
void Datablock::getCompressedColumn(const BytesArray& input_db, u32 col_i, u8*& ptr, u32& size) {
  auto db_meta = reinterpret_cast<DatablockMeta*>(input_db.get());
  ptr = input_db.get() + db_meta->attributes_meta[col_i].offset;
  if (col_i == relation.columns.size() - 1) {
    size = db_meta->size - db_meta->attributes_meta[col_i].offset;
  } else {
    size = db_meta->attributes_meta[col_i + 1].offset - db_meta->attributes_meta[col_i].offset;
  }
}
// -------------------------------------------------------------------------------------
void CSchemePool::refresh() {
  btrblocks::db::CSchemePool::available_schemes = make_unique<btrblocks::db::SchemesCollection>();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db
// -------------------------------------------------------------------------------------
