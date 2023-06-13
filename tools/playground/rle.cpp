// -------------------------------------------------------------------------------------
#include "headers/codecfactory.h"
#include "headers/deltautil.h"
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
using namespace btrblocks::mmapvector;
using namespace btrblocks::units;
// -------------------------------------------------------------------------------------
int main() {
   // -------------------------------------------------------------------------------------
   // Randomness generators
   std::srand(std::time(nullptr));
   // -------------------------------------------------------------------------------------
   using namespace FastPForLib;
   IntegerCODEC &codec = *CODECFactory::getFromName("simdfastpfor256");
   size_t N = 1000 * 1000;
   std::vector<uint32_t> rle_input(N);
   for ( uint32_t i = 0; i < N; i++ ) {
      rle_input[i] = rand() % 100000;
      if ( rand() % 10 > 2 ) {
         size_t repeat = rand() % 7;
         repeat = std::min(repeat, N - i - 1);
         for ( size_t r_i = 1; r_i <= repeat; r_i++ ) {
            rle_input[i + r_i] = rle_input[i];
         }
         i += repeat;
      }
   }
   writeBinary("rle_input.integer", rle_input);
   // 30% chance -> repeat from 0 till 7

   std::vector<u32> rle_output;

   std::vector<u32> rle_values;
   std::vector<u32> rle_count;

   {
      // RLE encoding
      u32 last_item = rle_input[0];
      u32 count = 1;
      for ( uint32_t i = 1; i < N; i++ ) {
         if ( rle_input[i] == last_item ) {
            count++;
         } else {
            rle_output.push_back(count);
            rle_count.push_back(count);
            rle_output.push_back(last_item);
            rle_values.push_back(last_item);
            last_item = rle_input[i];
            count = 1;
         }
      }
   }

   cout << " b = " << rle_input.size() << " a = " << rle_output.size() << " r  = " << 1.0 * rle_input.size() / rle_output.size() << endl;
   cout <<"plain rle bytes = " << rle_output.size() * 4 << endl;
   {
      std::vector<uint32_t> compressed_output(N + 1024);
      size_t compressedsize = compressed_output.size();
      codec.encodeArray(rle_input.data(), rle_input.size(), compressed_output.data(),
                        compressedsize);
      //
      // if desired, shrink back the array:
      compressed_output.resize(compressedsize);
      compressed_output.shrink_to_fit();
      // display compression rate:
      std::cout << std::setprecision(3);
      std::cout << "You are using "
                << 32.0 * static_cast<double>(compressed_output.size()) /
                   static_cast<double>(rle_input.size())
                << " bits per integer. " << std::endl;

      cout <<"direct bitpacking gives " << compressed_output.size() * 4 << endl;
   }

   {
      std::vector<uint32_t> compressed_output(N + 1024);
      size_t compressedsize = compressed_output.size();
      codec.encodeArray(rle_output.data(), rle_output.size(), compressed_output.data(),
                        compressedsize);
      //
      // if desired, shrink back the array:
      compressed_output.resize(compressedsize);
      compressed_output.shrink_to_fit();

      cout <<"bitpacking rle plain gives " << compressed_output.size() * 4 << endl;

   }
   // compress values and counts separately
   {
      u32 total_size = 0;
      {

         //Delta::deltaSIMD(rle_values.data(), rle_values.size());

         std::vector<uint32_t> compressed_output(N + 1024);
         size_t compressedsize = compressed_output.size();
         codec.encodeArray(rle_values.data(), rle_values.size(), compressed_output.data(),
                           compressedsize);

         total_size += compressedsize * 4;
      }
      {
         std::vector<uint32_t> compressed_output(N + 1024);
         size_t compressedsize = compressed_output.size();
         codec.encodeArray(rle_count.data(), rle_count.size(), compressed_output.data(),
                           compressedsize);

         // if desired, shrink back the array:
         compressed_output.resize(compressedsize);
         compressed_output.shrink_to_fit();
         std::cout << std::setprecision(3);
         std::cout << "You are using "
                   << 32.0 * static_cast<double>(compressed_output.size()) /
                      static_cast<double>(rle_count.size())
                   << " bits per integer. " << std::endl;
         total_size += compressedsize *4 ;
      }
      cout << " separate choice total_size " << total_size << endl;
   }
}
// -------------------------------------------------------------------------------------
