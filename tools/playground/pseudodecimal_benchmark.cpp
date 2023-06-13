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
DEFINE_int32(cascade_depth, 1, "cascade");
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
       << bucket << object << "\" \"" << outfile << "\")'";
  std::string cmd(_cmd.str());
  spdlog::info("running {}", cmd);
  system(cmd.c_str());
  return outfile;
}
// -------------------------------------------------------------------------------------
using T = double;
bool test_compression(DoubleScheme &scheme, DoubleStats& stats, T* src, size_t size, PerfEvent& e, u8 cascade) {
    std::vector<u8> compressed(size * sizeof(T) * 2);
    std::vector<T> dst(size * 2, 0);

    auto src_ptr = src;
    auto compressed_ptr = reinterpret_cast<u8 *>(compressed.data());
    auto dst_ptr = dst.data();

    size_t output_bytes{0};
    e.setParam("cascade", cascade);
    e.setParam("phase", "compression");
    {
      PerfEventBlock blk(e, size);
      output_bytes = scheme.compress(src_ptr, nullptr, compressed_ptr, stats, cascade);
      //std::cout << "cf: " << 1.0 * size * sizeof(T) / output_bytes << std::endl;
      e.setParam("compr", (1.0 * size * sizeof(T)) / output_bytes);
    }

    e.setParam("phase", "decompression");
    {
      PerfEventBlock blk(e, size);
      scheme.decompress(dst_ptr, nullptr, compressed_ptr, stats.tuple_count, cascade);
    }
    //std::cerr << "Decompression done." << std::endl;
    for (auto i = 0ul; i != size; ++i) {
        die_if(src[i] == dst[i]);
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
    std::cerr << "using cascade depth " << FLAGS_cascade_depth << " and input file " << FLAGS_file_list_file << std::endl;

    InputFiles file_list(FLAGS_file_list_file);

    std::string nextfile;
    int i = 0;
    while (file_list.next(nextfile)) {
      if (i++ > 10) { break; }
      std::string outfile = ensure_file(nextfile);

      Vector<T> doubles(outfile.c_str());

      {
        std::vector<T> head(doubles.data, doubles.data + std::min(10ul, doubles.size()));
        spdlog::info("size: {:03.2f} MiB, head: {}", (sizeof(T) * doubles.size()) * 1.0 / 1024 / 1024, head);
      }

      perf.setParam("column", nextfile);
      perf.setParam("scheme", "none");
      perf.setParam("compr", 1);
      perf.setParam("cascade", 0);

      DoubleStats stats(doubles.data, nullptr, doubles.size());
      perf.setParam("phase", "stats");
      {
        PerfEventBlock blk(perf, doubles.size());
        stats = DoubleStats::generateStats(doubles.data, nullptr, doubles.size());
      }

      perf.setParam("scheme", "bitpack");
      doubles::DoubleBP bp;
      test_compression(bp, stats, doubles.data, doubles.count, perf, 0);

      perf.setParam("scheme", "decimal");
      doubles::Decimal pd;
      test_compression(pd, stats, doubles.data, doubles.count, perf, 1);
      test_compression(pd, stats, doubles.data, doubles.count, perf, 2);

      perf.setParam("scheme", "dict");
      doubles::DynamicDictionary dict;
      test_compression(dict, stats, doubles.data, doubles.count, perf, 1);
      test_compression(dict, stats, doubles.data, doubles.count, perf, 2);

      perf.setParam("scheme", "rle");
      doubles::RLE rle;
      test_compression(rle, stats, doubles.data, doubles.count, perf, 1);
      test_compression(dict, stats, doubles.data, doubles.count, perf, 2);

      //perf.setParam("scheme", "freq");
      //doubles::Frequency freq;
      //test_compression(freq, stats, doubles.data, doubles.count, perf, 1);
      //test_compression(freq, stats, doubles.data, doubles.count, perf, 2);
    }
}
// -------------------------------------------------------------------------------------
