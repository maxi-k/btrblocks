// -------------------------------------------------------------------------------------
#include "TestHelper.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "storage/Relation.hpp"
#include "compression/Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
#include "scheme/SchemePool.hpp"
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
TEST(V2, Begin) {
   BtrBlocksConfig::get().integers.schemes = defaultIntegerSchemes();
   BtrBlocksConfig::get().doubles.schemes = defaultDoubleSchemes();
   BtrBlocksConfig::get().strings.schemes = defaultStringSchemes();
   SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
TEST(V2, StringCompressedDictionary)
{
   EnforceScheme<StringSchemeType> enforcer(StringSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("string/COMPRESSED_DICTIONARY.string"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(StringSchemeType::DICT)});
}
// -------------------------------------------------------------------------------------
TEST(V2, IntegerRLE)
{
   EnforceScheme<IntegerSchemeType> enforcer(IntegerSchemeType::RLE);
   Relation relation;
   relation.addColumn(TEST_DATASET("integer/RLE.integer"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::RLE)});
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleRLE)
{
   EnforceScheme<DoubleSchemeType> enforcer(DoubleSchemeType::RLE);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/RANDOM.double"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::RLE)});
}
// -------------------------------------------------------------------------------------
TEST(V2, IntegerDyanmicDict)
{
   EnforceScheme<IntegerSchemeType> enforcer(IntegerSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("integer/DICTIONARY_16.integer"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::DICT)});
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleDecimal)
{
   EnforceScheme<DoubleSchemeType> enforcer(DoubleSchemeType::PSEUDODECIMAL);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/DICTIONARY_8.double"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::PSEUDODECIMAL)});
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleDyanmicDict)
{
   EnforceScheme<DoubleSchemeType> enforcer(DoubleSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/DICTIONARY_8.double"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::DICT)});
}
// -------------------------------------------------------------------------------------
// TEST(V2, IntegerFrequency)
// {
//    FLAGS_force_integer_scheme = CB(IntegerSchemeType::FREQUENCY);
//    Relation relation;
//    relation.addColumn(TEST_DATASET("integer/FREQUENCY.integer"));
//    Datablock datablockV2(relation);
//    TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::FREQUENCY)});
//    FLAGS_force_integer_scheme = AUTO_SCHEME;
// }
// -------------------------------------------------------------------------------------
// scheme is disabled
TEST(V2, DoubleFrequency)
{
   EnforceScheme<DoubleSchemeType> enforcer(DoubleSchemeType::FREQUENCY);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/FREQUENCY.double"));
   Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::FREQUENCY)});
}
// -------------------------------------------------------------------------------------
TEST(V2, End)
{
   SchemePool::refresh();
}
// -------------------------------------------------------------------------------------
