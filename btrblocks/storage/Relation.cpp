#include "Relation.hpp"
#include "StringArrayViewer.hpp"
#include "btrblocks.hpp"
#include "common/Exceptions.hpp"
#include "storage/Chunk.hpp"
// -------------------------------------------------------------------------------------
#include <mutex>
#include <random>
#include <regex>
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
Relation::Relation() {
  columns.reserve(100);
}
// -------------------------------------------------------------------------------------
void Relation::addColumn(Column&& column) {
  columns.push_back(std::move(column));
  fixTupleCount();
}
// -------------------------------------------------------------------------------------
void Relation::addColumn(const string& column_file_path) {
  std::string column_name, column_type_str;
  std::regex re(R"(.*\/(.*)\.(\w*))");
  std::smatch match;
  if (std::regex_search(column_file_path, match, re) && match.size() > 1) {
    column_name = match.str(1);
    column_type_str = match.str(2);
    ColumnType column_type = ConvertStringToType(column_type_str);

    string bitmap_file_path = column_file_path;
    bitmap_file_path = bitmap_file_path.replace(bitmap_file_path.end() - column_type_str.length(),
                                                bitmap_file_path.end(), "bitmap");
    columns.emplace_back(column_type, column_name, column_file_path, bitmap_file_path);
  } else {
    UNREACHABLE();
  }
  fixTupleCount();
}
// -------------------------------------------------------------------------------------
// This could as well just return vector<btrblocks::Range>
vector<tuple<u64, u64>> Relation::getRanges(btrblocks::SplitStrategy strategy,
                                            u32 max_chunk_count) const {
  // -------------------------------------------------------------------------------------
  auto& cfg = BtrBlocksConfig::get();
  // -------------------------------------------------------------------------------------
  // Build all possible ranges
  vector<tuple<u64, u64>> ranges;  // (start_index, length)
  for (u64 offset = 0; offset < tuple_count; offset += cfg.block_size) {
    // -------------------------------------------------------------------------------------
    u64 chunk_tuple_count;
    if (offset + cfg.block_size >= tuple_count) {
      chunk_tuple_count = tuple_count - offset;
    } else {
      chunk_tuple_count = cfg.block_size;
    }
    ranges.emplace_back(offset, chunk_tuple_count);
  }
  // -------------------------------------------------------------------------------------
  if (strategy == SplitStrategy::RANDOM) {
    std::shuffle(ranges.begin(), ranges.end(), std::mt19937(std::random_device()()));
    cout << std::get<0>(ranges[0]) << endl;
  }
  // -------------------------------------------------------------------------------------
  if (max_chunk_count) {
    ranges.resize(std::min(static_cast<SIZE>(max_chunk_count), ranges.size()));
  }
  return ranges;
}
// -------------------------------------------------------------------------------------
Chunk Relation::getChunk(const vector<btrblocks::Range>& ranges, SIZE chunk_i) const {
  auto const& range = ranges[chunk_i];
  auto offset = std::get<0>(range);
  auto chunk_tuple_count = std::get<1>(range);
  // -------------------------------------------------------------------------------------
  auto c_columns =
      std::unique_ptr<std::unique_ptr<u8[]>[]>(new std::unique_ptr<u8[]>[columns.size()]);
  auto c_bitmaps =
      std::unique_ptr<std::unique_ptr<BITMAP[]>[]>(new std::unique_ptr<BITMAP[]>[columns.size()]);
  auto c_sizes = std::unique_ptr<SIZE[]>(new SIZE[columns.size()]);
  // -------------------------------------------------------------------------------------
  for (u32 i = 0; i < columns.size(); i++) {
    // -------------------------------------------------------------------------------------
    c_bitmaps[i] = std::unique_ptr<BITMAP[]>(new BITMAP[chunk_tuple_count * sizeof(BITMAP)]);
    std::memcpy(reinterpret_cast<void*>(c_bitmaps[i].get()), columns[i].bitmaps().data + offset,
                chunk_tuple_count * sizeof(BITMAP));
    // -------------------------------------------------------------------------------------
    switch (columns[i].type) {
      case ColumnType::INTEGER: {
        c_sizes[i] = chunk_tuple_count * sizeof(INTEGER);
        c_columns[i] = std::unique_ptr<u8[]>(new u8[c_sizes[i]]);
        std::memcpy(reinterpret_cast<void*>(c_columns[i].get()),
                    columns[i].integers().data + offset, chunk_tuple_count * sizeof(INTEGER));
        break;
      }
      case ColumnType::DOUBLE: {
        c_sizes[i] = chunk_tuple_count * sizeof(DOUBLE);
        c_columns[i] = std::unique_ptr<u8[]>(new u8[c_sizes[i]]);
        std::memcpy(reinterpret_cast<void*>(c_columns[i].get()), columns[i].doubles().data + offset,
                    chunk_tuple_count * sizeof(DOUBLE));
        break;
      }
      case ColumnType::STRING: {
        const u64 slots_size = sizeof(StringArrayViewer::Slot) * (chunk_tuple_count + 1);
        // -------------------------------------------------------------------------------------
        const StringIndexSlot* source_slots = columns[i].strings().data->slot;

        const u64 strings_size =
            (source_slots[offset + chunk_tuple_count - 1].offset - source_slots[offset].offset) +
            source_slots[offset + chunk_tuple_count - 1].size;
        // -------------------------------------------------------------------------------------
        c_sizes[i] = slots_size + strings_size;
        c_columns[i] = std::unique_ptr<u8[]>(new u8[c_sizes[i]]);
        // -------------------------------------------------------------------------------------
        auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(c_columns[i].get());
        // -------------------------------------------------------------------------------------
        const u64 bias = source_slots[offset].offset - slots_size;
        for (u64 slot_index = 0; slot_index < chunk_tuple_count; slot_index++) {
          dest_slots[slot_index].offset = source_slots[slot_index + offset].offset - bias;
        }
        dest_slots[chunk_tuple_count].offset = c_sizes[i];
        // -------------------------------------------------------------------------------------
        // copy the strings
        std::memcpy(reinterpret_cast<void*>(c_columns[i].get() + slots_size),
                    columns[i].strings()[offset].data(), strings_size);
        // -------------------------------------------------------------------------------------
        break;
      }
      default:
        UNREACHABLE();
        break;
    }
  }
  // -------------------------------------------------------------------------------------
  return Chunk(std::move(c_columns), std::move(c_bitmaps), chunk_tuple_count, *this,
               std::move(c_sizes));
}
// -------------------------------------------------------------------------------------
InputChunk Relation::getInputChunk(const Range& range,
                                   [[maybe_unused]] SIZE chunk_i,
                                   u32 column) const {
  auto offset = std::get<0>(range);
  auto chunk_tuple_count = std::get<1>(range);

  auto bitmap = std::unique_ptr<BITMAP[]>(new BITMAP[chunk_tuple_count * sizeof(BITMAP)]);
  std::memcpy(reinterpret_cast<void*>(bitmap.get()), columns[column].bitmaps().data + offset,
              chunk_tuple_count * sizeof(BITMAP));

  SIZE size;
  std::unique_ptr<u8[]> data;
  switch (columns[column].type) {
    case ColumnType::INTEGER: {
      size = chunk_tuple_count * sizeof(INTEGER);
      data = std::unique_ptr<u8[]>(new u8[size]);
      std::memcpy(reinterpret_cast<void*>(data.get()), columns[column].integers().data + offset,
                  chunk_tuple_count * sizeof(INTEGER));
      break;
    }
    case ColumnType::DOUBLE: {
      size = chunk_tuple_count * sizeof(DOUBLE);
      data = std::unique_ptr<u8[]>(new u8[size]);
      std::memcpy(reinterpret_cast<void*>(data.get()), columns[column].doubles().data + offset,
                  chunk_tuple_count * sizeof(DOUBLE));
      break;
    }
    case ColumnType::STRING: {
      const u64 slots_size = sizeof(StringArrayViewer::Slot) * (chunk_tuple_count + 1);
      // -------------------------------------------------------------------------------------
      const StringIndexSlot* source_slots = columns[column].strings().data->slot;

      const u64 strings_size =
          (source_slots[offset + chunk_tuple_count - 1].offset - source_slots[offset].offset) +
          source_slots[offset + chunk_tuple_count - 1].size;
      // -------------------------------------------------------------------------------------
      size = slots_size + strings_size;
      data = std::unique_ptr<u8[]>(new u8[size]);
      // -------------------------------------------------------------------------------------
      auto dest_slots = reinterpret_cast<StringArrayViewer::Slot*>(data.get());
      // -------------------------------------------------------------------------------------
      const u64 bias = source_slots[offset].offset - slots_size;
      for (u64 slot_index = 0; slot_index < chunk_tuple_count; slot_index++) {
        dest_slots[slot_index].offset = source_slots[slot_index + offset].offset - bias;
      }
      dest_slots[chunk_tuple_count].offset = size;
      // -------------------------------------------------------------------------------------
      // copy the strings
      std::memcpy(reinterpret_cast<void*>(data.get() + slots_size),
                  columns[column].strings()[offset].data(), strings_size);
      // -------------------------------------------------------------------------------------
      break;
    }
    default:
      throw Generic_Exception("Type not implemented");
  }

  return {std::move(data), std::move(bitmap), columns[column].type, chunk_tuple_count, size};
}
// -------------------------------------------------------------------------------------
void Relation::fixTupleCount() {
  if (columns.size()) {
    tuple_count = columns[0].bitmap.size();
    for (auto& column : columns) {
      die_if(tuple_count == column.bitmap.size());
    }
  } else {
    tuple_count = 0;
  }
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
