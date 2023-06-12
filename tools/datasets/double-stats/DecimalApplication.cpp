#include "common/Units.hpp"
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
DEFINE_uint32(siginifcant_digit_bits_limit, 32, "");
DEFINE_uint32(exponent_limit, 15, "");
DEFINE_bool(eps,false,"");
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
   u32 positive = 0, negative = 0, blocks_count = 0, positive_blocks = 0;
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
      DOUBLE d;
      for ( u32 tuple_i = 0; tuple_i < chunk_tuple_count; tuple_i++ ) {
         if ( !bitmap[offset + tuple_i] ) {
            continue;
         }
         column_set_count++;
         d = column[offset + tuple_i];
         bool flag = false;
         for ( s32 e = 0; e <= FLAGS_exponent_limit; e++ ) {
            double cd = d * std::pow(10, e);
            u64 significant_digits = std::round(cd);
            double rd = significant_digits;
            double if_converted_back = std::pow(10, -e) * rd;
            // -------------------------------------------------------------------------------------
            // string method
            stringstream ss;
            if ( e && significant_digits ) {
               u32 left = (significant_digits / (std::pow(10, e)));
               u32 right = (significant_digits % CU(std::pow(10, e)));
               ss << left << ".";
               ss << setfill('0') << setw(e) << right;
            }
            string str;
            ss >> str;
            if ( str.size())
               if_converted_back = stod(str);
            // -------------------------------------------------------------------------------------
            if ( if_converted_back == d ) {
               flag = true;
               positive++;
               cout << significant_digits << '\t' << e << endl;
               break;
            }
         }
         if ( !flag ) {
            negative++;
            printDouble(d);
            block_flag = false;
         }
      }
      if ( block_flag )
         positive_blocks++;
   }

   cout << fixed << setprecision(4) << 100.0 * CD(positive) / CD(column_set_count) << '\t'
        << positive_blocks
        << endl;
}
// -------------------------------------------------------------------------------------
