// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/Units.hpp"
#include "common/PerfEvent.hpp"
// -------------------------------------------------------------------------------------
#include "compression/SchemePicker.hpp"
#include "cache/ThreadCache.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include <gflags/gflags.h>
#include <spdlog/spdlog.h>
// -------------------------------------------------------------------------------------
#include <future>
#include <iostream>
#include <unordered_map>
#include <filesystem>
#include <fstream>
#include <string>
// -------------------------------------------------------------------------------------
using namespace std::string_literals;
using namespace btrblocks;
// -------------------------------------------------------------------------------------
DEFINE_int32(bm_max_cascade_depth, 3, "maximum compression cascade depth.");
DEFINE_int32(bm_sample_count, 10, "sample count");
DEFINE_int32(bm_sample_size, 64, "sample size");
DEFINE_int32(bm_skip_files, 0, "skip these many files from the input file list");
DEFINE_int32(bm_file_count, 20, "test these many files");
DEFINE_string(bm_input_file_list, "s3-columns.txt", "the file listing the s3 input objects");
DEFINE_bool(bm_clear_filecache, false, "whether to remove s3 dowloaded files");
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
struct InputFiles {
    std::ifstream list;
    InputFiles(const std::string& filename) : list(filename) {
        spdlog::info("s3 list file %s", filename);
    }

    bool next(std::string& output) {
        return !(std::getline(list, output).fail());
    }
};
// -------------------------------------------------------------------------------------
std::string ensure_file(const std::string& object) {
  static const std::string bucket = "s3://public-bi-benchmark/binary/"s;
  std::string outfile = "columns/"s + object;
  std::stringstream _cmd;
  _cmd << "bash -c '(mkdir -p columns; test -f \"" << outfile
       << "\" && echo \"file exists, skipping download\" || (echo "
          "\"downloading file\"; aws s3 cp \""
       << bucket << object << "\" \"" << outfile << "\")) 1>&2'";
  std::string cmd(_cmd.str());
  spdlog::info("running {}", cmd);
  system(cmd.c_str());
  return outfile;
}
// -------------------------------------------------------------------------------------
static constexpr size_t MAX_PBI_BYTES = 2019996069;
static constexpr size_t BLOCK_SIZE = 65000;
// -------------------------------------------------------------------------------------
struct TestResult {

    friend std::ostream& operator<<(std::ostream& s, const TestResult&) {
        return s << "{}";
    }
};
// -------------------------------------------------------------------------------------
template<typename SchemeEnum, typename Scheme>
using pool_t = std::unordered_map<SchemeEnum, unique_ptr<Scheme>>;
// -------------------------------------------------------------------------------------
template<typename T>
struct Schemes{};
template <>
struct Schemes<INTEGER> {
  static constexpr char name[] = "integer";
  using picker = IntegerSchemePicker;
  using stats = SInteger32Stats;
  using scheme = IntegerScheme;
  using scheme_enum = IntegerSchemeType;
  using pool = pool_t<scheme_enum, scheme>;

  static const pool& schemes() {
      return SchemePool::available_schemes->integer_schemes;
  }

  static const std::set<scheme_enum> excluded() {
    //return {};
    return std::set({ IntegerSchemeType::ONE_VALUE });
  }
};
// -------------------------------------------------------------------------------------
template <>
struct Schemes<DOUBLE> {
  static constexpr char name[] = "double";
  using picker = DoubleSchemePicker;
  using stats = DoubleStats;
  using scheme = DoubleScheme;
  using scheme_enum = DoubleSchemeType;
  using pool = pool_t<scheme_enum, scheme>;

  static const pool& schemes() {
      return SchemePool::available_schemes->double_schemes;
  }

  static const std::set<scheme_enum> excluded() {
    //return {};
    return std::set({ DoubleSchemeType::ONE_VALUE });
  }
};
// -------------------------------------------------------------------------------------
template <>
struct Schemes<std::string_view> {
  static constexpr char name[] = "string";
  using picker = StringSchemePicker;
  using stats = StringStats;
  using scheme = StringScheme;
  using scheme_enum = StringSchemeType;
  using pool = pool_t<scheme_enum, scheme>;

  static const pool& schemes() {
      return SchemePool::available_schemes->string_schemes;
  }
  static const std::set<scheme_enum> excluded() {
    //return {};
    return std::set({ StringSchemeType::ONE_VALUE });
  }
};
// -------------------------------------------------------------------------------------
template<typename T>
using sample_t = std::tuple<std::vector<T>, std::vector<BITMAP>>;
// -------------------------------------------------------------------------------------
template<typename T>
struct Sampler {
  using types = Schemes<T>;
  using type = sample_t<T>;
  using stats_t = typename types::stats;
  virtual std::string name() const = 0;
  virtual u32 sampled_items() const = 0;
  virtual sample_t<T> operator()(const T* input, size_t count, stats_t& stats) const = 0;
};
// -------------------------------------------------------------------------------------
template<typename T>
struct RandomSampler : Sampler<T> {
  using stats_t = typename Schemes<T>::stats;
  RandomSampler(const std::string& name, u32 sample_size, u32 sample_count)
    : _name(name)
    , sample_size(sample_size)
    , sample_count(sample_count){}

  RandomSampler(u32 sample_size, u32 sample_count)
    : _name("r" + std::to_string(sample_count) + "x" +
                          std::to_string(sample_size))
    , sample_size(sample_size)
    , sample_count(sample_count){}

  RandomSampler() : RandomSampler(cfg().sample_size, cfg().sample_count) {}

  std::string name() const override { return _name; }
  u32 sampled_items() const override { return sample_size * sample_count; }

  sample_t<T> operator()([[maybe_unused]] const T* input, [[maybe_unused]] size_t count, stats_t& stats) const override {
    cfg().sample_count = this->sample_count;
    cfg().sample_size = this->sample_size;
    return stats.samples(this->sample_count, this->sample_size);
  }

  static BtrBlocksConfig& cfg() {
    return BtrBlocksConfig::get();
  }

  std::string _name;
  u32 sample_size, sample_count;
};
// -------------------------------------------------------------------------------------
template<typename T>
TestResult testSampling(const std::string& filename, const std::string& bitmap_file, u8* output, PerfEvent& e) {
    using types = Schemes<T>;
    //using Picker = typename types::picker;
    using Stats = typename types::stats;
    //using Scheme = typename types::scheme;
    auto& cache = ThreadCache::get();
    auto excluded = types::excluded();
    const Vector<T> infile(filename.c_str());
    const Vector<BITMAP> bitmap(bitmap_file.c_str());

    assert(infile.size() == bitmap.size());
    size_t blocksize = std::min(infile.size(), BLOCK_SIZE);

    e.setParam("file", "\"" + filename + "\"");
    e.setParam("scheme", "sampling");
    e.setParam("type", types::name);
    e.setParam("insize", blocksize * sizeof(T)); // TODO strings

    e.setParam("sampling", "full");
    e.setParam("sample_size", blocksize);
    e.setParam("sample_count", 1);
    e.setParam("sampled_items", blocksize);
    Stats whole_stats = Stats::generateStats(infile.data, bitmap.data, blocksize);
    for (auto& [stype, scheme] : types::schemes()) {
      if (excluded.find(stype) != excluded.end()) { continue; }
      u32 outsize;
      e.setParam("scheme", scheme->selfDescription());
      {
        PerfEventBlock blk(e, 1);
        outsize = scheme->compress(infile.data, bitmap.data, output, whole_stats, FLAGS_bm_max_cascade_depth);
        e.setParam("outsize", outsize);
        e.setParam("compr", ((double)blocksize * sizeof(T))/((double)outsize));
      }
      std::fill(output, output + outsize, 0);
    }

    //std::array counts  = {1, 10, 100, 1000};
    //std::array sizes = {1, 16, 64, 256, 1024, 4096};

    //std::array counts  = {1, 10, 100, 1000};
    //std::array sizes = {1, 16, 64, 256, 1024, 4096};
  //   std::vector<std::pair<int, int>> combinations = {
  //   {1, 64}, {5, 64}, {10, 64}, {15, 64}, {20, 64}, {25, 64}, {30, 64}, {35, 64}, {40, 64}, {45, 64}, {50, 64},
  //   {10, 1},
  //   {10, 8},
  //   {10, 16},
  //   {10, 32},
  //   /*{10, 64},*/
  //   {10, 96},
  //   {10, 128},
  //   {10, 192},
  //   {10, 256},
  // };
    std::vector<std::pair<int, int>> combinations = {
        {640, 1}, {320, 2}, {160, 4}, {80, 8},  {40, 16},
        {20, 32}, {10, 64}, {5, 128}, {1, 640},
    };

    for (auto& [sample_count, sample_size] : combinations) {
    //  for (auto sample_count : counts) {
        RandomSampler<T> sampler(sample_size, sample_count);
        sample_t<T> sample_result;
        sample_result = sampler(infile.data, blocksize, whole_stats);
        auto& [sample, sample_nulls] = sample_result;
        Stats sample_stats = Stats::generateStats(
            sample.data(), sample_nulls.data(), sample.size());
        for (auto& [stype, scheme] : types::schemes()) {
          if (excluded.find(stype) != excluded.end()) {
            continue;
          }
          e.setParam("scheme", scheme->selfDescription());
          e.setParam("sampling", sampler.name());
          e.setParam("sample_size", sample_size);
          e.setParam("sample_count", sample_count);
          e.setParam("sampled_items", sampler.sampled_items());
          {
            PerfEventBlock blk(e, 1);
            auto outsize = scheme->compress(sample_stats.src, sample_stats.bitmap, output, sample_stats, FLAGS_bm_max_cascade_depth);
            double estimate = CD(sample_count * sample_size * sizeof(T)) / CD(outsize);
            e.setParam("compr", estimate);
          }
        }
      //}
    }
    return TestResult{};
}
// -------------------------------------------------------------------------------------
using test_fn_t = std::function<TestResult(const std::string&, const std::string&, u8*, PerfEvent&)>;
// -------------------------------------------------------------------------------------
std::unordered_map<std::string, std::pair<test_fn_t, u8>> FILE_TYPE_MAP = {
{".double"s, {testSampling<DOUBLE>, 7}},
{"integer"s, {testSampling<INTEGER>, 8}},
//{".string", testSampling<std::string_view>},
};
static constexpr u8 TYPE_MAPPING_SUFFIX_LENGTH = 7;
// -------------------------------------------------------------------------------------
int main(int argc, char *argv[]) {
  gflags::ParseCommandLineFlags(&argc, &argv, true);
  spdlog::set_level(spdlog::level::warn);
  SchemePool::refresh();
  PerfEvent perf;

  InputFiles filelist(FLAGS_bm_input_file_list);
  std::string s3file;
  size_t bufsize = 2 * MAX_PBI_BYTES / sizeof(size_t);
  size_t* scratchbuf = new size_t[bufsize];

  for (auto i = 0; i != FLAGS_bm_skip_files;) {
    filelist.next(s3file);
    i += (s3file[0] != '#');
  }
  int count = 0;
  while (filelist.next(s3file)) {
    if (s3file[0] == '#') { continue; }
    if (count>FLAGS_bm_file_count) { break; } //only some files

    //std::fill(scratchbuf, scratchbuf + bufsize, 0);

    TestResult res;

    auto typestr = s3file.substr(s3file.length() - TYPE_MAPPING_SUFFIX_LENGTH, s3file.length());
    auto typedata = FILE_TYPE_MAP.find(typestr);
    if (typedata == FILE_TYPE_MAP.end()) {
      spdlog::debug("skipping file %s with suffix %s", s3file, typestr);
      continue;
    } else {
      try {
        ++count;
        auto& [fn, substr] = typedata->second;
        auto bitmap_s3 = s3file.substr(0, s3file.length() - substr) + ".bitmap";
        auto [infile, bitmap_file] = [&] {
          auto f1 = std::async(std::launch::async,
                               [&] { return ensure_file(s3file); });
          auto f2 = std::async(std::launch::async,
                               [&] { return ensure_file(bitmap_s3); });
          return std::make_pair(f1.get(), f2.get());
        }();
        res = fn(infile, bitmap_file, reinterpret_cast<u8*>(scratchbuf), perf);
        if (FLAGS_bm_clear_filecache || count > 800) {
          std::filesystem::remove(infile);
        }
      } catch (std::exception& e) {
        std::cerr << s3file << ":" << e.what() << std::endl;
      }
    }

    //std::cout << res << std::endl;

  }
  delete[] scratchbuf;
  return 0;
}
// -------------------------------------------------------------------------------------
