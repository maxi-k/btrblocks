#include "Column.hpp"

#include <utility>
#include "common/Exceptions.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
Column::Column(const ColumnType type,
               string name,
               const string& data_path,
               const string& bitmap_path)
    : type(type), name(std::move(name)) {
  switch (type) {
    case ColumnType::INTEGER:
      data.emplace<0>(data_path.c_str());
      break;
    case ColumnType::DOUBLE:
      data.emplace<1>(data_path.c_str());
      break;
    case ColumnType::STRING:
      data.emplace<2>(data_path.c_str());
      break;
    default:
      UNREACHABLE();
      break;
  }
  bitmap.readBinary(bitmap_path.c_str());
}
// -------------------------------------------------------------------------------------
Column::Column(string name, Data&& data, Vector<BITMAP>&& bitmap)
    : type([&](const Data& d) {
        if (std::holds_alternative<Vector<INTEGER>>(d)) {
          return ColumnType::INTEGER;
        } else if (std::holds_alternative<Vector<DOUBLE>>(d)) {
          return ColumnType::DOUBLE;
        } else if (std::holds_alternative<Vector<str>>(d)) {
          return ColumnType::STRING;
        } else {
          UNREACHABLE();
        }
      }(data)),
      name(std::move(name)),
      data(std::move(data)),
      bitmap(std::move(bitmap)) {}
// -------------------------------------------------------------------------------------
Column::Column(string name, Data&& data)
    : Column(std::move(name),
             std::move(data),
             fullBitmap(std::visit([](const auto& d) { return d.size(); }, data))) {}
// -------------------------------------------------------------------------------------
const Vector<INTEGER>& Column::integers() const {
  return std::get<0>(data);
}
// -------------------------------------------------------------------------------------
const Vector<DOUBLE>& Column::doubles() const {
  return std::get<1>(data);
}
// -------------------------------------------------------------------------------------
const Vector<str>& Column::strings() const {
  return std::get<2>(data);
}
// -------------------------------------------------------------------------------------
const Vector<BITMAP>& Column::bitmaps() const {
  return bitmap;
}
// -------------------------------------------------------------------------------------
SIZE Column::size() const {
  return std::visit([](const auto& d) { return d.size(); }, data);
}
// -------------------------------------------------------------------------------------
SIZE Column::sizeInBytes() const {
  switch (type) {
    case ColumnType::INTEGER:
      return integers().size() * sizeof(INTEGER);
      break;
    case ColumnType::DOUBLE:
      return doubles().size() * sizeof(DOUBLE);
      break;
    case ColumnType::STRING:
      return strings().fileSize;
      break;
    default:
      UNREACHABLE();
      break;
  }
}
// -------------------------------------------------------------------------------------
Vector<BITMAP> Column::fullBitmap(SIZE count) {
  Vector<BITMAP> result(count);
  std::fill(result.begin(), result.end(), 1);
  return result;
}
}  // namespace btrblocks
// ------------------------------------------------------------------------------
