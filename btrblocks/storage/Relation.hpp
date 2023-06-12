#pragma once
#include "Column.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
enum class SplitStrategy : u8 { SEQUENTIAL, RANDOM };
// -------------------------------------------------------------------------------------
class Chunk;
class InputChunk;
// -------------------------------------------------------------------------------------
using Range = tuple<u64, u64>;
// -------------------------------------------------------------------------------------
class Relation {
 public:
  string name;
  u64 tuple_count;
  vector<Column> columns;
  Relation();
  [[nodiscard]] vector<Range> getRanges(btrblocks::SplitStrategy strategy,
                                        u32 max_chunk_count) const;

  [[nodiscard]] Chunk getChunk(const vector<Range>& ranges, SIZE chunk_i) const;
  [[nodiscard]] InputChunk getInputChunk(const Range& range,
                                         [[maybe_unused]] SIZE chunk_i,
                                         u32 column) const;

  void addColumn(Column&& column);
  void addColumn(const string& column_file_path);

 private:
  void fixTupleCount();
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
