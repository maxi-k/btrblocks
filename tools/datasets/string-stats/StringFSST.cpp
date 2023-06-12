// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <cstdlib>     /* srand, rand */
#include <cstdlib>
#include <ctime>       /* time */
#include <set>
#include <string>
#include <regex>
#include <fsst.h>
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
DEFINE_uint32(n, 10, "");
DEFINE_uint32(s, 100, "");
DEFINE_uint32(e, 26, "");
DEFINE_string(in, "", "");

using namespace std;
using namespace btrblocks;

int main(int argc, char **argv)
{
   srand(time(nullptr));
   gflags::SetUsageMessage("");
   gflags::ParseCommandLineFlags(&argc, &argv, true);
   // -------------------------------------------------------------------------------------
   set<str> unique_strings;
   Vector<str> input_strings;

   string bitmap_file;
   {
      std::regex re("(.*).string");
      std::smatch match;
      if ( std::regex_search(FLAGS_in, match, re) && match.size() > 1 ) {
         bitmap_file = match.str(1) + ".bitmap";
      }
   }
   Vector<BITMAP> bitmap;
   bitmap.readBinary(bitmap_file.c_str());
   input_strings.readBinary(FLAGS_in.c_str());
   // -------------------------------------------------------------------------------------
   // extract unique values
   u32 before_size = 0;
   u32 start_index = rand() % input_strings.size();
   u32 tuple_count = std::min(static_cast<u64>(input_strings.size() - start_index), 65000ul);
   if ( start_index == input_strings.size()) {
      start_index = 0;
      tuple_count = std::min(input_strings.size(), 65000ul);
   }
   for ( u32 tuple_i = start_index; tuple_i < start_index + tuple_count; tuple_i++ ) {
      if ( bitmap[tuple_i] == 0 ) {
         continue;
      }
      auto current_str = input_strings[tuple_i];
      if ( unique_strings.find(current_str) == unique_strings.end()) {
         before_size += current_str.length();
         unique_strings.insert(current_str);
      }
   }
   // -------------------------------------------------------------------------------------
   unsigned long n = unique_strings.size();
   if ( n == 0 ) {
      cout << "--" << "\t" << "--" << "\t" << "--" << "\t" << "--" << "\t" << "--" << "\t" << FLAGS_in << endl;
      return 0;
   }
   u8 **srcBuf = (u8 **) calloc(n, sizeof(u8 *));
   u8 **dstBuf = (u8 **) calloc(n, sizeof(u8 *));
   u64 *srcLen = (u64 *) calloc(n, sizeof(u64));
   u64 *dstLen = (u64 *) calloc(n, sizeof(u64));
   // -------------------------------------------------------------------------------------
   auto unique_strings_buffers = std::unique_ptr<u8[]>(new u8[before_size]);
   u8 *write_ptr = unique_strings_buffers.get();
   u32 i = 0;
   for ( const auto &unique_str : unique_strings ) {
      srcBuf[i] = write_ptr;
      srcLen[i] = unique_str.size();
      memcpy(write_ptr, unique_str.data(), unique_str.size());
      write_ptr += unique_str.size();
      i++;
   }
   // -------------------------------------------------------------------------------------
   unsigned char serialized_encoder_buf[FSST_MAXHEADER];
   fsst_encoder_t *encoder = fsst_create(n, srcLen, srcBuf, 0);
   unsigned long hdr = fsst_export(encoder, serialized_encoder_buf);

   unsigned long output_buffer_size = 7 + 4 * before_size;//1024 * 1024 * 1024
   auto output_buffer = (u8 *) calloc(output_buffer_size, sizeof(u8));

   auto n_compressed_strings = fsst_compress(encoder, n, srcLen, srcBuf, output_buffer_size, output_buffer,
                                            dstLen, dstBuf);
   assert(n_compressed_strings == n);
   u32 after_size = hdr;
   for ( u32 tuple_i = 0; tuple_i < n; tuple_i++ ) {
      after_size += dstLen[tuple_i];
   }
//   cout << "n" << "\t" << "before_size" << "\t" << "after_size" << "\t" << "fsst_compression_ratio" << "\t" << "hdr" << "\t" << "path" << "\t" << endl;
   cout << n << "\t" << before_size << "\t" << after_size << "\t" << static_cast<double>(before_size) / static_cast<double>(after_size) << "\t" << hdr << "\t" << FLAGS_in << endl;
   fsst_destroy(encoder);
   return 0;
}
/*
 * Notes:
 * 1- one you pass an array of input pointers, they should not be equal, otherwise srcLen will be equal to destLen
 *
 */
