#include "Column.hpp"
#include "common/Exceptions.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
Column::Column(const ColumnType type,
               const string& name,
               const string& data_path,
               const string& bitmap_path)
    : type(type), name(name) {
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
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
