#include "Chunk.hpp"
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/Utils.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
#include <fstream>
#include <iomanip>
#include <memory>
// -------------------------------------------------------------------------------------
constexpr uint32_t chunk_part_size_threshold = 16 * 1024 * 1024;
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
void Chunk::reset() {
  for (u32 col_i = 0; col_i < relation.columns.size(); col_i++) {
    columns[col_i].reset();
  }
  sizes.reset();
  columns.reset();
}

// -------------------------------------------------------------------------------------
bool Chunk::operator==(const btrblocks::Chunk& other) const {
  // offset_from_relation is not relevant here
  if (tuple_count != other.tuple_count) {
    return false;
  }
  if (relation.columns.size() != other.relation.columns.size()) {
    return false;
  }

  for (u32 column_i = 0; column_i < relation.columns.size(); column_i++) {
    if (!column_requires_copy[column_i] && sizes[column_i] != other.sizes[column_i]) {
      cerr << "== : sizes in column " << column_i << " are not identical: " << sizes[column_i]
           << " vs. " << other.sizes[column_i] << endl;
      return false;
    }

    if ((bitmaps[column_i] && other.bitmaps[column_i]) &&
        (std::memcmp(bitmaps[column_i].get(), other.bitmaps[column_i].get(),
                     sizeof(BITMAP) * tuple_count) != 0)) {
      cerr << "== : bitmaps in column " << column_i << " are not identical" << endl;
      return false;
    }

    for (u32 row_i = 0; row_i < tuple_count; row_i++) {
      if (bitmaps[column_i][row_i]) {
        switch (relation.columns[column_i].type) {
          case ColumnType::INTEGER: {
            auto me = reinterpret_cast<INTEGER*>(columns[column_i].get())[row_i];
            auto they = reinterpret_cast<INTEGER*>(other.columns[column_i].get())[row_i];
            if (me != they) {
              cerr << "== : INTEGER column (" << relation.columns[column_i].name
                   << ") data are not identical\t"
                   << "row_i = " << row_i << endl
                   << me << endl
                   << they << endl;
              return false;
            }
            break;
          }
          case ColumnType::DOUBLE: {
            auto me = reinterpret_cast<DOUBLE*>(columns[column_i].get())[row_i];
            auto they = reinterpret_cast<DOUBLE*>(other.columns[column_i].get())[row_i];
            if (me != they) {
              cerr << std::setprecision(30) << endl;
              cerr << "== : DOUBLE column (" << relation.columns[column_i].name
                   << ") data are not identical\t"
                   << "row_i = " << row_i << endl
                   << me << endl
                   << they << endl;
              return false;
            }
            break;
          }
          case ColumnType::STRING: {
            auto me = this->operator()(column_i, row_i);
            auto they = other.operator()(column_i, row_i);
            if (me.length() != they.length() ||
                std::memcmp(me.data(), they.data(), me.length()) != 0) {
              cerr << "== : STRING column (" << relation.columns[column_i].name
                   << ") data are not identical\t"
                   << "row_i = " << row_i << endl;
              cerr << "me_size = \t" << me.length() << " - " << me << endl;
              cerr << "they_size = \t" << they.length() << " - " << they << endl;
              return false;
            }
            break;
          }
          default:
            throw Generic_Exception("Column type not supported");
        }
      }
    }
  }
  return true;
}

// -------------------------------------------------------------------------------------
Chunk::Chunk(unique_ptr<unique_ptr<u8[]>[]>&& columns,
             unique_ptr<unique_ptr<BITMAP[]>[]>&& bitmaps,
             unique_ptr<bool[]>&& column_requires_copy,
             u64 tuple_count,
             const Relation& relation,
             unique_ptr<SIZE[]>&& sizes)
    : relation(relation),
      columns(std::move(columns)),
      bitmaps(std::move(bitmaps)),
      column_requires_copy(std::move(column_requires_copy)),
      sizes(std::move(sizes)),
      tuple_count(tuple_count) {}

Chunk::Chunk(unique_ptr<unique_ptr<u8[]>[]>&& columns,
             unique_ptr<unique_ptr<BITMAP[]>[]>&& bitmaps,
             u64 tuple_count,
             const Relation& relation,
             unique_ptr<SIZE[]>&& sizes)
    : relation(relation),
      columns(std::move(columns)),
      bitmaps(std::move(bitmaps)),
      sizes(std::move(sizes)),
      tuple_count(tuple_count) {
  this->column_requires_copy = std::unique_ptr<bool[]>(new bool[tuple_count]);
  for (u64 idx = 0; idx < tuple_count; idx++) {
    column_requires_copy[idx] = false;
  }
}

// -------------------------------------------------------------------------------------
InputChunk::InputChunk(unique_ptr<u8[]>&& data,
                       unique_ptr<BITMAP[]>&& bitmap,
                       ColumnType type,
                       u64 tuple_count,
                       SIZE size)
    : data(std::move(data)),
      nullmap(std::move(bitmap)),
      type(type),
      size(size),
      tuple_count(tuple_count) {}

bool InputChunk::compareContents(u8* their_data,
                                 const std::vector<BITMAP>& their_bitmap,
                                 u64 their_tuple_count,
                                 bool requires_copy) const {
  if (their_tuple_count != this->tuple_count) {
    std::cerr << "Tuple count is not equal. Expected: " << this->tuple_count
              << ". Got: " << their_tuple_count << std::endl;
    return false;
  }

  for (u64 idx = 0; idx < their_tuple_count; idx++) {
    if (this->nullmap[idx] != their_bitmap[idx]) {
      std::cerr << "Bitmaps are not equal at index " << idx << ". Expected: " << this->nullmap[idx]
                << " Got: " << their_bitmap[idx] << std::endl;
      return false;
    }
  }

  switch (this->type) {
    case ColumnType::INTEGER: {
      if (requires_copy) {
        throw Generic_Exception("requires_copy not implemented for type INTEGER");
      }

      auto their_ints = reinterpret_cast<INTEGER*>(their_data);
      auto my_ints = reinterpret_cast<INTEGER*>(this->data.get());
      for (u64 idx = 0; idx < their_tuple_count; idx++) {
        if (this->nullmap[idx] && my_ints[idx] != their_ints[idx]) {
          std::cerr << "Integer data is not equal at index " << idx << " Expected: " << my_ints[idx]
                    << " Got: " << their_ints[idx] << std::endl;
          return false;
        }
      }
      break;
    }
    case ColumnType::DOUBLE: {
      if (requires_copy) {
        throw Generic_Exception("requires_copy not implemented for type DOUBLE");
      }

      auto their_doubles = reinterpret_cast<DOUBLE*>(their_data);
      auto my_doubles = reinterpret_cast<DOUBLE*>(this->data.get());
      for (u64 idx = 0; idx < their_tuple_count; idx++) {
        if (this->nullmap[idx] && my_doubles[idx] != their_doubles[idx]) {
          std::cerr << "Double data is not equal at index " << idx << std::setprecision(1000)
                    << " Expected: " << my_doubles[idx] << " Got: " << their_doubles[idx]
                    << std::endl;
          return false;
        }
      }
      break;
    }
    case ColumnType::STRING: {
      auto my_view = btrblocks::StringArrayViewer(this->data.get());
      for (u64 idx = 0; idx < their_tuple_count; idx++) {
        if (!this->nullmap[idx]) {
          continue;
        }

        str my_str = my_view(idx);
        str their_str;
        if (requires_copy) {
          auto their_view = btrblocks::StringPointerArrayViewer(their_data);
          their_str = their_view(idx);
        } else {
          auto their_view = btrblocks::StringArrayViewer(their_data);
          their_str = their_view(idx);
        }

        if (my_str.length() != their_str.length()) {
          std::cerr << "String lengths are not equal at index " << idx << " Expected: " << my_str
                    << " " << my_str.length() << " Got: " << their_str << " " << their_str.length()
                    << std::endl;
          return false;
        }

        if (my_str != their_str) {
          std::cerr << "Strings are not equal at index " << idx << " Expected: " << my_str
                    << " Got: " << their_str << std::endl;
          return false;
        }
      }
      break;
    }
    default:
      throw Generic_Exception("Type not implemented");
  }
  return true;
}

bool ColumnPart::canAdd(SIZE chunk_size) {
  if (chunk_size > chunk_part_size_threshold) {
    // This may appear in practice, but we ignore the problem for now.
    // Although writing will work, reading the data back in may break, as we
    // assume objects to always have a maximum size of part_size_threshold.
    throw std::logic_error(
        "chunks with compressed size greater than part_size_threshold "
        "unsupported");
  }
  return (total_size + chunk_size) <= chunk_part_size_threshold;
}

void ColumnPart::addCompressedChunk(vector<u8>&& chunk) {
  total_size += chunk.size();
  chunks.push_back(chunk);
}

u32 ColumnPart::writeToDisk(const std::string& outputfile) {
  std::ofstream btr_file(outputfile, std::ios::out | std::ios::binary);
  if (!btr_file.good()) {
    perror(outputfile.c_str());
    throw Generic_Exception("Opening btr output file failed");
  }

  struct ColumnPartMetadata metadata {
    .num_chunks = static_cast<u32>(this->chunks.size())
  };

  // We need to align the offsets by 16. Otherwise, PBP decompression breaks.
  u32 current_offset = sizeof(metadata) + this->chunks.size() * sizeof(u32);
  u64 diff;
  current_offset = Utils::alignBy(current_offset, 16, diff);
  std::vector<u32> offsets;
  std::vector<u64> diffs;
  for (const auto& chunk : this->chunks) {
    offsets.push_back(current_offset);
    diffs.push_back(diff);
    current_offset += chunk.size();
    current_offset = Utils::alignBy(current_offset, 16, diff);
  }

  // Write metadata
  btr_file.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));
  // Write offsets
  btr_file.write(reinterpret_cast<const char*>(offsets.data()), offsets.size() * sizeof(u32));
  // Write chunks
  for (std::size_t chunk_i = 0; chunk_i < chunks.size(); chunk_i++) {
    auto& chunk = this->chunks[chunk_i];
    btr_file.seekp(diffs[chunk_i], std::ios::cur);
    btr_file.write(reinterpret_cast<const char*>(chunk.data()), chunk.size());
  }

  u32 bytes_written = this->total_size + sizeof(metadata) + offsets.size() * sizeof(u32);
  btr_file.flush();
  btr_file.close();
  if (btr_file.fail()) {
    perror(outputfile.c_str());
    throw Generic_Exception("Closing btr file failed");
  }
  this->reset();

  return bytes_written;
}

void ColumnPart::reset() {
  this->total_size = 0;
  this->chunks.clear();
}
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
