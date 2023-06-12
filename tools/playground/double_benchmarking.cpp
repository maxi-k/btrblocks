#include "common/Units.hpp"
#include "common/PerfEvent.hpp"
#include "storage/MMapVector.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <sstream>
#include <iomanip> // setprecision
#include <cstring>
#include <cmath>
#include <bitset>
// -------------------------------------------------------------------------------------
#include <double-conversion/strtod.h>
#include <double-conversion/utils.h>
// -------------------------------------------------------------------------------------
using namespace std;
using namespace btrblocks::units;
using namespace double_conversion;
// -------------------------------------------------------------------------------------
void printDouble(double input) {
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
void printFloat(float input) {
   union {
      float d;
      uint32_t u;
   };

   d = input;
   bool sign = (u >> 31) & 0x1;
   uint64_t exponent = (u >> 23) & 0xFF;
   uint64_t mantissa = u & 0x7FFFFF;

   cout << sign << " " << bitset<8>(exponent) << " " << bitset<23>(mantissa) << " " << std::setprecision(7) << d << endl;
}
// -------------------------------------------------------------------------------------
// double_conversion vector, not btrblocks Vector
static Vector<const char> StringToVector(const char* str) {
   return Vector<const char>(str, strlen(str));
}
// -------------------------------------------------------------------------------------
int main(int, char **)
{
   auto my_vec = StringToVector("123");
   PerfEvent e;
   {
      PerfEventBlock b(e, 65000);
      for(u32 i=0;i< 65000; i++)
         double_conversion::Strtod(my_vec, -1);
   }
   {
      PerfEventBlock b(e, 65000);
      for(u32 i=0;i< 65000; i++){
         double d = 123456789;
         d/=10;
         d/=10;
         d/=10;
         d/=10;

      }

   }
   return 0;
}
// -------------------------------------------------------------------------------------
