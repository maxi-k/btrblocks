// -------------------------------------------------------------------------------------
#include "common/Utils.hpp"
// -------------------------------------------------------------------------------------
#include "CScheme.hpp"
#include "CSchemePool.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
bool shouldUseFOR(str min) {
  return false;
}
bool shouldUseFOR(DOUBLE min) {
  return false;
  return (Utils::getBitsNeeded(static_cast<u32>(min)) >= 8);
}
bool shouldUseFOR(INTEGER min) {
  return false;
  return (Utils::getBitsNeeded(min) >= 8);
}
}  // namespace btrblocks::db
