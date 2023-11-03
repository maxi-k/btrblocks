#include "benchmark/benchmark.h"
#include "btrblocks.hpp"
#include "scheme/SchemePool.hpp"
#include "storage/MMapVector.hpp"
#include "storage/Relation.hpp"
// ---------------------------------------------------------------------------------------------------

using namespace btrblocks;
using namespace std;

namespace btrbench {

static const vector<string> string_datasets{"1_F1.string", "2_F2.string", "6_F6.string",
                                            "7_F7.string"};

static const std::vector<std::string> integer_datasets{
    "10_Semana.integer",
    "12_Venta_uni_hoy.integer",
    "1_Agencia_ID.integer",
    "2_Canal_ID.integer",
    "3_Cliente_ID.integer",
    "4_Demanda_uni_equil.integer",
    "6_Dev_uni_proxima.integer",
    "8_Producto_ID.integer",
    "9_Ruta_SAK.integer"
};

static const std::vector<std::string> double_datasets{
    "4_F4.double",
    "5_F5.double",
    "8_F8.double",
    "9_F9.double",
    "11_Venta_hoy.double",
    // "5_Dev_proxima.double"
};

static const vector<IntegerSchemeType> benchmarkedIntegerSchemes{
    IntegerSchemeType::DICT, IntegerSchemeType::RLE, IntegerSchemeType::PFOR,
    IntegerSchemeType::BP};

static const vector<DoubleSchemeType> benchmarkedDoubleSchemes{
    DoubleSchemeType::DICT, DoubleSchemeType::RLE, DoubleSchemeType::FREQUENCY,
    DoubleSchemeType::PSEUDODECIMAL};

static const vector<StringSchemeType> benchmarkedStringSchemes{StringSchemeType::DICT,
                                                               StringSchemeType::FSST};

static void SetupAllSchemes() {
  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  BtrBlocksConfig::get().doubles.schemes = defaultDoubleSchemes();
  BtrBlocksConfig::get().strings.schemes = defaultStringSchemes();

  SchemePool::refresh();
}

static void SetupIntSchemes(IntegerSchemeType scheme) {
  BtrBlocksConfig::get().integers.schemes = SchemeSet<IntegerSchemeType>({});
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::UNCOMPRESSED);
  BtrBlocksConfig::get().integers.schemes.enable(IntegerSchemeType::ONE_VALUE);
  BtrBlocksConfig::get().integers.schemes.enable(scheme);

  SchemePool::refresh();
}

static void SetupDoubleSchemes(DoubleSchemeType scheme) {
  BtrBlocksConfig::get().doubles.schemes = SchemeSet<DoubleSchemeType>({});
  BtrBlocksConfig::get().doubles.schemes.enable(DoubleSchemeType::UNCOMPRESSED);
  BtrBlocksConfig::get().doubles.schemes.enable(DoubleSchemeType::ONE_VALUE);
  BtrBlocksConfig::get().doubles.schemes.enable(scheme);

  SchemePool::refresh();
}

static void SetupStringSchemes(StringSchemeType scheme) {
  BtrBlocksConfig::get().strings.schemes = SchemeSet<StringSchemeType>({});
  BtrBlocksConfig::get().strings.schemes.enable(StringSchemeType::UNCOMPRESSED);
  BtrBlocksConfig::get().strings.schemes.enable(StringSchemeType::ONE_VALUE);
  BtrBlocksConfig::get().strings.schemes.enable(scheme);

  SchemePool::refresh();
}

static void BtrBlocksBenchmark(benchmark::State& state,
                               const string& dataset,
                               const function<void()>& setup) {
  setup();
  Relation relation;

  relation.addColumn(BENCHMARK_DATASET() + dataset);

  for (auto _ : state) {
    Datablock datablock(relation);

    auto ranges = relation.getRanges(btrblocks::SplitStrategy::SEQUENTIAL, 9999999);

    double comp_ratio_sum = 0.0;
    auto size = static_cast<double>(ranges.size());

    double uncompressed_data_size = 0l;
    double compressed_data_size = 0l;

    unordered_map<uint8_t, size_t> scheme_occurences{};

    vector<BytesArray> compressed_chunks;

    compressed_chunks.resize(ranges.size());

    for (u32 chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      auto chunk = relation.getChunk(ranges, chunk_i);
      auto db_meta = datablock.compress(chunk, compressed_chunks[chunk_i]);

      comp_ratio_sum += db_meta.compression_ratio;

      uncompressed_data_size += static_cast<double>(chunk.size_bytes());
      compressed_data_size += static_cast<double>(db_meta.total_data_size);

      auto& scheme = db_meta.used_compression_schemes[0];
      scheme_occurences[scheme] += 1;

      auto decompressed_chunk = datablock.decompress(compressed_chunks[chunk_i]);
    }

    state.counters["uncompressed_data_size"] =
        benchmark::Counter(uncompressed_data_size, benchmark::Counter::Flags::kAvgThreads);
    state.counters["compressed_data_size"] =
        benchmark::Counter(compressed_data_size, benchmark::Counter::Flags::kAvgThreads);

    double comp_ratio_avg = comp_ratio_sum / size;

    state.counters["comp_ratio_avg"] =
        benchmark::Counter(comp_ratio_avg, benchmark::Counter::Flags::kAvgThreads);

    double most_used_scheme =
        std::max_element(scheme_occurences.begin(), scheme_occurences.end(),
                         [](const pair<uint8_t, size_t>& p1, const pair<uint8_t, size_t>& p2) {
                           return p1.second - p2.second;
                         })
            ->second;
    state.counters["most_used_scheme"] = benchmark::Counter(most_used_scheme);
  }
}

void RegisterSingleBenchmarks() {
  //
  // Integer schemes
  //
  for (auto& dataset : integer_datasets) {
    // Run all schemes together
    benchmark::RegisterBenchmark(("all/" + dataset).c_str(), BtrBlocksBenchmark, dataset,
                                 SetupAllSchemes)
        ->UseRealTime()
        ->MinTime(10);

    // Run for all individually
    for (auto& scheme : benchmarkedIntegerSchemes) {
      auto name = ConvertSchemeTypeToString(scheme) + "/" + dataset;
      benchmark::RegisterBenchmark(name.c_str(), BtrBlocksBenchmark, dataset,
                                   [&]() { SetupIntSchemes(scheme); })
          ->UseRealTime()
          ->MinTime(10);
    }
  }
  //
  // Double schemes
  //
  for (auto& dataset : double_datasets) {
    // Run all schemes together
    benchmark::RegisterBenchmark(("all/" + dataset).c_str(), BtrBlocksBenchmark, dataset,
                                 SetupAllSchemes)
        ->UseRealTime()
        ->MinTime(30);

    // Run for all individually
    for (auto& scheme : benchmarkedDoubleSchemes) {
      auto name = ConvertSchemeTypeToString(scheme) + "/" + dataset;
      benchmark::RegisterBenchmark(name.c_str(), BtrBlocksBenchmark, dataset,
                                   [&]() { SetupDoubleSchemes(scheme); })
          ->UseRealTime()
          ->MinTime(10);
    }
  }
  //
  // String schemes
  //
  for (auto& dataset : string_datasets) {
    // Run all schemes together
    benchmark::RegisterBenchmark(("all/" + dataset).c_str(), BtrBlocksBenchmark, dataset,
                                 SetupAllSchemes)
        ->UseRealTime()
        ->MinTime(30);

    // Run for all individually
    for (auto& scheme : benchmarkedStringSchemes) {
      auto name = ConvertSchemeTypeToString(scheme) + "/" + dataset;
      benchmark::RegisterBenchmark(name.c_str(), BtrBlocksBenchmark, dataset,
                                   [&]() { SetupStringSchemes(scheme); })
          ->UseRealTime()
          ->MinTime(10);
    }
  }
}
}  // namespace btrbench