#pragma once
// -------------------------------------------------------------------------------------
#include "compression/Compressor.hpp"
#include "storage/Chunk.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
#include <unordered_map>
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
// Begin new chunking
struct ColumnChunkMeta {
  u8 compression_type;
  BitmapType nullmap_type;
  ColumnType type;
  // There is 1 unused Bytes here.
  u32 nullmap_offset = 0;
  u32 tuple_count;
  u8 data[];
};
static_assert(sizeof(ColumnChunkMeta) == 12);

struct ColumnPartInfo {
  ColumnType type;
  // There are 3 unused bytes here.
  u32 num_parts;
};
static_assert(sizeof(ColumnPartInfo) == 8);

struct FileMetadata {
  u32 num_columns;
  u32 num_chunks;
  struct ColumnPartInfo parts[];
};
static_assert(sizeof(FileMetadata) == 8);
// End new chunking
struct __attribute__((packed)) ColumnMeta {
  u8 compression_type;
  ColumnType column_type;
  BitmapType bitmap_type;
  u8 padding;
  u32 offset;
  s32 bias;
  u32 nullmap_offset = 0;
};
static_assert(sizeof(ColumnMeta) == 16);
// -------------------------------------------------------------------------------------
struct DatablockMeta {
  u32 count;
  u32 size;
  u32 column_count;
  u32 padding;
  ColumnMeta attributes_meta[];
};
static_assert(sizeof(DatablockMeta) == 16);
// -------------------------------------------------------------------------------------
class Datablock : public RelationCompressor {
 public:
  explicit Datablock(const Relation& relation);
  OutputBlockStats compress(const Chunk& input_chunk, BytesArray& output_block) override;
  Chunk decompress(const BytesArray& input_block) override;
  virtual void getCompressedColumn(const BytesArray& input_db, u32 col_i, u8*& ptr, u32& size);

  static bool decompress(const u8* data_in, BitmapWrapper** bitmap_out, u8* data_out);
  static vector<u8> compress(const InputChunk& input_chunk);
  static u32 writeMetadata(const std::string& path,
                           std::vector<ColumnType> types,
                           vector<u32> part_counters,
                           u32 num_chunks);
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
