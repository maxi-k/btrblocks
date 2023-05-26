// -------------------------------------------------------------------------------------
#include "FastPFOR.hpp"
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/SIMD.hpp"
// -------------------------------------------------------------------------------------
// fastpfor
#include <headers/blockpacking.h>
#include <headers/codecfactory.h>
#include <headers/deltautil.h>
#include <headers/fastpfor.h>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FPF>::impl {
  // using codec_t = BTR_IFELSESIMD(FastPForLib::SIMDFastPFor<8>,
  // FastPForLib::FastPFor<8>);
  using codec_t = FastPForLib::SIMDFastPFor<8>;
  FastPForLib::CompositeCodec<codec_t, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FBP>::impl {
  // TODO Adnan did not use SIMDBinaryPacking in the original? ask him why
  FastPForLib::CompositeCodec<FastPForLib::FastBinaryPacking<32>, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
LemiereImpl<Codec>::LemiereImpl() : pImpl(new LemiereImpl<Codec>::impl) {}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
LemiereImpl<Codec>::~LemiereImpl() = default;
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
u32 LemiereImpl<Codec>::compress(const data_t* src, u32 count, data_t* dest, SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  codec.encodeArray(src, count, dest, outsize);
  return outsize;
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
const typename LemiereImpl<Codec>::data_t* LemiereImpl<Codec>::decompress(const data_t* src,
                                                                          u32 count,
                                                                          data_t* dest,
                                                                          SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  return codec.decodeArray(src, count, dest, outsize);
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
void LemiereImpl<Codec>::applyDelta(data_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::deltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec>
void LemiereImpl<Codec>::revertDelta(data_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::inverseDeltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template struct LemiereImpl<FastPForCodec::FPF>;
template struct LemiereImpl<FastPForCodec::FBP>;
// -------------------------------------------------------------------------------------
