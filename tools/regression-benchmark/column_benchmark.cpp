// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/Units.hpp"
#include "common/PerfEvent.hpp"
// -------------------------------------------------------------------------------------
#include "compression/SchemePicker.hpp"
#include "scheme/double/DynamicDictionary.hpp"
#include "scheme/double/Frequency.hpp"
#include "scheme/double/RLE.hpp"
#include "scheme/double/DoubleBP.hpp"
#include "scheme/double/AlpRD.hpp"
#include "scheme/double/Alp.hpp"
#include "scheme/integer/PBP.hpp"
#include "scheme/double/Pseudodecimal.hpp"
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemePool.hpp"
#include "compression/BtrReader.hpp"
// -------------------------------------------------------------------------------------
#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
// -------------------------------------------------------------------------------------
#include <string>
#include <fstream>
#include <sstream>

#include "tbb/global_control.h"
#include "tbb/parallel_for.h"
// -------------------------------------------------------------------------------------
DEFINE_string(file_list_file, "pbi-double-columns.txt", "file-list");
DEFINE_string(btr, "btr", "chunk_size");
DEFINE_int32(threads, -1, "Number of threads used. not specifying lets tbb decide");
DEFINE_bool(verify, true, "Verify that decompression works");
// -------------------------------------------------------------------------------------
// example2.double: s3://public-bi-benchmark/binary/Telco/1/Telco_1/106_RECHRG_INC_MIN_USED_P1.double
// example2.bitmap: s3://public-bi-benchmark/binary/Telco/1/Telco_1/106_RECHRG_INC_MIN_USED_P1.bitmap
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
struct InputFiles {
  std::ifstream list;
  InputFiles(const std::string& filename) : list(filename) {
    std::cout << "file " << filename << std::endl;
  }

  bool next(std::string& output) {
    return !(std::getline(list, output).fail());
  }
};
// -------------------------------------------------------------------------------------
std::string ensure_file(const std::string& object) {
  static const std::string bucket = "s3://public-bi-benchmark/binary/";
  std::string outfile = "columns/" + object;
  std::stringstream _cmd;
  _cmd << "bash -c 'mkdir -p columns; mkdir -p " << FLAGS_btr << "; test -f \"" << outfile
       << "\" || (aws s3 cp \""
       << bucket << object << "\" \"" << outfile << "\"" << " --no-sign 2>&1 > /dev/null" << ")'";
  std::string cmd(_cmd.str());
  // spdlog::info("running {}", cmd);
  system(cmd.c_str());
  return outfile;
}

// ------------------------------------------------------------------------------
void verify_or_die(const std::string& filename, const std::vector<InputChunk> &input_chunks) {
  if (!FLAGS_verify) {
    return;
  }
  // Verify that decompression works
  thread_local std::vector<char> compressed_data;
  Utils::readFileToMemory(filename, compressed_data);
  BtrReader reader(compressed_data.data());
  for (SIZE chunk_i = 0; chunk_i < reader.getChunkCount(); chunk_i++) {
    std::vector<u8> output(reader.getDecompressedSize(chunk_i));
    bool requires_copy = reader.readColumn(output, chunk_i);
    auto bm = reader.getBitmap(chunk_i)->writeBITMAP();
    if (!input_chunks[chunk_i].compareContents(output.data(), bm, reader.getTupleCount(chunk_i),
                                               requires_copy)) {
      throw Generic_Exception("Decompression yields different contents");
    }
  }
}

// -------------------------------------------------------------------------------------
// note: previous the whole thing was compressed as one block
// now we have chunked it up into CHUNK_SIZE tuples at a time
// therefore it gets complicated to test if the result is
// correct

vector<tuple<u64, u64>> getRanges(btrblocks::SplitStrategy strategy,
                                            u32 tuple_count) {
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
  return ranges;
}

// todo: template it in the future
InputChunk getInputChunk(const Range& range,
                         [[maybe_unused]] SIZE chunk_i,
                         DOUBLE* double_data
                         ) {
  auto offset = std::get<0>(range);
  auto chunk_tuple_count = std::get<1>(range);

  auto bitmap = std::unique_ptr<BITMAP[]>(new BITMAP[chunk_tuple_count]);

  // todo: this is the ugliest thing on earth
  //        1. read the bitmap file with the other file
  //        2. more efficient way to set every value to 1 in the bitmap array
  for (size_t i = 0; i < chunk_tuple_count; i++) {
    bitmap[i] = 1;
  }

  SIZE size = chunk_tuple_count * sizeof(DOUBLE);
  std::unique_ptr<u8[]> data = std::unique_ptr<u8[]>(new u8[size]);
  std::memcpy(reinterpret_cast<void*>(data.get()), double_data + offset,
              chunk_tuple_count * sizeof(DOUBLE));

  return {std::move(data), std::move(bitmap), ColumnType::DOUBLE, chunk_tuple_count, size};
}

const std::string path_prefix = FLAGS_btr + "/" + "column_part";

size_t compress(DOUBLE* uncompressed_data, SIZE tuple_count, PerfEvent& e) {
  auto ranges = getRanges(SplitStrategy::SEQUENTIAL, tuple_count);
  size_t uncompressed_size = 0;
  size_t compressed_size = 0;
  u32 part_counter = 0;

  std::vector<InputChunk> input_chunks;
  ColumnPart part;
  e.setParam("phase", "compression");
  {
    PerfEventBlock blk(e, tuple_count);
    for (SIZE chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      auto input_chunk = getInputChunk(ranges[chunk_i], chunk_i, uncompressed_data);
      std::vector<u8> compressed_data = Datablock::compress(input_chunk);
      uncompressed_size += input_chunk.size;
      if (!part.canAdd(compressed_data.size())) {
        std::string filename = path_prefix + std::to_string(part_counter);
        part_counter++;
        compressed_size += part.writeToDisk(filename);
        verify_or_die(filename, input_chunks);
        input_chunks.clear();
      }

      input_chunks.push_back(std::move(input_chunk));
      part.addCompressedChunk(std::move(compressed_data));
    }

    if (!part.chunks.empty()) {
      std::string filename = path_prefix + std::to_string(part_counter);
      part_counter++;
      compressed_size += part.writeToDisk(filename);
      verify_or_die(filename, input_chunks);
      input_chunks.clear();
    }

    e.setParam("compr_size", compressed_size);
    e.setParam("uncompr_size", uncompressed_size);
    e.setParam("factor", (1.0 * uncompressed_size) / compressed_size);
  }

  return part_counter;
}

// -------------------------------------------------------------------------------------
void measure_decompression(size_t num_parts, std::vector<BtrReader> &readers) {
  tbb::parallel_for(size_t(0), num_parts, [&](size_t part_i) {
    auto &reader = readers[part_i];
    tbb::parallel_for(u32(0), reader.getChunkCount(), [&](u32 chunk_i) {
      thread_local std::vector<u8> decompressed_data;
      reader.readColumn(decompressed_data, chunk_i);
    });
  });
}

void decompress(size_t num_parts, size_t tuple_count, PerfEvent& e) {
  std::vector<std::vector<char>> compressed_data(num_parts);
  std::vector<BtrReader> readers{};

  for (u32 part_i = 0; part_i < num_parts; part_i++) {
    auto path = path_prefix + std::to_string(part_i);
    Utils::readFileToMemory(path, compressed_data[part_i]);
    readers.emplace_back(compressed_data[part_i].data());
  }

  // Measure once to make sure all buffers are allocated properly
  measure_decompression(num_parts, readers);

  e.setParam("phase", "decompression");
  {
    PerfEventBlock blk(e, tuple_count);
    measure_decompression(num_parts, readers);
  }
}


bool test_compression(DOUBLE* src, size_t size, PerfEvent& e) {
  size_t num_parts = compress(src, size, e);

  decompress(num_parts, size, e);

  return 0;
}
// -------------------------------------------------------------------------------------
void setupSchemePool() {
  SchemePool::refresh();
  auto& schemes = *SchemePool::available_schemes;
  return;
  // double: DOUBLE_BP, UNCOMPRESSED,
  for (auto it = schemes.double_schemes.begin();
       it != schemes.double_schemes.end();) {
    if (it->first != DoubleSchemeType::DOUBLE_BP &&
        it->first != DoubleSchemeType::UNCOMPRESSED) {
      it = schemes.double_schemes.erase(it);
    } else {
      ++it;
    }
  }
  // int: X_FBP, UNCOMPRESSED,
  for (auto it = schemes.integer_schemes.begin(); it != schemes.integer_schemes.end();) {
    if (it->first != IntegerSchemeType::BP &&
        it->first != IntegerSchemeType::UNCOMPRESSED) {
      it = schemes.integer_schemes.erase(it);
    } else {
      ++it;
    }
  }
}
// -------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  std::filesystem::path btr_dir = FLAGS_btr;

  SchemePool::refresh();

  int threads;
  if (FLAGS_threads < 1) {
    // Automatic selection
    tbb::global_control c(tbb::global_control::max_allowed_parallelism,
                          std::thread::hardware_concurrency());
  } else {
    threads = FLAGS_threads;
    tbb::global_control c(tbb::global_control::max_allowed_parallelism, threads);
  }

  gflags::ParseCommandLineFlags(&argc, &argv, true);
  setupSchemePool();
  Log::set_level(Log::level::info);
  PerfEvent perf;
  // std::cerr << "using cascade depth " << FLAGS_cascade_depth << " and input file " << FLAGS_file_list_file << std::endl;

  InputFiles file_list(FLAGS_file_list_file);

  std::string nextfile;
  int i = 0;
  while (file_list.next(nextfile)) {
    // if (i++ > 10) { break; }
    std::string outfile = ensure_file(nextfile);

    Vector<DOUBLE> doubles(outfile.c_str());
    // doubles.count = std::min(1UL << 12, doubles.size());

    perf.setParam("column", nextfile);

    test_compression(doubles.data, doubles.count, perf);


    //perf.setParam("scheme", "freq");
    //legacy::doubles::Frequency freq;
    //test_compression(freq, stats, doubles.data, doubles.count, perf, 1);
    //test_compression(freq, stats, doubles.data, doubles.count, perf, 2);
    //test_compression(freq, stats, doubles.data, doubles.count, perf, 3);
    //test_compression(freq, stats, doubles.data, doubles.count, perf, 1);
    //test_compression(freq, stats, doubles.data, doubles.count, perf, 2);
  }
}
// -------------------------------------------------------------------------------------

