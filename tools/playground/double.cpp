#include "common/Units.hpp"
#include "common/Reinterpret.hpp"
#include "common/PerfEvent.hpp"
// -------------------------------------------------------------------------------------
#include <iostream>
#include <sstream>
#include <iomanip> // setprecision
#include <cstring>
#include <cmath>
#include <bitset>
// -------------------------------------------------------------------------------------
#include <double-conversion/diy-fp.h>
#include <double-conversion/fast-dtoa.h>
#include <double-conversion/ieee.h>
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
static Vector<const char> StringToVector(const char *str) {
   return Vector<const char>(str, strlen(str));
}
// -------------------------------------------------------------------------------------
const u8 max_exponent = 22;
static const double exact_powers_of_ten[] = {
  1.0,  // 10^0
  10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0, 10000000000.0,  // 10^10
  100000000000.0, 1000000000000.0, 10000000000000.0, 100000000000000.0, 1000000000000000.0, 10000000000000000.0, 100000000000000000.0, 1000000000000000000.0, 10000000000000000000.0, 100000000000000000000.0,  // 10^20
  1000000000000000000000.0,
  // 10^22 = 0x21e19e0c9bab2400000 = 0x878678326eac9 * 2^22
  10000000000000000000000.0
};
// -------------------------------------------------------------------------------------
int main(int, char **) {
    std::string test_double("1.5");
    double d = std::stod(test_double);
    float d_f = static_cast<float>(d);
    cout << ((static_cast<double>(d_f) == d) ? "Y" : "N") << endl;

   //double wtf = std::stod("-2.2e-2");
//    printFloat(f);
//    printDouble(d);
// //   return 0;
//    // -------------------------------------------------------------------------------------
//    // GDouble printing algorithm

//    char buffer_container[1024];
//    Vector<char> buffer(buffer_container, 1024);
//    int length;
//    int point;
//    bool status;

//    double min_double = std::stod(test_double);
//    status = FastDtoa(min_double, FAST_DTOA_SHORTEST, 0,
//                      buffer, &length, &point);

   // -------------------------------------------------------------------------------------
   // Try optimized print double
   {
      //      d = stod("128.83");//cin >> d; 128.83 16.24
     double d, cd;
      cout << "enter your decimal to convert : ";
      cin >> d;

      double current_double = d;
      u32 e;
      u64 sd;
      bool convertable = false;
      for ( e = 0; e <= max_exponent; e++ ) {
         double cd = current_double * exact_powers_of_ten[e];
         cd = std::round(cd);
         sd = static_cast<u64>(cd);
         double if_converted_back = CD(sd) / exact_powers_of_ten[e];
         if ( if_converted_back == current_double && ((std::floor(std::log2(sd)) + 1) <= 32)) {
            cout << "awesome here is your i = " << sd << ", e = " << e << endl;
            convertable = true;
            break;
         }
      }
      if(!convertable){
        cout << "damn, not convertible !" << endl;
      }
   }
   // -------------------------------------------------------------------------------------
   return 0;
}
// -------------------------------------------------------------------------------------
