#include "btrblocks.hpp"
#include <thread>
#include <iostream>
#include <cassert>

#include <tbb/parallel_for.h>
#include <tbb/blocked_range.h>

using namespace btrblocks;

using range = tbb::blocked_range<uint32_t>;
int main(int argc, char *argv[]) {
  auto& cfg = BtrBlocksConfig::get();
  cfg.block_size = 32;

  tbb::parallel_for(range(0, 10), [&](const range& r) {

    auto& cfg = BtrBlocksConfig::local();
    std::cout << cfg.block_size << std::endl;
    assert(cfg.block_size == 32);
  });

  cfg.block_size = 10;

  tbb::parallel_for(range(0, 10), [&](const range& r) {
    auto& cfg = BtrBlocksConfig::local();
    std::cout << cfg.block_size << std::endl;
    // depending on whether this is a new thread or not,
    // could get either config value at this point.
    assert(cfg.block_size == 32 || cfg.block_size == 10);
    cfg = BtrBlocksConfig::get();
  });

  tbb::parallel_for(range(0, 10), [&](const range& r) {
    auto& cfg = BtrBlocksConfig::local();
    std::cout << cfg.block_size << std::endl;
    assert(cfg.block_size == 10);
    cfg = BtrBlocksConfig::get();
  });

  return 0;
}
