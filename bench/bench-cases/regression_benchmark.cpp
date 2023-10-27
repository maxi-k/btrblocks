#include "BenchmarkHelper.hpp"
#include "benchmark/benchmark.h"
#include "btrblocks.hpp"
#include "scheme/SchemePool.hpp"
#include "storage/MMapVector.hpp"
#include "storage/Relation.hpp"
// ---------------------------------------------------------------------------------------------------
namespace btrbench {

static const std::array<std::string, 4> string_datasets{
    "1_F1.string",
    "2_F2.string",
    "6_F6.string",
    "7_F7.string"
};

static const std::array<std::string, 11> integer_datasets{
    "10_Number of Records.integer",
    "11_WNET (bin).integer",
    "10_Semana.integer",
    "1_Agencia_ID.integer",
    "2_Canal_ID.integer",
    "3_Cliente_ID.integer",
    "4_Demanda_uni_equil.integer",
    "6_Dev_uni_proxima.integer",
    "7_Number of Records.integer",
    "8_Producto_ID.integer",
    "9_Ruta_SAK.integer"
};

static const std::array<std::string, 3> double_datasets{
    "4_F4.double",
    "11_Venta_hoy.double",
    "5_Dev_proxima.double"
};

static void SetupSchemes(const benchmark::State& state) {
  BtrBlocksConfig::get().integers.schemes.enableAll();
  BtrBlocksConfig::get().integers.schemes.disable(IntegerSchemeType::TRUNCATION_8);
  BtrBlocksConfig::get().integers.schemes.disable(IntegerSchemeType::TRUNCATION_16);
  BtrBlocksConfig::get().doubles.schemes.enableAll();
  BtrBlocksConfig::get().strings.schemes.enableAll();
  SchemePool::refresh();
}

static void BtrBlocksBenchmark(benchmark::State& state, const std::string& dataset) {
  SetupSchemes(state);
  Relation relation;

  relation.addColumn(BENCHMARK_DATASET() + dataset);

  for (auto _ : state) {
    double comp_ratio_sum = 0.0;
    Datablock datablock(relation);

    auto ranges = relation.getRanges(btrblocks::SplitStrategy::SEQUENTIAL, 9999999);

    vector<BytesArray> compressed_chunks;

    compressed_chunks.resize(ranges.size());

    for (u32 chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      auto chunk = relation.getChunk(ranges, chunk_i);
      auto db_meta = datablock.compress(chunk, compressed_chunks[chunk_i]);

      comp_ratio_sum += db_meta.compression_ratio;

      auto decompressed_chunk = datablock.decompress(compressed_chunks[chunk_i]);
    }

    double comp_ratio_avg = comp_ratio_sum / static_cast<double>(ranges.size());

    state.counters["comp_ratio_avg"] = benchmark::Counter(comp_ratio_avg, benchmark::Counter::Flags::kAvgThreads);
  }
}

void RegisterSingleBenchmarks() {
  /* for (auto& dataset : integer_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(5);
  } */
  for (auto& dataset : double_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(30);
  }
  for (auto& dataset : string_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(5);
  }
}
}  // namespace btrblocks