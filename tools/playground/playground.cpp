// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "common/PerfEvent.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <cstdlib>     /* srand, rand */
#include <ctime>       /* time */
#include <fsst.h>
#include <gflags/gflags.h>
// -------------------------------------------------------------------------------------
#include "headers/codecfactory.h"
#include "headers/deltautil.h"
// -------------------------------------------------------------------------------------
DEFINE_uint32(n, 10, "");
DEFINE_uint32(s, 100, "");
DEFINE_uint32(e, 26, "");
// -------------------------------------------------------------------------------------
using namespace std;
using namespace btrblocks;
// -------------------------------------------------------------------------------------
int main(int argc, char **argv)
{
   gflags::SetUsageMessage("");
   gflags::ParseCommandLineFlags(&argc, &argv, true);

   // -------------------------------------------------------------------------------------
   auto input_size = 1024;
   auto in_arr = std::unique_ptr<u32[]>(new u32[input_size]);
   using namespace FastPForLib;
   for(int t = 0; t < input_size; t++) {
     in_arr[t] = (1 << 1);
   }
   in_arr[10] = u32(1 << 31) - 1;

   auto fast_pfor = std::shared_ptr<IntegerCODEC>(
     new CompositeCodec<SIMDFastPFor<4>, VariableByte>());
   auto codec = std::shared_ptr<IntegerCODEC>(
                                              new CompositeCodec<FastBinaryPacking<32>, VariableByte>()); //

   size_t compressed_codes_size = input_size * 2; // not really used
   // -------------------------------------------------------------------------------------
   auto dest = std::unique_ptr<u32[]>(new u32[input_size * 2]);
   auto dest_integer = reinterpret_cast<u64>(dest.get());
   dest_integer = (dest_integer + 32) & ~3ul;
   auto dest_4_aligned = reinterpret_cast<u32 *>(dest_integer);
   // -------------------------------------------------------------------------------------
   fast_pfor->encodeArray(in_arr.get(), input_size, dest_4_aligned, compressed_codes_size);
   cout << compressed_codes_size << std::endl;
   // -------------------------------------------------------------------------------------
   codec->encodeArray(in_arr.get(), input_size, dest_4_aligned, compressed_codes_size);
   cout << compressed_codes_size << std::endl;
   return 0;
   // -------------------------------------------------------------------------------------
   for(int t = 0; t < input_size; t++) {
     in_arr[t] -= (1 << 24);
   }
// -------------------------------------------------------------------------------------
   fast_pfor->encodeArray(in_arr.get(), input_size, dest_4_aligned, compressed_codes_size);
   cout << compressed_codes_size << std::endl;
   // -------------------------------------------------------------------------------------
   codec->encodeArray(in_arr.get(), input_size, dest_4_aligned, compressed_codes_size);
   cout << compressed_codes_size << std::endl;
   // -------------------------------------------------------------------------------------

   return 0;
   // -------------------------------------------------------------------------------------

   srand(time(NULL));
   const u32 n = 10, s = FLAGS_s;
   unsigned char *srcBuf[n] = {};
   unsigned char *dstBuf[n] = {};
   unsigned long srcLen[n] = {};
   unsigned long dstLen[n] = {};
//   unsigned long dstLen[2] = { 0, 0 };

   auto in_array = std::unique_ptr<std::unique_ptr<char[]>[]>();
   in_array = std::unique_ptr<std::unique_ptr<char[]>[]>(new std::unique_ptr<char[]>[n]);

   for ( auto i = 0; i < n; i++ ) {
      srcBuf[i] = (u8 *) malloc(s);
      for ( u32 b_i = 0; b_i < s; b_i++ ) {
         srcBuf[i][b_i] = 65 + rand() % FLAGS_e;
      }
      srcLen[i] = s;
   }
   unsigned char serialized_encoder_buf[FSST_MAXHEADER];
   fsst_encoder_t *encoder = fsst_create(n, srcLen, srcBuf, 0);
   unsigned long hdr = fsst_export(encoder, serialized_encoder_buf);

   auto output_buffer_size = 10 * 1024 * 1024;
   auto output_buffer = (u8 *) malloc(output_buffer_size);

   auto n_compressed_strings = fsst_compress(encoder, n, srcLen, srcBuf, output_buffer_size, output_buffer,
                                            dstLen, dstBuf);
   assert(n_compressed_strings == n);
   //fsst_destroy(encoder);

   // -------------------------------------------------------------------------------------
   // decompress time
   unsigned char *decompressedBuf[n] = {};
   fsst_decoder_t decoder;
   PerfEvent e;
   {
      PerfEventBlock block(e,1);
      decoder = fsst_decoder(encoder);
   }
   cout << sizeof(fsst_decoder_t) << endl;
   cout << FSST_MAXHEADER << endl;
   {
      PerfEventBlock block(e,1);
      fsst_import(&decoder, serialized_encoder_buf);
   }

   for ( auto i = 0; i < n; i++ ) {
      decompressedBuf[i] = (u8 *) malloc(s);
      assert(fsst_decompress(&decoder, dstLen[i], dstBuf[i], s, decompressedBuf[i]) == s);
      assert(memcmp(decompressedBuf[i], srcBuf[i], s) == 0);
   }

   return 0;
}
// -------------------------------------------------------------------------------------
