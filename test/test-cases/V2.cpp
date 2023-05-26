// -------------------------------------------------------------------------------------
#include "TestHelper.hpp"
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "storage/Relation.hpp"
#include "compression/Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
DECLARE_uint32(force_string_scheme);
DECLARE_uint32(force_integer_scheme);
DECLARE_uint32(force_double_scheme);
#include "compression/schemes/CSchemePool.hpp"
DECLARE_bool(db2);
// -------------------------------------------------------------------------------------
using namespace db;
// -------------------------------------------------------------------------------------
TEST(V2, Begin) {
   FLAGS_db2 = true;
   db::CSchemePool::refresh();
}
// -------------------------------------------------------------------------------------
TEST(V2, StringCompressedDictionary)
{
   FLAGS_force_string_scheme = CB(StringSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("string/COMPRESSED_DICTIONARY.string"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(StringSchemeType::DICT)});
}
// -------------------------------------------------------------------------------------
TEST(V2, IntegerRLE)
{
   FLAGS_force_integer_scheme = CB(IntegerSchemeType::RLE);
   Relation relation;
   relation.addColumn(TEST_DATASET("integer/RLE.integer"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::RLE)});
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleRLE)
{
   FLAGS_force_double_scheme = CB(DoubleSchemeType::RLE);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/RANDOM.double"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::RLE)});
   FLAGS_force_double_scheme = AUTO_SCHEME;
}
// -------------------------------------------------------------------------------------
TEST(V2, IntegerDyanmicDict)
{
   FLAGS_force_integer_scheme = CB(IntegerSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("integer/DICTIONARY_16.integer"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::DICT)});
   FLAGS_force_integer_scheme = AUTO_SCHEME;
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleDecimal)
{
   FLAGS_force_double_scheme = CB(DoubleSchemeType::DECIMAL);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/DICTIONARY_8.double"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::DECIMAL)});
   FLAGS_force_double_scheme = AUTO_SCHEME;
}
// -------------------------------------------------------------------------------------
TEST(V2, DoubleDyanmicDict)
{
   FLAGS_force_double_scheme = CB(DoubleSchemeType::DICT);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/DICTIONARY_8.double"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::DICT)});
   FLAGS_force_double_scheme = AUTO_SCHEME;
}
// -------------------------------------------------------------------------------------
// TEST(V2, IntegerFrequency)
// {
//    FLAGS_force_integer_scheme = CB(IntegerSchemeType::FREQUENCY);
//    Relation relation;
//    relation.addColumn(TEST_DATASET("integer/FREQUENCY.integer"));
//    db::Datablock datablockV2(relation);
//    TestHelper::CheckRelationCompression(relation, datablockV2, {CB(IntegerSchemeType::FREQUENCY)});
//    FLAGS_force_integer_scheme = AUTO_SCHEME;
// }
// -------------------------------------------------------------------------------------
// scheme is disabled
TEST(V2, DoubleFrequency)
{
   FLAGS_force_double_scheme = CB(DoubleSchemeType::FREQUENCY);
   Relation relation;
   relation.addColumn(TEST_DATASET("double/FREQUENCY.double"));
   db::Datablock datablockV2(relation);
   TestHelper::CheckRelationCompression(relation, datablockV2, {CB(DoubleSchemeType::FREQUENCY)});
   FLAGS_force_double_scheme = AUTO_SCHEME;
}
// -------------------------------------------------------------------------------------
TEST(V2, End)
{
   FLAGS_db2=false;
   db::CSchemePool::refresh();
}
// -------------------------------------------------------------------------------------
