// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include <fsst.h>
// -------------------------------------------------------------------------------------
#include <iostream>
#include <cassert>
// -------------------------------------------------------------------------------------
using namespace btrblocks;
using namespace std;
// -------------------------------------------------------------------------------------
/*
 * corner case: setting n to 0, makes fsst hangs with 100% core usage.
 */
int main(int argc, char **argv)
{
   unsigned long fsst_n = 1993;

   auto input_string_buffers = std::unique_ptr<u8 *[]>(new u8 *[fsst_n]);
   auto input_string_lengths = std::unique_ptr<u64[]>(new u64[fsst_n]);
   auto output_string_buffers = std::unique_ptr<u8 *[]>(new u8 *[fsst_n]);
   auto output_string_lengths = std::unique_ptr<u64[]>(new u64[fsst_n]);


   Vector<str> fsst_strings;
   fsst_strings.readBinary("fsst_strings");
   Vector<u64> fsst_lengths;
   fsst_lengths.readBinary("fsst_lengths");

   for(u32 s_i =0; s_i < fsst_n; s_i++) {
      cout << fsst_strings[s_i] << endl;
      input_string_buffers[s_i] = (u8*) malloc(fsst_lengths[s_i]);
      memcpy(input_string_buffers[s_i],fsst_strings[s_i].data(), fsst_lengths[s_i]);
      input_string_lengths[s_i] = fsst_lengths[s_i];
   }
   fsst_encoder_t *encoder = fsst_create(fsst_n, input_string_lengths.get(), input_string_buffers.get(), 0);
   return 0;
}
