#pragma once
#include "CScheme.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
// -------------------------------------------------------------------------------------
struct SchemesCollection {
  std::unordered_map<IntegerSchemeType, unique_ptr<IntegerScheme>> integer_schemes;
  std::unordered_map<DoubleSchemeType, unique_ptr<DoubleScheme>> double_schemes;
  std::unordered_map<StringSchemeType, unique_ptr<StringScheme>> string_schemes;
  SchemesCollection();
};
// -------------------------------------------------------------------------------------
class CSchemePool {
 public:
  static unique_ptr<SchemesCollection> available_schemes;
  // -------------------------------------------------------------------------------------
  static void refresh();
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db
