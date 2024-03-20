#include "common/Log.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include "MaxExponent.hpp"
#include "compression/SchemePicker.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
#include "roaring/roaring.hh"
// -------------------------------------------------------------------------------------
#include <bitset>
#include <cmath>
#include <iomanip>
// -------------------------------------------------------------------------------------
constexpr uint32_t max_exponent_scheme_siginifcant_digit_bits_limit = 64;
// -------------------------------------------------------------------------------------
using namespace std;
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

namespace btrblocks::legacy::doubles {

const u8 max_exponent = 22;
// const u8 exponent_exception_code = 23;
static const double exact_powers_of_ten[] = {
    1.0,  // 10^0
    10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0, 100000000.0, 1000000000.0,
    10000000000.0,  // 10^10
    100000000000.0, 1000000000000.0, 10000000000000.0, 100000000000000.0, 1000000000000000.0,
    10000000000000000.0, 100000000000000000.0, 1000000000000000000.0, 10000000000000000000.0,
    100000000000000000000.0,  // 10^20
    1000000000000000000000.0,
    // 10^22 = 0x21e19e0c9bab2400000 = 0x878678326eac9 * 2^22
    10000000000000000000000.0};
// -------------------------------------------------------------------------------------
/*
 * Plan:
 * 1- find the maximum exponent
 * 2- compress all, exceptions are stored in bitmap
 * only one exponent saved in structure.
 *
 * not necessarily slower than decimal, there is a chance to optimize, e.g don't
 * start from e=0 but from the latest max_exponent
 *
 */
u32 MaxExponent::compress(const DOUBLE* src,
                          const BITMAP* nullmap,
                          u8* dest,
                          DoubleStats& stats,
                          u8 allowed_cascading_level) {
  // Layout : Header | sd_v | e_v | p_v
  // ignore bitmap
  auto& col_struct = *reinterpret_cast<MaxExponentStructure*>(dest);
  vector<INTEGER> sd_v;
  vector<DOUBLE> p_v;  // patches
  // u32 not_convertable = 0;
  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    u32 e;
    u64 sd;
    bool convertable = false;
    double current_double = src[row_i];
    if (std::signbit(current_double)) {
      current_double = std::copysign(current_double, +1.0);
    }
    for (e = 0; e <= max_exponent; e++) {
      double cd = current_double * exact_powers_of_ten[e];
      cd = std::round(cd);
      sd = static_cast<u64>(cd);
      double if_converted_back = CD(sd) / exact_powers_of_ten[e];
      if (if_converted_back == current_double &&
          ((std::floor(std::log2(sd)) + 1) <= max_exponent_scheme_siginifcant_digit_bits_limit)) {
        // not_convertable++;
        break;
      }
    }
    if (convertable) {
      die_if((e & 0x20) == 0);
      if (col_struct.max_exponent < e) {
        col_struct.max_exponent = e;
      }
    } else if (nullmap != nullptr && nullmap[row_i]) {
      cout << row_i << endl;
      cout << std::fixed;
      cout.precision(std::numeric_limits<double>::max_digits10);
      double wtf = current_double * 1000000.0;
      cout << src[row_i] << '\t' << current_double << '\t' << wtf << endl;
      not_convertable++;
      printDouble(current_double);
    }
  }

  Roaring exceptions_bitmap;
  Roaring negative_bitmap;

  for (u32 row_i = 0; row_i < stats.tuple_count; row_i++) {
    u64 sd;
    bool convertable = false;
    double current_double = src[row_i];
    if (std::signbit(current_double)) {
      current_double = std::copysign(current_double, +1.0);
      negative_bitmap.add(row_i);
    }

    double cd = current_double * exact_powers_of_ten[col_struct.max_exponent];
    cd = std::round(cd);
    sd = static_cast<u64>(cd);
    double if_converted_back = CD(sd) / exact_powers_of_ten[col_struct.max_exponent];
    if (if_converted_back == current_double &&
        ((std::floor(std::log2(sd)) + 1) <= max_exponent_scheme_siginifcant_digit_bits_limit)) {
      convertable = true;
    }
    if (convertable) {
      sd_v.push_back(static_cast<INTEGER>(sd));
    } else {
      exceptions_bitmap.add(row_i);
      p_v.push_back(src[row_i]);
    }
  }
  // -------------------------------------------------------------------------------------
  exceptions_bitmap.runOptimize();
  exceptions_bitmap.setCopyOnWrite(true);
  negative_bitmap.runOptimize();
  negative_bitmap.setCopyOnWrite(true);
  // -------------------------------------------------------------------------------------
  col_struct.converted_count = sd_v.size();
  // -------------------------------------------------------------------------------------
  auto write_ptr = col_struct.data;
  write_ptr += exceptions_bitmap.write(reinterpret_cast<char*>(write_ptr), false);
  col_struct.negatives_bitmap_offset = write_ptr - col_struct.data;
  write_ptr += negative_bitmap.write(reinterpret_cast<char*>(write_ptr), false);
  // -------------------------------------------------------------------------------------
  // Compress significant digits
  if (sd_v.size()) {
    col_struct.sd_offset = write_ptr - col_struct.data;
    u32 used_space;
    IntegerSchemePicker::compress(sd_v.data(), nullptr, write_ptr, sd_v.size(),
                                  allowed_cascading_level - 1, used_space, col_struct.sd_scheme,
                                  autoScheme(), "significant digits");
    write_ptr += used_space;
    // -------------------------------------------------------------------------------------
    Log::debug("MaxExponent: sd_c = {} sd_s = {}", CI(col_struct.sd_scheme), CI(used_space));
    // -------------------------------------------------------------------------------------
  }
  // -------------------------------------------------------------------------------------
  // Compress patches
  {
    col_struct.p_offset = write_ptr - col_struct.data;
    u32 used_space;
    DoubleSchemePicker::compress(p_v.data(), nullptr, write_ptr, p_v.size(),
                                 allowed_cascading_level - 1, used_space, col_struct.p_scheme,
                                 autoScheme(), "patches");
    write_ptr += used_space;
    // -------------------------------------------------------------------------------------
    Log::debug("MaxExponent: p_c = {} p_s = {}", CI(col_struct.p_scheme), CI(used_space));
    // -------------------------------------------------------------------------------------
  }
  // -------------------------------------------------------------------------------------
  return write_ptr - dest;
}
// -------------------------------------------------------------------------------------
void MaxExponent::decompress(DOUBLE* dest,
                             BitmapWrapper*,
                             const u8* src,
                             u32 tuple_count,
                             u32 level) {}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::legacy::doubles
// -------------------------------------------------------------------------------------
