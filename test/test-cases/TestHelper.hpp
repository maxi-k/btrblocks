#pragma once
#include "btrblocks.hpp"
#include "storage/Relation.hpp"
#include "compression/Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
class TestHelper {
public:
   static void CheckRelationCompression(Relation &relation, RelationCompressor &compressor, const vector<u8> expected_compression_schemes = {});
};
// -------------------------------------------------------------------------------------
template<typename T>
struct EnforceScheme {
   explicit EnforceScheme(T scheme_type) { getSchemeRef() = scheme_type; }
   ~EnforceScheme() noexcept(false) { getSchemeRef() = static_cast<T>(autoScheme()); }

   T& getSchemeRef();

};
// -------------------------------------------------------------------------------------
template <>
inline IntegerSchemeType& EnforceScheme<IntegerSchemeType>::getSchemeRef() {
   return BtrBlocksConfig::get().integers.override_scheme;
}
template <>
inline DoubleSchemeType& EnforceScheme<DoubleSchemeType>::getSchemeRef() {
   return BtrBlocksConfig::get().doubles.override_scheme;
}
template <>
inline StringSchemeType& EnforceScheme<StringSchemeType>::getSchemeRef() {
   return BtrBlocksConfig::get().strings.override_scheme;
}
// ------------------------------------------------------------------------------
//
