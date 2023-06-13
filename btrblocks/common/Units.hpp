#pragma once
// -------------------------------------------------------------------------------------
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
#include "common/Exceptions.hpp"
#include "common/SIMD.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
inline namespace units {
// -------------------------------------------------------------------------------------
enum { NULL_CODE_MARGIN = 1 };
// -------------------------------------------------------------------------------------
using std::cerr;
using std::cout;
using std::endl;
using std::make_unique;
using std::string;
using std::tuple;
using std::unique_ptr;
using std::vector;
// -------------------------------------------------------------------------------------
using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
// -------------------------------------------------------------------------------------
using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;
// -------------------------------------------------------------------------------------
#define CB(enum) static_cast<u8>(enum)
#define CD(num) static_cast<double>(num)
#define CU(num) static_cast<u32>(num)
#define CI(num) static_cast<s32>(num)
#define CS(num) static_cast<size_t>(num)
// -------------------------------------------------------------------------------------
using SIZE = size_t;
// -------------------------------------------------------------------------------------
enum class BitmapType : u8 { ALLONES, ALLZEROS, REGULAR, FLIPPED };
// -------------------------------------------------------------------------------------
enum class ColumnType : u8 {
  INTEGER = 0,
  DOUBLE = 1,
  STRING = 2,
  SKIP,  // SKIP THIS COLUMN
  // The next types are out of scope
  FLOAT,
  BIGINT,
  SMALLINT,
  UNDEFINED
};
using TINYINT = s8;
using SMALLINT = s16;
using INTEGER = s32;  // we use FOR always at the beginning so negative integers
                      // will be handled out
using UINTEGER = u32;
using DOUBLE = double;
using STRING = string;
using BITMAP = u8;
// -------------------------------------------------------------------------------------
using str = std::string_view;
// -------------------------------------------------------------------------------------
inline ColumnType ConvertStringToType(const string& type_str) {
  if (type_str == "integer") {
    return ColumnType::INTEGER;
  } else if (type_str == "double") {
    return ColumnType::DOUBLE;
  } else if (type_str == "string") {
    return ColumnType::STRING;
  } else {
    return ColumnType::SKIP;
  }
};
// --------------------------------------------------------------------------------------
inline string ConvertTypeToString(const ColumnType type_str) {
  if (type_str == ColumnType::INTEGER) {
    return "integer";
  } else if (type_str == ColumnType::DOUBLE) {
    return "double";
  } else if (type_str == ColumnType::STRING) {
    return "string";
  } else {
    UNREACHABLE();
    return "";
  }
};
// -------------------------------------------------------------------------------------
using BytesArray = std::unique_ptr<u8[]>;
// Does not use make_unique because that zeros out the memory which is
// expensive.
#define makeBytesArray(size) std::unique_ptr<u8[]>(new u8[size])

// -------------------------------------------------------------------------------------
#define TEST_DATASET(file) "test-dataset/" file ""
// -------------------------------------------------------------------------------------
template <typename T>
inline constexpr void writeRaw(u8* base, u32 offset, T value) {
  *reinterpret_cast<T*>(base + offset) = value;
}
// -------------------------------------------------------------------------------------
template <typename T>
inline constexpr T readRaw(const u8* base, u32 offset) {
  return *reinterpret_cast<const T*>(base + offset);
}
// -------------------------------------------------------------------------------------
template <typename T>
inline constexpr T readRaw(u8* base, u32 offset) {  // TODO: Deprecate
  return *reinterpret_cast<const T*>(base + offset);
}
// -------------------------------------------------------------------------------------
template <typename T>
inline T* get_level_data(std::vector<std::vector<T>>& v, std::size_t s, std::size_t level) {
  v.resize(std::max(level + 1, v.size()));
  v[level].resize(std::max(s, v[level].size()));
  return v[level].data();
}

template <typename T>
inline T* get_data(std::vector<T>& v, std::size_t s) {
  v.resize(std::max(s, v.size()));
  return v.data();
}
#define WRITE_RAW(base, offset, type, value) *reinterpret_cast<(type)*>((base) + (offset)) = value
// -------------------------------------------------------------------------------------
enum { INT_BUFFER_SIZE = (65000 * 4 * 2) };
// -------------------------------------------------------------------------------------
enum { NULL_CODE = 0 };
// -------------------------------------------------------------------------------------
}  // namespace units
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
