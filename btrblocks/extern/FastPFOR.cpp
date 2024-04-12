// -------------------------------------------------------------------------------------
#include "FastPFOR.hpp"
// -------------------------------------------------------------------------------------
#include "common/Exceptions.hpp"
#include "common/SIMD.hpp"
// -------------------------------------------------------------------------------------
// fastpfor
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuninitialized"
#include <headers/blockpacking.h>
#include <headers/compositecodec.h>
#include <headers/deltautil.h>
#include <headers/simdfastpfor.h>
#include <headers/variablebyte.h>
#include <headers/fastpfor.h>
#pragma GCC diagnostic pop
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FPF, uint32_t>::impl {
  // using codec_t = BTR_IFELSESIMD(FastPForLib::SIMDFastPFor<8>,
  // FastPForLib::FastPFor<8>);
  using codec_t = FastPForLib::SIMDFastPFor<8>;
  FastPForLib::CompositeCodec<codec_t, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FBP, uint32_t>::impl {
  // TODO Adnan did not use SIMDBinaryPacking in the original? ask him why
  FastPForLib::CompositeCodec<FastPForLib::FastBinaryPacking<32>, FastPForLib::VariableByte> codec;
};
// -------------------------------------------------------------------------------------
template <>
struct LemiereImpl<FastPForCodec::FPF, uint64_t>::impl {
  // TODO Adnan did not use SIMDBinaryPacking in the original? ask him why
  FastPForLib::FastPForImpl<4, uint64_t> codec;
};
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec, class T>
LemiereImpl<Codec, T>::LemiereImpl() : pImpl(new LemiereImpl<Codec, T>::impl) {}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec, class T>
LemiereImpl<Codec, T>::~LemiereImpl() = default;
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec, class T>
u32 LemiereImpl<Codec, T>::compress(const T* src, u32 count, u32* dest, SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  codec.encodeArray(src, count, dest, outsize);
  return outsize;
}
// -------------------------------------------------------------------------------------
template <FastPForCodec Codec, class T>
const u32* LemiereImpl<Codec, T>::decompress(const u32* src,
                                                                          u32 count,
                                                                          data_t* dest,
                                                                          SIZE& outsize) {
  auto& codec = this->pImpl->codec;
  return codec.decodeArray(src, count, dest, outsize);
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FPF, uint32_t>::applyDelta(uint32_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::deltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FPF, uint32_t>::revertDelta(uint32_t * src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::inverseDeltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FBP, uint32_t>::applyDelta(uint32_t* src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::deltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FBP, uint32_t>::revertDelta(uint32_t * src, size_t count) {
  using namespace FastPForLib;
  FastPForLib::Delta::inverseDeltaSIMD(src, count);
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FPF, uint64_t>::applyDelta(uint64_t* src, size_t count) {
  throw std::logic_error("not implemented");
}
// -------------------------------------------------------------------------------------
template <>
void LemiereImpl<FastPForCodec::FPF, uint64_t>::revertDelta(uint64_t * src, size_t count) {
  throw std::logic_error("not implemented");
}
// -------------------------------------------------------------------------------------
template struct LemiereImpl<FastPForCodec::FPF, uint32_t>;
template struct LemiereImpl<FastPForCodec::FBP, uint32_t>;
template struct LemiereImpl<FastPForCodec::FPF, uint64_t>;
// -------------------------------------------------------------------------------------