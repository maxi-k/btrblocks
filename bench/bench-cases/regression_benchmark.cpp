#include "benchmark/benchmark.h"
#include "btrblocks.hpp"
#include "scheme/SchemePool.hpp"
#include "storage/MMapVector.hpp"
#include "storage/Relation.hpp"
// ---------------------------------------------------------------------------------------------------

using namespace btrblocks;

namespace btrbench {

static const std::vector<std::string> string_datasets{
    "1_F1.string",
    "2_F2.string",
    "6_F6.string",
    "7_F7.string"
};

static const std::vector<std::string> integer_datasets{
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

static const std::vector<std::string> double_datasets{
    "4_F4.double",
    "11_Venta_hoy.double",
    "5_Dev_proxima.double"
};

static void SetupSchemes(const benchmark::State& state) {
  BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
  BtrBlocksConfig::get().doubles.schemes = defaultDoubleSchemes();
  BtrBlocksConfig::get().strings.schemes = defaultStringSchemes();

  SchemePool::refresh();
}

static void BtrBlocksBenchmark(benchmark::State& state, const std::string& dataset) {
  SetupSchemes(state);
  Relation relation;

  relation.addColumn(BENCHMARK_DATASET() + dataset);

  for (auto _ : state) {
    Datablock datablock(relation);

    auto ranges = relation.getRanges(btrblocks::SplitStrategy::SEQUENTIAL, 9999999);
    double comp_ratio_avg = 0.0;
    double size = static_cast<double>(ranges.size());

    vector<BytesArray> compressed_chunks;

    compressed_chunks.resize(ranges.size());

    for (u32 chunk_i = 0; chunk_i < ranges.size(); chunk_i++) {
      auto chunk = relation.getChunk(ranges, chunk_i);
      auto db_meta = datablock.compress(chunk, compressed_chunks[chunk_i]);

      comp_ratio_avg += db_meta.compression_ratio / size;

      auto decompressed_chunk = datablock.decompress(compressed_chunks[chunk_i]);
    }

    state.counters["comp_ratio_avg"] = benchmark::Counter(comp_ratio_avg, benchmark::Counter::Flags::kAvgThreads);
  }
}

void RegisterSingleBenchmarks() {
  for (auto& dataset : integer_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(10);
  }
  for (auto& dataset : double_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(30);
  }
  for (auto& dataset : string_datasets) {
    benchmark::RegisterBenchmark(dataset.c_str(), BtrBlocksBenchmark, dataset)
        ->UseRealTime()
        ->MinTime(30);
  }
}
}  // namespace btrblocks