#ifndef BTRBLOCKS_SCHEMETYPE_H_
#define BTRBLOCKS_SCHEMETYPE_H_
// ------------------------------------------------------------------------------
#include <cstdint>
#include "SchemeSet.hpp"
// ------------------------------------------------------------------------------
namespace btrblocks {
// ------------------------------------------------------------------------------
// List of available compression schemes. Can be enabled/disabled for
// compression in using the configuration mechanism.
// ------------------------------------------------------------------------------
enum class IntegerSchemeType : uint8_t {
  UNCOMPRESSED = 0,
  ONE_VALUE = 1,
  DICT = 2,
  RLE = 3,
  PFOR = 4,
  BP = 5,
  // legacy schemes
  FREQUENCY = 25,
  FOR = 26,
  PFOR_DELTA = 27,
  TRUNCATION_8 = 28,
  TRUNCATION_16 = 29,
  DICTIONARY_8 = 30,
  DICTIONARY_16 = 31,
  SCHEME_MAX = 32
};
using IntegerSchemeSet = SchemeSet<IntegerSchemeType>;
constexpr IntegerSchemeSet defaultIntegerSchemes() {
  return {IntegerSchemeType::UNCOMPRESSED, IntegerSchemeType::ONE_VALUE, IntegerSchemeType::DICT,
          IntegerSchemeType::RLE,          IntegerSchemeType::PFOR,      IntegerSchemeType::BP};
};
// ------------------------------------------------------------------------------
enum class Int64SchemeType : uint8_t {
  UNCOMPRESSED = 0,
  ONE_VALUE = 1,
  DICT = 2,
  RLE = 3,
  PFOR = 4,
  BP = 5,
  // legacy schemes
  TRUNCATION_8 = 28,
  TRUNCATION_16 = 29,
  SCHEME_MAX = 32
};
using Int64SchemeSet = SchemeSet<Int64SchemeType>;
constexpr Int64SchemeSet defaultInt64Schemes() {
  return {Int64SchemeType::UNCOMPRESSED, Int64SchemeType::ONE_VALUE, Int64SchemeType::DICT,
          Int64SchemeType::RLE,          Int64SchemeType::PFOR,      Int64SchemeType::BP};
};
// ------------------------------------------------------------------------------
enum class DoubleSchemeType : uint8_t {
  UNCOMPRESSED = 0,
  ONE_VALUE = 1,
  DICT = 2,
  RLE = 3,
  FREQUENCY = 4,
  PSEUDODECIMAL = 5,
  ALP_RD = 6,
  ALP = 7,
  // legacy schemes
  DOUBLE_BP = 28,
  DICTIONARY_8 = 29,
  DICTIONARY_16 = 31,
  SCHEME_MAX = 32
};
using DoubleSchemeSet = SchemeSet<DoubleSchemeType>;
constexpr DoubleSchemeSet defaultDoubleSchemes() {
  return {DoubleSchemeType::UNCOMPRESSED, DoubleSchemeType::ONE_VALUE,
          DoubleSchemeType::DICT,         DoubleSchemeType::RLE,
          DoubleSchemeType::FREQUENCY,    DoubleSchemeType::PSEUDODECIMAL, DoubleSchemeType::ALP};
};
// ------------------------------------------------------------------------------
enum class StringSchemeType : uint8_t {
  UNCOMPRESSED = 0,
  ONE_VALUE = 1,
  DICT = 2,
  FSST = 3,
  // legacy schemes
  DICTIONARY_8 = 30,
  DICTIONARY_16 = 31,
  SCHEME_MAX = 32
};
using StringSchemeSet = SchemeSet<StringSchemeType>;
constexpr StringSchemeSet defaultStringSchemes() {
  return {StringSchemeType::UNCOMPRESSED, StringSchemeType::ONE_VALUE, StringSchemeType::DICT,
          StringSchemeType::FSST};
};
// ------------------------------------------------------------------------------
// When overriding schemes, pass this value to use automatic scheme selection.
constexpr auto autoScheme() {
  return 255;
}
// ------------------------------------------------------------------------------
}  // namespace btrblocks
// ------------------------------------------------------------------------------
#endif  // BTRBLOCKS_SCHEMETYPE_H_
