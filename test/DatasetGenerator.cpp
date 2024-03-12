// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <set>
#include <cstdlib>
#include <ctime>
#include <random>
// -------------------------------------------------------------------------------------
using namespace std;
using namespace btrblocks;
// -------------------------------------------------------------------------------------
DEFINE_uint64(tuple_count, 6500 * 10, "");
DEFINE_uint32(dict_distinct_val_threshold, 10, "");
//--------------------------------------------------------------------------------------
void GenerateRandomString(char *dest, SIZE len, u8 entropy = 26 * 2 + 10);
// -------------------------------------------------------------------------------------
const string out_dir_name = "test-dataset";
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   gflags::SetUsageMessage("CSV Dataset parser");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   if ( mkdir(TEST_DATASET(), S_IRWXU | S_IRWXG) && errno != EEXIST ) {
      cerr << "creating output directory failed, status = " << errno << endl;
   }
   // -------------------------------------------------------------------------------------
   // TODO:  at the moment, null values are not supported --> bitmaps are ones
   vector<BITMAP> bitmap = vector<BITMAP>(FLAGS_tuple_count, 1);
   // -------------------------------------------------------------------------------------
   // Randomness generators
   std::srand(std::time(nullptr));
   // -------------------------------------------------------------------------------------
   // INTEGER
   {
      vector<INTEGER> integers;
      if ( mkdir(TEST_DATASET("integer"), S_IRWXU | S_IRWXG) && errno != EEXIST ) {
         cerr << "creating output directory failed, status = " << errno << endl;
      }
      // One Value
      {
         integers = vector<INTEGER>(FLAGS_tuple_count, 100);
         writeBinary(TEST_DATASET("integer/ONE_VALUE.integer"), integers);
         writeBinary(TEST_DATASET("integer/ONE_VALUE.bitmap"), bitmap);
         integers.clear();
      }
      // Truncate 8
      {
         integers.push_back(std::numeric_limits<SMALLINT>::max());
         for ( u64 i = 1; i < FLAGS_tuple_count; i++ ) {
            integers.push_back(std::numeric_limits<SMALLINT>::max() + (std::rand() % std::numeric_limits<TINYINT>::max()));
         }
         writeBinary(TEST_DATASET("integer/TRUNCATE_8.integer"), integers);
         writeBinary(TEST_DATASET("integer/TRUNCATE_8.bitmap"), bitmap);
         integers.clear();
      }
      // Truncate 16
      {
         constexpr auto min = std::numeric_limits<s32>::max() / 2;
         constexpr auto range = std::numeric_limits<s16>::max();
         integers.push_back(min);
         for ( u64 i = 1; i < FLAGS_tuple_count; i++ ) {
            integers.push_back(min + (std::rand() % range));
         }
         writeBinary(TEST_DATASET("integer/TRUNCATE_16.integer"), integers);
         writeBinary(TEST_DATASET("integer/TRUNCATE_16.bitmap"), bitmap);
         integers.clear();
      }
      // Dictionary 8
      {
         vector<INTEGER> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max();
         {
            // prepare the distinct values
            set<INTEGER> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               distinct_values_set.insert(std::rand() % std::numeric_limits<INTEGER>::max());
            }
            distinct_values = vector<INTEGER>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            integers.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("integer/DICTIONARY_8.integer"), integers);
         writeBinary(TEST_DATASET("integer/DICTIONARY_8.bitmap"), bitmap);
         integers.clear();
      }
      // Dictionary 16
      {
         vector<INTEGER> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max() * 4;
         {
            // prepare the distinct values
            set<INTEGER> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               distinct_values_set.insert(std::rand() % std::numeric_limits<INTEGER>::max());
            }
            distinct_values = vector<INTEGER>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            integers.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("integer/DICTIONARY_16.integer"), integers);
         writeBinary(TEST_DATASET("integer/DICTIONARY_16.bitmap"), bitmap);
         integers.clear();
      }
   }
   // -------------------------------------------------------------------------------------
   {
      // DOUBLE
      vector<DOUBLE> doubles;
      std::random_device rd;
      std::mt19937 gen(rd());
      std::uniform_real_distribution<> dis(-999999, 999999); // pick better range
      if ( mkdir(TEST_DATASET("double"), S_IRWXU | S_IRWXG) && errno != EEXIST ) {
         cerr << "creating output directory failed, status = " << errno << endl;
      }
      // One Value
      {
         doubles = vector<DOUBLE>(FLAGS_tuple_count, 100.0);
         writeBinary(TEST_DATASET("double/ONE_VALUE.double"), doubles);
         writeBinary(TEST_DATASET("double/ONE_VALUE.bitmap"), bitmap);
         doubles.clear();
      }
      // Dictionary 8
      {
         vector<DOUBLE> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max();
         {
            // prepare the distinct values
            set<DOUBLE> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               distinct_values_set.insert(dis(gen));
            }
            distinct_values = vector<DOUBLE>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            doubles.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("double/DICTIONARY_8.double"), doubles);
         writeBinary(TEST_DATASET("double/DICTIONARY_8.bitmap"), bitmap);
         doubles.clear();
      }
      // Dictionary 16
      {
         vector<DOUBLE> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max() * 4;
         {
            // prepare the distinct values
            set<DOUBLE> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               distinct_values_set.insert(dis(gen));
            }
            distinct_values = vector<DOUBLE>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            doubles.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("double/DICTIONARY_16.double"), doubles);
         writeBinary(TEST_DATASET("double/DICTIONARY_16.bitmap"), bitmap);
         doubles.clear();
      }
      // Random
      {
         doubles = vector<DOUBLE>();
         std::random_device rd;
         std::mt19937 gen(rd());
         std::uniform_real_distribution<> dis(-999999, 999999); // pick better range
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            doubles.push_back(dis(gen));
         }
         writeBinary(TEST_DATASET("double/RANDOM.double"), doubles);
         writeBinary(TEST_DATASET("double/RANDOM.bitmap"), bitmap);
         doubles.clear();
      }
      // TODO:
   }
   // -------------------------------------------------------------------------------------
   // STRING
   {
      vector<STRING> strings;
      if ( mkdir(TEST_DATASET("string"), S_IRWXU | S_IRWXG) && errno != EEXIST ) {
         cerr << "creating output directory failed, status = " << errno << endl;
      }
      // One Value
      {
         strings = vector<STRING>(FLAGS_tuple_count, "Hello Compression !");
         writeBinary(TEST_DATASET("string/ONE_VALUE.string"), strings);
         writeBinary(TEST_DATASET("string/ONE_VALUE.bitmap"), bitmap);
         strings.clear();
      }
      // Dictionary 8
      {
         vector<STRING> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max();
         {
            // prepare the distinct values
            set<STRING> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               string str(10, 'a');
               GenerateRandomString(str.data(), 10);
               distinct_values_set.insert(str);
            }
            distinct_values = vector<STRING>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            strings.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("string/DICTIONARY_8.string"), strings);
         writeBinary(TEST_DATASET("string/DICTIONARY_8.bitmap"), bitmap);
         strings.clear();
      }
      // WARNING: from now on, bitmap will be used
      // Dictionary 16
      {
         vector<STRING> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u8>::max() * 4;
         {
            // prepare the distinct values
            set<STRING> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               string str(10, 'a');
               GenerateRandomString(str.data(), 10);
               distinct_values_set.insert(str);
            }
            distinct_values = vector<STRING>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            if ( rand() % 10 ) {
               strings.push_back(distinct_values[distinct_values_index]);
               distinct_values_index++;
               distinct_values_index = distinct_values_index % distinct_values_count;
            } else {
               strings.push_back("");
               bitmap[i] = 0;
               continue;
            }
         }
         writeBinary(TEST_DATASET("string/DICTIONARY_16.string"), strings);
         writeBinary(TEST_DATASET("string/DICTIONARY_16.bitmap"), bitmap);
         strings.clear();
         bitmap = vector<BITMAP>(FLAGS_tuple_count, 1);
      }

      // -------------------------------------------------------------------------------------
      // V2 Schemes starts here
      // -------------------------------------------------------------------------------------
      // Integer
      // -------------------------------------------------------------------------------------
      // RLE
      {
         vector<INTEGER> integers;
         {
            integers = vector<INTEGER>(FLAGS_tuple_count);
            for ( uint32_t i = 0; i < FLAGS_tuple_count; i++ ) {
               integers[i] = (rand() % 100000);
               if ( rand() % 10 > 2 ) {
                  size_t repeat = 20;
                  repeat = std::min(static_cast<u64>(repeat), FLAGS_tuple_count - i - 1);
                  for ( size_t r_i = 1; r_i <= repeat; r_i++ ) {
                     integers[i + r_i] = integers[i];
                  }
                  i += repeat;
               }
            }
            writeBinary(TEST_DATASET("integer/RLE.integer"), integers);
            writeBinary(TEST_DATASET("integer/RLE.bitmap"), bitmap);
            integers.clear();
         }
      }
      {
         vector<INTEGER> integers;
         {
            integers = vector<INTEGER>(FLAGS_tuple_count);
            INTEGER top_value = rand() % std::numeric_limits<INTEGER>::max();
            for ( uint32_t i = 0; i < FLAGS_tuple_count; i++ ) {
               if ( rand() % 100 > 98 ) {
                  integers[i] = rand();
               } else {
                  integers[i] = top_value;
               }
            }
            writeBinary(TEST_DATASET("integer/FREQUENCY.integer"), integers);
            writeBinary(TEST_DATASET("integer/FREQUENCY.bitmap"), bitmap);
            integers.clear();
         }
      }
      // -------------------------------------------------------------------------------------
      // Double
      {
         vector<DOUBLE> doubles;
         {
            doubles = vector<DOUBLE>(FLAGS_tuple_count);
            DOUBLE top_value = rand() % std::numeric_limits<INTEGER>::max();
            for ( uint32_t i = 0; i < FLAGS_tuple_count; i++ ) {
               if ( rand() % 100 > 98 ) {
                  doubles[i] = rand();
               } else {
                  doubles[i] = top_value;
               }
            }
            writeBinary(TEST_DATASET("double/FREQUENCY.double"), doubles);
            writeBinary(TEST_DATASET("double/FREQUENCY.bitmap"), bitmap);
            doubles.clear();
         }
      }
      // String
      // -------------------------------------------------------------------------------------
      const u32 TZT_MIN_INPUT = 200 * 1024;
      // Dictionary 16
      {
         vector<STRING> distinct_values;
         const u32 distinct_values_count = std::numeric_limits<u16>::max() / 2 - NULL_CODE_MARGIN;
         const u32 string_length = TZT_MIN_INPUT / distinct_values_count;
         {
            // prepare the distinct values
            set<STRING> distinct_values_set;
            while ( distinct_values_set.size() < distinct_values_count ) {
               string str(string_length, 'a');
               GenerateRandomString(str.data(), string_length, 15);
               distinct_values_set.insert(str);
            }
            distinct_values = vector<STRING>(distinct_values_set.begin(), distinct_values_set.end());
         }
         u32 distinct_values_index = 0;
         for ( u64 i = 0; i < FLAGS_tuple_count; i++ ) {
            strings.push_back(distinct_values[distinct_values_index]);
            distinct_values_index++;
            distinct_values_index = distinct_values_index % distinct_values_count;
         }
         writeBinary(TEST_DATASET("string/COMPRESSED_DICTIONARY.string"), strings);
         writeBinary(TEST_DATASET("string/COMPRESSED_DICTIONARY.bitmap"), bitmap);
         strings.clear();
      }
   }
   return 0;
}
// -------------------------------------------------------------------------------------
void GenerateRandomString(char *dest, SIZE len, u8 entropy)
{
   const static char alphanum[] = "0123456789"
                                  "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                  "abcdefghijklmnopqrstuvwxyz";

   for ( unsigned j = 0; j < len; ++j ) {
      dest[j] = alphanum[std::rand() % (entropy - 1)];
   }
}
// -------------------------------------------------------------------------------------
