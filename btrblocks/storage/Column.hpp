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
  const ColumnType type;
  const string name;
  std::variant<Vector<INTEGER>, Vector<DOUBLE>, Vector<str>> data;
  Vector<BITMAP> bitmap;

  Column(const ColumnType type, string name, const string& data_path, const string& bitmap_path);
  [[nodiscard]] const Vector<INTEGER>& integers() const;
  [[nodiscard]] const Vector<DOUBLE>& doubles() const;
  [[nodiscard]] const Vector<str>& strings() const;
  [[nodiscard]] const Vector<BITMAP>& bitmaps() const;
  [[nodiscard]] SIZE sizeInBytes() const;
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
