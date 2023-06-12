#include "common/Units.hpp"
#include "common/Reinterpret.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
#include <regex>
#include <set>
#include <map>
#include <fstream>
#include <cmath>
#include <iomanip>
// -------------------------------------------------------------------------------------
DEFINE_bool(print_header, false, ".");
DEFINE_string(in, "", ".");
DEFINE_uint32(block_size, 65000, ".");
using namespace std;
using namespace btrblocks;
// -------------------------------------------------------------------------------------
void printDouble(double input)
{
   union {
      double d;
      uint64_t u;
   };

   d = input;
   bool sign = (u >> 63) & 0x1;
   uint64_t exponent = (u >> 52) & 0x7FF;
   uint64_t mantissa = u & 0xFFFFFFFFFFFFF;

   cout << sign << " " << bitset<11>(exponent) << " " << bitset<52>(mantissa) << " "
        << std::setprecision(17) << d << endl;
}
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   srand(time(NULL));
   // -------------------------------------------------------------------------------------
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   string data_file = FLAGS_in, bitmap_file;
   {
      std::regex re("(.*).double");
      std::smatch match;
      if ( std::regex_search(data_file, match, re) && match.size() > 1 ) {
         bitmap_file = match.str(1) + ".bitmap";
      }
   }
   // -------------------------------------------------------------------------------------
   Vector<DOUBLE> column;
   column.readBinary(data_file.c_str());
   Vector<BITMAP> bitmap;
   bitmap.readBinary(bitmap_file.c_str());
   auto column_count = bitmap.size();
   u32 column_set_count = 0;
   assert(bitmap.size() == column.size());
   // -------------------------------------------------------------------------------------
   // Looking for : positive_blocks(where all exponents are equal); range,min,max of exponents
   u32 blocks_count = 0, positive_blocks = 0;
   set<s64> exponents;
   set<u64> mantissas;
   set<double> doubles;
   for ( u64 offset = 0; offset < column_count; offset += FLAGS_block_size ) {
      blocks_count++;
      // -------------------------------------------------------------------------------------
      u64 chunk_tuple_count;
      if ( offset + FLAGS_block_size >= column_count ) {
         chunk_tuple_count = column_count - offset;
      } else {
         chunk_tuple_count = FLAGS_block_size;
      }
      bool block_flag = true;
      bool init = false;
      DOUBLE d;
      s64 block_exponent = 0;
      for ( u32 tuple_i = 0; tuple_i < chunk_tuple_count; tuple_i++ ) {
         if ( !bitmap[offset + tuple_i] ) {
            continue;
         }
         column_set_count++;
         d = column[offset + tuple_i];
         doubles.insert(d);
         s64 current_exponent = (((RU64(d)) >> 52) & 0x7FF) - 1023;
         mantissas.insert(RU64(d) & 0xFFFFFFFFFFFFF);
         exponents.insert(current_exponent);
         if ( !init ) {
            init = true;
            block_exponent = current_exponent;
            continue;
         } else {
            if ( block_flag && current_exponent != block_exponent ) {
               block_flag = false;
            }
         }
      }
      if ( block_flag ) {
         positive_blocks++;
      }
   }

   cout << fixed << setprecision(4);
   cout << '"' << FLAGS_in << '"' << '\t'
     <<  column_count << '\t'
     <<  column_set_count << '\t'
     << 100.0 * CD(positive_blocks) / CD(blocks_count) << '\t'
     <<  *std::max_element(exponents.begin(),exponents.end()) << '\t'
     <<  *std::min_element(exponents.begin(),exponents.end()) << '\t'
     <<  doubles.size() << '\t'
     <<  exponents.size() << '\t'
     <<  mantissas.size() << '\t'
     << endl;
}
// -------------------------------------------------------------------------------------
