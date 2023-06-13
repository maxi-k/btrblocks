#include "TestHelper.hpp"
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
void TestHelper::CheckRelationCompression(Relation &relation, RelationCompressor &compressor, const vector<u8> expected_compression_schemes)
{
   auto ranges = relation.getRanges(btrblocks::SplitStrategy::SEQUENTIAL, 999999);
   vector<BytesArray> compressed_chunks;
   compressed_chunks.resize(ranges.size());
   for ( u32 chunk_i = 0; chunk_i < ranges.size(); chunk_i++ ) {
      auto chunk = relation.getChunk(ranges, chunk_i);
      auto db_meta = compressor.compress(chunk, compressed_chunks[chunk_i]);
      if ( expected_compression_schemes.size()) {
         //auto scheme_cnt = 0;
         //for (auto e : expected_compression_schemes) {
         //   for (auto x : db_meta.used_compression_schemes) {
         //      scheme_cnt += e == x;
         //   }
         //}
         //EXPECT_EQ(scheme_cnt, expected_compression_schemes.size());
      }
      ASSERT_GE(db_meta.compression_ratio, 0.95);
      cout << "db_meta.compression_ratio  = " << db_meta.compression_ratio << endl;

      auto decompressed_chunk = compressor.decompress(compressed_chunks[chunk_i]);
      ASSERT_TRUE(decompressed_chunk == chunk);
   }
}
// -------------------------------------------------------------------------------------

// -------------------------------------------------------------------------------------
