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
// -------------------------------------------------------------------------------------
#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bundled/ranges.h>
// -------------------------------------------------------------------------------------
#include <string>
#include <fstream>
#include <sstream>
// -------------------------------------------------------------------------------------
DEFINE_string(file_list_file, "pbi-double-columns.txt", "file-list");
DEFINE_int32(cascade_depth, 3, "cascade");
DEFINE_int32(chunk_size, 1 << 10, "chunk_size");
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
  _cmd << "bash -c 'mkdir -p columns; test -f \"" << outfile
       << "\" && echo \"file exists, skipping download\" || (echo "
          "\"downloading file\"; aws s3 cp \""
       << bucket << object << "\" \"" << outfile << "\"" << " --no-sign" << ")'";
  std::string cmd(_cmd.str());
  // spdlog::info("running {}", cmd);
  system(cmd.c_str());
  return outfile;
}
// -------------------------------------------------------------------------------------
// note: previous the whole thing was compressed as one block
// now we have chunked it up into CHUNK_SIZE tuples at a time
// therefore it gets complicated to test if the result is
// correct
// TODO: maybe i will add this in the future but for now this is fine
using T = double;
bool test_compression(DoubleScheme &scheme, T* src, size_t size, PerfEvent& e, u8 cascade, size_t chunk_size) {
    std::vector<u8> compressed(chunk_size * sizeof(T) * 2);

    auto compressed_ptr = reinterpret_cast<u8 *>(compressed.data());

    size_t output_bytes{0};
    e.setParam("phase", "compression");
    {
      PerfEventBlock blk(e, size);
      for (size_t t = 0; t < size; t += chunk_size) {
        DOUBLE* data = src + t;
        size_t count = t + chunk_size < size ? chunk_size : size % chunk_size;
        DoubleStats stats(data, nullptr, count);
        stats = DoubleStats::generateStats(data, nullptr, count);

        output_bytes += scheme.compress(data, nullptr, compressed_ptr, stats, cascade);
      }
      //std::cout << "cf: " << 1.0 * size * sizeof(T) / output_bytes << std::endl;
      e.setParam("compr", (1.0 * size * sizeof(T)) / output_bytes);
    }

    if (output_bytes == std::numeric_limits<uint32_t>::max()) {
      std::cerr << "Compression with " << ConvertSchemeTypeToString(scheme.schemeType()) << " failed." << std::endl;
      return 1;
    }

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
    setupSchemePool();
    Log::set_level(Log::level::info);
    PerfEvent perf;
    // std::cerr << "using cascade depth " << FLAGS_cascade_depth << " and input file " << FLAGS_file_list_file << std::endl;

    InputFiles file_list(FLAGS_file_list_file);

    std::string nextfile;
    int i = 0;
    while (file_list.next(nextfile)) {
      if (i++ > 10) { break; }
      std::string outfile = ensure_file(nextfile);

      size_t cascade = FLAGS_cascade_depth;
      size_t chunk_size = FLAGS_chunk_size;

      Vector<T> doubles(outfile.c_str());
      // doubles.count = std::min(1UL << 12, doubles.size());

      {
        std::vector<T> head(doubles.data, doubles.data + std::min(10ul, doubles.size()));
        // spdlog::info("size: {:03.2f} MiB, head: {}", (sizeof(T) * doubles.size()) * 1.0 / 1024 / 1024, head);
      }

      perf.setParam("column", nextfile);
      perf.setParam("scheme", "none");
      perf.setParam("compr", 1);
      perf.setParam("chunk_size", chunk_size);
      perf.setParam("cascade", cascade);

      perf.setParam("scheme", "bitpack");
      doubles::DoubleBP bp;
      test_compression(bp, doubles.data, doubles.count, perf, cascade, chunk_size);

      perf.setParam("scheme", "decimal");
      doubles::Decimal pd;
      test_compression(pd, doubles.data, doubles.count, perf, cascade, chunk_size);

      perf.setParam("scheme", "alp");
      doubles::Alp alp;
      test_compression(alp, doubles.data, doubles.count, perf, cascade, chunk_size);

      perf.setParam("scheme", "alprd");
      doubles::AlpRD alprd;
      test_compression(alprd, doubles.data, doubles.count, perf, cascade, chunk_size);


      perf.setParam("scheme", "dict");
      doubles::DynamicDictionary dict;
      test_compression(dict, doubles.data, doubles.count, perf, cascade, chunk_size);


      perf.setParam("scheme", "rle");
      doubles::RLE rle;
      test_compression(rle, doubles.data, doubles.count, perf, cascade, chunk_size);


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
