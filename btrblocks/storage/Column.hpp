#pragma once
#include "MMapVector.hpp"
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
#include <variant>
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
class Column {
 public:
  using Data = std::variant<Vector<INTEGER>, Vector<DOUBLE>, Vector<str>, Vector<INT64>>;
  const ColumnType type;
  const string name;
  Data data;
  Vector<BITMAP> bitmap;

  // read from file system
  Column(const ColumnType type, string name, const string& data_path, const string& bitmap_path);
  // read from existing in-memory data
  Column(string name, Data&& data, Vector<BITMAP>&& bitmap);
  // from existing in-memory data, assuming full bitmap (no nulls)
  Column(string name, Data&& data);

  [[nodiscard]] const Vector<INTEGER>& integers() const;
  [[nodiscard]] const Vector<INT64>& int64s() const;
  [[nodiscard]] const Vector<DOUBLE>& doubles() const;
  [[nodiscard]] const Vector<str>& strings() const;
  [[nodiscard]] const Vector<BITMAP>& bitmaps() const;
  [[nodiscard]] SIZE size() const;
  [[nodiscard]] SIZE sizeInBytes() const;

  // a bitmap that is all 1s
  static Vector<BITMAP> fullBitmap(SIZE count);
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
