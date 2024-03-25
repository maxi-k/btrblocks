#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <memory>
#include <type_traits>
// -------------------------------------------------------------------------------------
// the linker breaks when including the fastpfor library multiple times.
// thus, provide a wrapper for the functions we use from it.
// -------------------------------------------------------------------------------------
enum class FastPForCodec { FPF, FBP };
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
struct LemiereImpl {
  using u32 = btrblocks::u32;
  using data_t = btrblocks::u32;
  // -------------------------------------------------------------------------------------
  LemiereImpl();
  ~LemiereImpl();
  // -------------------------------------------------------------------------------------
  u32 compress(const data_t* src, u32 count, data_t* dest, size_t& outsize);
  const data_t* decompress(const data_t* src, u32 count, data_t* dest, size_t& outsize);
  // -------------------------------------------------------------------------------------
  static void applyDelta(data_t* src, size_t count);
  static void revertDelta(data_t* src, size_t count);
  // -------------------------------------------------------------------------------------
 private:
  struct impl;
  std::unique_ptr<impl> pImpl;
};
// -------------------------------------------------------------------------------------
using FPFor = LemiereImpl<FastPForCodec::FPF>;
using FBPImpl = LemiereImpl<FastPForCodec::FBP>;
// -------------------------------------------------------------------------------------