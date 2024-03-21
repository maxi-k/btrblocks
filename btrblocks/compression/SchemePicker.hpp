#pragma once
// -------------------------------------------------------------------------------------
#include "btrblocks.hpp"
#include "common/Log.hpp"
#include "common/Utils.hpp"
// -------------------------------------------------------------------------------------
#include "cache/ThreadCache.hpp"
#include "compression/Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/CompressionScheme.hpp"
#include "scheme/SchemePool.hpp"
// -------------------------------------------------------------------------------------
#if defined(BTR_FLAG_ENABLE_FOR_SCHEME) && BTR_FLAG_ENABLE_FOR_SCHEME
#define BTR_ENABLE_FOR_SCHEME 1
constexpr bool enableFORScheme() {
  return true;
}
#else
#undef BTR_ENABLE_FOR_SCHEME
constexpr bool enableFORScheme() {
  return false;
}
#endif
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
template <typename SchemeType, typename SchemeCodeType>
class TypeWrapper {};
// -------------------------------------------------------------------------------------
template <typename Type, typename SchemeType, typename StatsType, typename SchemeCodeType>
class CSchemePicker {
 public:
  using MyTypeWrapper = TypeWrapper<SchemeType, SchemeCodeType>;
  static SchemeType& chooseScheme(StatsType& stats, u8 allowed_cascading_level) {
    double max_compression_ratio = 0;
    if (MyTypeWrapper::getOverrideScheme() != autoScheme()) {
      auto scheme_code = MyTypeWrapper::getOverrideScheme();
      MyTypeWrapper::getOverrideScheme() = autoScheme();
      return MyTypeWrapper::getScheme(scheme_code);
    } else {
      SchemeType* preferred_scheme = nullptr;
      for (auto& scheme : MyTypeWrapper::getSchemes()) {
        if (ThreadCache::get().estimation_level != 0 || ThreadCache::get().compression_level > 1) {
          if (scheme.second->schemeType() == SchemeCodeType::ONE_VALUE) {
            continue;
          }
        }

        if (!scheme.second->isUsable(stats)) {
          continue;
        }

        auto compression_ratio =
            scheme.second->expectedCompressionRatio(stats, allowed_cascading_level);
        if (compression_ratio > max_compression_ratio) {
          max_compression_ratio = compression_ratio;
          preferred_scheme = scheme.second.get();
        }
      }
      if (max_compression_ratio < 1.0) {
        throw Generic_Exception("compression ratio lower than 1");
      }
      if (preferred_scheme == nullptr) {
        throw Generic_Exception("No compression scheme found for the input !!");
      }
      return *preferred_scheme;
    }
  }
  // -------------------------------------------------------------------------------------
  static void compress(const Type* src,
                       const BITMAP* nullmap,
                       u8* dest,
                       u32 tuple_count,
                       u8 allowed_cascading_level,
                       u32& after_size,
                       u8& scheme_code,
                       u8 force_scheme = autoScheme(),
                       const string& comment = "?") {
    Log::debug("Compressing with max level {}", allowed_cascading_level);
    ThreadCache::get().compression_level++;
    StatsType stats = StatsType::generateStats(src, nullmap, tuple_count);
    SchemeType* preferred_scheme = nullptr;

    // -----------------------------------------------------------------------------------
    // If flag is set, simply test all schemes on the entire data to get optimal
    // compression ratio (and thus test how much worse sampling vs optimal is).
    // -----------------------------------------------------------------------------------
#if defined(BTR_FLAG_SAMPLING_TEST_MODE) and BTR_FLAG_SAMPLING_TEST_MODE
    if (allowed_cascading_level == 0 || tuple_count == 0) {
      Log::debug(MyTypeWrapper::getTypeName() + ": UNCOMPRESSED");
      preferred_scheme = &MyTypeWrapper::getScheme(SchemeCodeType::UNCOMPRESSED);
      after_size = preferred_scheme->compress(src, nullmap, dest, stats, 0);
      scheme_code = CB(preferred_scheme->schemeType());
    } else {
      auto tmp_dest = makeBytesArray(stats.total_size * 10);
      u32 least_after_size = std::numeric_limits<u32>::max();
      // SchemeType *preferred_scheme = nullptr;
      for (auto& scheme : MyTypeWrapper::getSchemes()) {
        if (scheme.second->expectedCompressionRatio(stats, allowed_cascading_level) > 0) {
          u32 after_size =
              scheme.second->compress(src, nullmap, tmp_dest.get(), stats, allowed_cascading_level);
          if (after_size < least_after_size) {
            least_after_size = after_size;
            preferred_scheme = scheme.second.get();
          }
        }
      }
      die_if(preferred_scheme != nullptr);
      scheme_code = CB(preferred_scheme->schemeType());
      Log::debug((MyTypeWrapper::getTypeName() + ": {}").c_str(), scheme_code);
      after_size = preferred_scheme->compress(src, nullmap, dest, stats, allowed_cascading_level);
    }
    ThreadCache::get().compression_level--;
    return;
#endif
    // -----------------------------------------------------------------------------------

    if ((ThreadCache::get().estimation_level == 0 && ThreadCache::get().compression_level == 1) &&
        (stats.null_count == stats.tuple_count || stats.unique_count == 1)) {
      Log::debug(MyTypeWrapper::getTypeName() + ": ONE_VALUE");
      preferred_scheme = &MyTypeWrapper::getScheme(SchemeCodeType::ONE_VALUE);
      scheme_code = CB(preferred_scheme->schemeType());
      after_size = preferred_scheme->compress(src, nullmap, dest, stats, 0);
    } else if (allowed_cascading_level == 0 || tuple_count == 0) {
      Log::debug(MyTypeWrapper::getTypeName() + ": UNCOMPRESSED");
      preferred_scheme = &MyTypeWrapper::getScheme(SchemeCodeType::UNCOMPRESSED);
      after_size = preferred_scheme->compress(src, nullmap, dest, stats, 0);
      scheme_code = CB(preferred_scheme->schemeType());
    }
#ifdef BTR_ENABLE_FOR_SCHEME
    else if (MyTypeWrapper::shouldUseFOR(stats.min) && allowed_cascading_level > 1) {
      Log::debug(MyTypeWrapper::getTypeName() + ": FOR");
      preferred_scheme = &MyTypeWrapper::getFORScheme();
      after_size = preferred_scheme->compress(src, nullmap, dest, stats, allowed_cascading_level);
      scheme_code = CB(preferred_scheme->schemeType());
    }
#endif
    else {
      switch (BtrBlocksConfig::get().scheme_selection) {
        // try all schemes
        case SchemeSelection::TRY_ALL: {
          auto tmp_dest = makeBytesArray(stats.total_size * 10);
          u32 least_after_size = std::numeric_limits<u32>::max();
          // SchemeType *preferred_scheme = nullptr;
          for (auto& scheme : MyTypeWrapper::getSchemes()) {
            if (scheme.second->expectedCompressionRatio(stats, allowed_cascading_level) > 0) {
              u32 after_size = scheme.second->compress(src, nullmap, tmp_dest.get(), stats,
                                                       allowed_cascading_level);
              if (after_size < least_after_size) {
                least_after_size = after_size;
                preferred_scheme = scheme.second.get();
              }
            }
          }
          die_if(preferred_scheme != nullptr);
          scheme_code = CB(preferred_scheme->schemeType());
          Log::debug((MyTypeWrapper::getTypeName() + ": {}").c_str(), scheme_code);
          after_size =
              preferred_scheme->compress(src, nullmap, dest, stats, allowed_cascading_level);
          break;
        }
        case SchemeSelection::SAMPLE: {
          if (force_scheme != autoScheme()) {
            preferred_scheme = &MyTypeWrapper::getScheme(force_scheme);
          } else if (MyTypeWrapper::getOverrideScheme() != autoScheme()) {
            preferred_scheme = &MyTypeWrapper::getScheme(MyTypeWrapper::getOverrideScheme());
            MyTypeWrapper::getOverrideScheme() = autoScheme();
          } else {
            preferred_scheme = &chooseScheme(stats, allowed_cascading_level);
          }
          die_if(preferred_scheme != nullptr);
          scheme_code = CB(preferred_scheme->schemeType());
          Log::debug((MyTypeWrapper::getTypeName() + ": {}").c_str(), scheme_code);
          after_size =
              preferred_scheme->compress(src, nullmap, dest, stats, allowed_cascading_level);

          break;
        }
      }
    }
    if (ThreadCache::get().isOnHotPath()) {
      if ((after_size > stats.total_size)) {
        cerr << "!!! compressed is larger than raw: \nfor : " + comment + " - scheme = " +
                    ConvertSchemeTypeToString(static_cast<SchemeCodeType>(scheme_code))
             << " difference = " << after_size - stats.total_size << "."
             << " Falling back to uncompressed." << endl;
        preferred_scheme = &MyTypeWrapper::getScheme(SchemeCodeType::UNCOMPRESSED);
        scheme_code = CB(preferred_scheme->schemeType());
        after_size = preferred_scheme->compress(src, nullmap, dest, stats, allowed_cascading_level);
      }
#if defined(BTR_FLAG_LOGGING) and BTR_FLAG_LOGGING
      for (u8 i = 0; i < MyTypeWrapper::maxCascadingLevel() - allowed_cascading_level; i++) {
        ThreadCache::get() << "\t";
      }
      ThreadCache::get() << "for : " + comment + " - scheme = " +
                                ConvertSchemeTypeToString(
                                    static_cast<SchemeCodeType>(scheme_code)) +
                                " before = " + std::to_string(stats.total_size) +
                                " after = " + std::to_string(after_size) +
                                " gain = " + std::to_string(CD(stats.total_size) / CD(after_size)) +
                                '\n';
#endif
      double estimated_cf =
          preferred_scheme->expectedCompressionRatio(stats, allowed_cascading_level);
      ThreadCache::dumpPush(ConvertSchemeTypeToString(static_cast<SchemeCodeType>(scheme_code)),
                            estimated_cf, stats.total_size, after_size, stats.unique_count,
                            comment);

      // if ( estimated_cf / (CD(stats.total_size) / CD(after_size)) >= 100 ) {
      //    for ( u32 row_i = 0; row_i < tuple_count; row_i++ ) {
      //       if ( nullmap == nullptr || nullmap[row_i] ) {
      //          cout << src[row_i] << ';';
      //       } else {
      //          cout << "N;";
      //       }
      //    }
      //    cout << endl;
      // }
    }
    ThreadCache::get().compression_level--;
  }
};
// -------------------------------------------------------------------------------------
template <>
class TypeWrapper<IntegerScheme, IntegerSchemeType> {
 public:
  // -------------------------------------------------------------------------------------
  static std::unordered_map<IntegerSchemeType, unique_ptr<IntegerScheme>>& getSchemes() {
    return SchemePool::available_schemes->integer_schemes;
  }
  // -------------------------------------------------------------------------------------
  static IntegerScheme& getScheme(IntegerSchemeType code) { return *getSchemes()[code]; }
  // -------------------------------------------------------------------------------------
  static IntegerScheme& getScheme(u8 code) {
    return *getSchemes()[static_cast<IntegerSchemeType>(code)];
  }
  // -------------------------------------------------------------------------------------
  static u8& getOverrideScheme() {
    auto& ref = BtrBlocksConfig::get().integers.override_scheme;
    return reinterpret_cast<u8&>(ref);
  }
  // -------------------------------------------------------------------------------------
  static inline string getTypeName() { return "INTEGER"; }
  // -------------------------------------------------------------------------------------
  constexpr static bool shouldUseFOR(INTEGER min) {
    return enableFORScheme() && (Utils::getBitsNeeded(min) >= 8) &&
           BtrBlocksConfig::get().integers.schemes.isEnabled(IntegerSchemeType::FOR);
  }
  static IntegerScheme& getFORScheme() { return getScheme(IntegerSchemeType::FOR); }
  // -------------------------------------------------------------------------------------
  static u8 maxCascadingLevel() { return BtrBlocksConfig::get().integers.max_cascade_depth; }
  // -------------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
template <>
class TypeWrapper<Int64Scheme, Int64SchemeType> {
 public:
  // -------------------------------------------------------------------------------------
  static std::unordered_map<Int64SchemeType, unique_ptr<Int64Scheme>>& getSchemes() {
    return SchemePool::available_schemes->int64_schemes;
  }
  // -------------------------------------------------------------------------------------
  static Int64Scheme& getScheme(Int64SchemeType code) { return *getSchemes()[code]; }
  // -------------------------------------------------------------------------------------
  static Int64Scheme& getScheme(u8 code) {
    return *getSchemes()[static_cast<Int64SchemeType>(code)];
  }
  // -------------------------------------------------------------------------------------
  static u8& getOverrideScheme() {
    auto& ref = BtrBlocksConfig::get().int64s.override_scheme;
    return reinterpret_cast<u8&>(ref);
  }
  // -------------------------------------------------------------------------------------
  static inline string getTypeName() { return "INT64"; }
  // -------------------------------------------------------------------------------------
  constexpr static bool shouldUseFOR(INT64) {return false;}
  static IntegerScheme& getFORScheme() {
    throw std::logic_error("FOR not implemented for int64 yet.");
  }
  // -------------------------------------------------------------------------------------
  static u8 maxCascadingLevel() { return BtrBlocksConfig::get().int64s.max_cascade_depth; }
  // -------------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
template <>
class TypeWrapper<DoubleScheme, DoubleSchemeType> {
 public:
  // -------------------------------------------------------------------------------------
  static std::unordered_map<DoubleSchemeType, unique_ptr<DoubleScheme>>& getSchemes() {
    return SchemePool::available_schemes->double_schemes;
  }
  // -------------------------------------------------------------------------------------
  static DoubleScheme& getScheme(DoubleSchemeType code) { return *getSchemes()[code]; }
  // -------------------------------------------------------------------------------------
  static DoubleScheme& getScheme(u8 code) {
    return *getSchemes()[static_cast<DoubleSchemeType>(code)];
  }
  // -------------------------------------------------------------------------------------
  static u8& getOverrideScheme() {
    auto& ref = BtrBlocksConfig::get().doubles.override_scheme;
    return reinterpret_cast<u8&>(ref);
  }
  // -------------------------------------------------------------------------------------
  static inline string getTypeName() { return "DOUBLE"; }
  // -------------------------------------------------------------------------------------
  constexpr static bool shouldUseFOR(DOUBLE) { return false; }
  static DoubleScheme& getFORScheme() {
    throw std::logic_error("FOR not implemented for doubles.");
  }
  // -------------------------------------------------------------------------------------
  static u8 maxCascadingLevel() { return BtrBlocksConfig::get().doubles.max_cascade_depth; }
  // -------------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
template <>
class TypeWrapper<StringScheme, StringSchemeType> {
 public:
  // -------------------------------------------------------------------------------------
  static std::unordered_map<StringSchemeType, unique_ptr<StringScheme>>& getSchemes() {
    return SchemePool::available_schemes->string_schemes;
  }
  // -------------------------------------------------------------------------------------
  static StringScheme& getScheme(StringSchemeType code) { return *getSchemes()[code]; }
  // -------------------------------------------------------------------------------------
  static StringScheme& getScheme(u8 code) {
    return *getSchemes()[static_cast<StringSchemeType>(code)];
  }
  // -------------------------------------------------------------------------------------
  static u8& getOverrideScheme() {
    auto& ref = BtrBlocksConfig::get().strings.override_scheme;
    return reinterpret_cast<u8&>(ref);
  }
  // -------------------------------------------------------------------------------------
  static inline string getTypeName() { return "STRING"; }
  // -------------------------------------------------------------------------------------
  constexpr static bool shouldUseFOR(str) { return false; }
  static StringScheme& getFORScheme() {
    throw std::logic_error("FOR not implemented for strings.");
  }
  // -------------------------------------------------------------------------------------
  static u8 maxCascadingLevel() { return BtrBlocksConfig::get().strings.max_cascade_depth; }
  // -------------------------------------------------------------------------------------
};
// -------------------------------------------------------------------------------------
using IntegerSchemePicker = CSchemePicker<INTEGER, IntegerScheme, SInteger32Stats, IntegerSchemeType>;
using Int64SchemePicker = CSchemePicker<INT64, Int64Scheme, SInt64Stats, Int64SchemeType>;
using DoubleSchemePicker = CSchemePicker<DOUBLE, DoubleScheme, DoubleStats, DoubleSchemeType>;
using StringSchemePicker = CSchemePicker<str, StringScheme, StringStats, StringSchemeType>;
}  // namespace btrblocks
