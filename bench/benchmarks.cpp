// ---------------------------------------------------------------------------
// BtrBlocks
// ---------------------------------------------------------------------------
#include "benchmark/benchmark.h"
#include "scheme/SchemePool.hpp"
#include "bench-cases/regression_benchmark.cpp"
// ---------------------------------------------------------------------------
using namespace btrblocks;
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
#ifdef BTR_USE_SIMD
  std::cout << "\033[0;35mSIMD ENABLED\033[0m" << std::endl;
#else
  std::cout << "\033[0;31mSIMD DISABLED\033[0m" << std::endl;
#endif

  // TODO: This is probably super dumb and we should do that in the corresponding git action
  const char* args[argc + 2];
  for (int i = 0; i < argc; i++) {
    args[i] = argv[i];
  }
  std::string out_format = "--benchmark_out_format=json";
  std::string out_file = "--benchmark_out=./bench_results/test.json";

  args[argc] = out_format.c_str();
  args[argc + 1] = out_file.c_str();

  int new_argc = argc + 2;

  btrbench::RegisterSingleBenchmarks();
  benchmark::Initialize(&new_argc, const_cast<char**>(args));
  benchmark::RunSpecifiedBenchmarks();
}
// ---------------------------------------------------------------------------
