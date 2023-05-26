// -------------------------------------------------------------------------------------
#include "common/Utils.hpp"
// -------------------------------------------------------------------------------------
#include "CSchemePool.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v1/integer/Dictionary.hpp"
#include "compression/schemes/v1/integer/OneValue.hpp"
#include "compression/schemes/v1/integer/Truncation.hpp"
#include "compression/schemes/v1/integer/Uncompressed.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v1/double/Dictionary.hpp"
#include "compression/schemes/v1/double/OneValue.hpp"
#include "compression/schemes/v1/double/Uncompressed.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v1/string/Dictionary.hpp"
#include "compression/schemes/v1/string/OneValue.hpp"
#include "compression/schemes/v1/string/Uncompressed.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v2/string/DynamicDictionary.hpp"
#include "compression/schemes/v2/string/Fsst.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v2/double/Decimal.hpp"
#include "compression/schemes/v2/double/DynamicDictionary.hpp"
#include "compression/schemes/v2/double/Frequency.hpp"
#include "compression/schemes/v2/double/MaxExponent.hpp"
#include "compression/schemes/v2/double/RLE.hpp"
// #include "compression/schemes/v2/double/Hacky.hpp"
#include "compression/schemes/v2/double/DoubleBP.hpp"
// -------------------------------------------------------------------------------------
#include "compression/schemes/v2/integer/DynamicDictionary.hpp"
#include "compression/schemes/v2/integer/FOR.hpp"
#include "compression/schemes/v2/integer/Frequency.hpp"
#include "compression/schemes/v2/integer/PBP.hpp"
#include "compression/schemes/v2/integer/RLE.hpp"
// -------------------------------------------------------------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
DEFINE_bool(db1, false, "Only use the compression schemes in the original Datablocks paper");
DEFINE_bool(db2, true, "");
// Integer flags
DEFINE_bool(integer_for, true, "");  // disabled in final version
DEFINE_bool(integer_fbp, true, "");
DEFINE_bool(integer_pbp, true, "");
DEFINE_bool(integer_delta, false, "");  // disabled in final version
DEFINE_bool(integer_dictionary, true, "");
DEFINE_bool(integer_rle, true, "");
DEFINE_bool(integer_frequency, false,
            "");  // Disabled because of slow decompression speeds.
// Double flags
DEFINE_bool(double_dictionary, true, "");
DEFINE_bool(double_rle, true, "");
DEFINE_bool(double_frequency, true, "");
DEFINE_bool(double_decimal, true, "");
DEFINE_bool(double_bp, false, "");
// String flags
DEFINE_bool(string_dictionary, true, "");
DEFINE_bool(string_fsst, true, "");
// -------------------------------------------------------------------------------------
DEFINE_uint32(force_scheme, 255, "");
DEFINE_uint32(force_string_scheme, 255, "");
DEFINE_uint32(force_integer_scheme, 255, "");
DEFINE_uint32(force_double_scheme, 255, "");
// -------------------------------------------------------------------------------------
DEFINE_bool(try_all_schemes, false, "");
DEFINE_bool(sampling_test_mode, false, "");
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
// -------------------------------------------------------------------------------------
SchemesCollection::SchemesCollection() {
  if (FLAGS_db2) {
    // Integer
    {
      // These two are required.
      integer_schemes.emplace(v1::integer::OneValue::staticSchemeType(),
                              make_unique<v1::integer::OneValue>());
      integer_schemes.emplace(v1::integer::Uncompressed::staticSchemeType(),
                              make_unique<v1::integer::Uncompressed>());
      if (FLAGS_integer_for) {
        integer_schemes.emplace(v2::integer::FOR::staticSchemeType(),
                                make_unique<v2::integer::FOR>());
      }
      if (FLAGS_integer_fbp) {
        integer_schemes.emplace(v2::integer::FBP::staticSchemeType(),
                                make_unique<v2::integer::FBP>());
        // Never tried this one. Seems to be incomplete anyway.
        // integer_schemes.emplace(v2::integer::EXP_FBP::staticSchemeType(),
        // make_unique<v2::integer::EXP_FBP>());
      }
      if (FLAGS_integer_pbp) {
        integer_schemes.emplace(v2::integer::PBP::staticSchemeType(),
                                make_unique<v2::integer::PBP>());
      }
      if (FLAGS_integer_delta) {
        integer_schemes.emplace(v2::integer::PBP_DELTA::staticSchemeType(),
                                make_unique<v2::integer::PBP_DELTA>());
      }
      if (FLAGS_integer_dictionary) {
        integer_schemes.emplace(v2::integer::DynamicDictionary::staticSchemeType(),
                                make_unique<v2::integer::DynamicDictionary>());
      }
      if (FLAGS_integer_rle) {
        integer_schemes.emplace(v2::integer::RLE::staticSchemeType(),
                                make_unique<v2::integer::RLE>());
      }
      if (FLAGS_integer_frequency) {
        integer_schemes.emplace(v2::integer::Frequency::staticSchemeType(),
                                make_unique<v2::integer::Frequency>());
      }
    }

    // Double
    {
      // These two are required.
      double_schemes.emplace(v1::d::OneValue::staticSchemeType(), make_unique<v1::d::OneValue>());
      double_schemes.emplace(v1::d::Uncompressed::staticSchemeType(),
                             make_unique<v1::d::Uncompressed>());
      if (FLAGS_double_dictionary) {
        double_schemes.emplace(v2::d::DynamicDictionary::staticSchemeType(),
                               make_unique<v2::d::DynamicDictionary>());
      }
      if (FLAGS_double_rle) {
        double_schemes.emplace(v2::d::RLE::staticSchemeType(), make_unique<v2::d::RLE>());
      }
      if (FLAGS_double_frequency) {
        double_schemes.emplace(v2::d::Frequency::staticSchemeType(),
                               make_unique<v2::d::Frequency>());
      }
      if (FLAGS_double_decimal) {
        double_schemes.emplace(v2::d::Decimal::staticSchemeType(), make_unique<v2::d::Decimal>());
        // double_schemes.emplace(v2::d::MaxExponent::staticSchemeType(),
        // make_unique<v2::d::MaxExponent>());
      }
      if (FLAGS_double_bp) {
        double_schemes.emplace(v2::d::DoubleBP::staticSchemeType(), make_unique<v2::d::DoubleBP>());
      }
    }

    // String
    {
      // These two are required.
      string_schemes.emplace(v1::string::OneValue::staticSchemeType(),
                             make_unique<v1::string::OneValue>());
      string_schemes.emplace(v1::string::Uncompressed::staticSchemeType(),
                             make_unique<v1::string::Uncompressed>());
      if (FLAGS_string_dictionary) {
        string_schemes.emplace(v2::string::DynamicDictionary::staticSchemeType(),
                               make_unique<v2::string::DynamicDictionary>());
      }
      if (FLAGS_string_fsst) {
        string_schemes.emplace(v2::string::Fsst::staticSchemeType(),
                               make_unique<v2::string::Fsst>());
      }
    }
  } else {
    // -------------------------------------------------------------------------------------
    integer_schemes.emplace(v1::integer::OneValue::staticSchemeType(),
                            make_unique<v1::integer::OneValue>());
    integer_schemes.emplace(v1::integer::Uncompressed::staticSchemeType(),
                            make_unique<v1::integer::Uncompressed>());
    integer_schemes.emplace(v1::integer::Truncation8::staticSchemeType(),
                            make_unique<v1::integer::Truncation8>());
    integer_schemes.emplace(v1::integer::Truncation16::staticSchemeType(),
                            make_unique<v1::integer::Truncation16>());
    integer_schemes.emplace(v1::integer::Dictionary8::staticSchemeType(),
                            make_unique<v1::integer::Dictionary8>());
    integer_schemes.emplace(v1::integer::Dictionary16::staticSchemeType(),
                            make_unique<v1::integer::Dictionary16>());
    // -------------------------------------------------------------------------------------
    double_schemes.emplace(v1::d::OneValue::staticSchemeType(), make_unique<v1::d::OneValue>());
    double_schemes.emplace(v1::d::Uncompressed::staticSchemeType(),
                           make_unique<v1::d::Uncompressed>());
    double_schemes.emplace(v1::d::Dictionary8::staticSchemeType(),
                           make_unique<v1::d::Dictionary8>());
    double_schemes.emplace(v1::d::Dictionary16::staticSchemeType(),
                           make_unique<v1::d::Dictionary16>());
    // -------------------------------------------------------------------------------------
    string_schemes.emplace(v1::string::OneValue::staticSchemeType(),
                           make_unique<v1::string::OneValue>());
    string_schemes.emplace(v1::string::Uncompressed::staticSchemeType(),
                           make_unique<v1::string::Uncompressed>());
    string_schemes.emplace(v1::string::Dictionary8::staticSchemeType(),
                           make_unique<v1::string::Dictionary8>());
    string_schemes.emplace(v1::string::Dictionary16::staticSchemeType(),
                           make_unique<v1::string::Dictionary16>());
  }
}
// -------------------------------------------------------------------------------------
unique_ptr<SchemesCollection> CSchemePool::available_schemes =
    unique_ptr<SchemesCollection>(nullptr);
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db
