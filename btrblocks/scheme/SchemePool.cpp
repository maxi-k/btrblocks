// -------------------------------------------------------------------------------------
#include "SchemePool.hpp"
#include "btrblocks.hpp"
// -------------------------------------------------------------------------------------
#include "common/Utils.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/integer/DynamicDictionary.hpp"
#include "scheme/integer/Frequency.hpp"
#include "scheme/integer/OneValue.hpp"
#include "scheme/integer/PBP.hpp"
#include "scheme/integer/RLE.hpp"
#include "scheme/integer/Uncompressed.hpp"
// legacy schemes
#include "scheme/integer/FOR.hpp"
#include "scheme/integer/FixedDictionary.hpp"
#include "scheme/integer/Truncation.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/int64/DynamicDictionary.hpp"
#include "scheme/int64/OneValue.hpp"
#include "scheme/int64/PBP.hpp"
#include "scheme/int64/RLE.hpp"
#include "scheme/int64/Uncompressed.hpp"
// legacy schemes
#include "scheme/int64/Truncation.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/double/DynamicDictionary.hpp"
#include "scheme/double/OneValue.hpp"
#include "scheme/double/Pseudodecimal.hpp"
#include "scheme/double/RLE.hpp"
#include "scheme/double/Uncompressed.hpp"
// legacy schemes
#include "scheme/double/DoubleBP.hpp"
#include "scheme/double/FixedDictionary.hpp"
#include "scheme/double/Frequency.hpp"
#include "scheme/double/MaxExponent.hpp"
// -------------------------------------------------------------------------------------
#include "scheme/string/DynamicDictionary.hpp"
#include "scheme/string/Fsst.hpp"
#include "scheme/string/OneValue.hpp"
#include "scheme/string/Uncompressed.hpp"
// legacy schemes
#include "scheme/string/FixedDictionary.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
template <typename... T, typename SchemeMap, typename SchemeSet>
int addIfEnabled(SchemeMap& schemeMap, const SchemeSet& schemeSet) {
  // https://godbolt.org/z/Pv5Grc8f1
  return (... + [&]() {
    if (schemeSet.isEnabled(T::staticSchemeType())) {
      schemeMap.emplace(T::staticSchemeType(), std::make_unique<T>());
      return 1;
    }
    return 0;
  }());
}
// -------------------------------------------------------------------------------------
SchemesCollection::SchemesCollection() {
  auto& cfg = BtrBlocksConfig::get();
  // Integer Schemes
  {
    using namespace legacy::integers;
    using namespace integers;
    // required schemes
    die_if(cfg.integers.schemes.isEnabled(IntegerSchemeType::ONE_VALUE));
    die_if(cfg.integers.schemes.isEnabled(IntegerSchemeType::UNCOMPRESSED));
    // optional integer schemes
    // clang-format off
    addIfEnabled<Uncompressed,
                 OneValue,
                 DynamicDictionary,
                 RLE,
                 FBP,
                 PBP,
                 Frequency,
                 FOR,
                 PBP_DELTA,
                 Truncation8,
                 Truncation16,
                 Dictionary8,
                 Dictionary16>(integer_schemes, cfg.integers.schemes);
    // clang-format on
  }
  // Int64 schemes
  {
    using namespace legacy::int64s;
    using namespace int64s;
    // required schemes
    die_if(cfg.int64s.schemes.isEnabled(Int64SchemeType::ONE_VALUE));
    die_if(cfg.int64s.schemes.isEnabled(Int64SchemeType::UNCOMPRESSED));
    // optional integer schemes
    // clang-format off
    addIfEnabled<Uncompressed,
                 OneValue,
                 DynamicDictionary,
                 RLE,
                 FBP,
                 PBP,
                 Truncation8,
                 Truncation16>(int64_schemes, cfg.int64s.schemes);
    // clang-format on
  }
  // Double Schemes
  {
    using namespace legacy::doubles;
    using namespace doubles;
    // required schemes
    die_if(cfg.doubles.schemes.isEnabled(DoubleSchemeType::ONE_VALUE));
    die_if(cfg.doubles.schemes.isEnabled(DoubleSchemeType::UNCOMPRESSED));
    // optional double schemes
    // clang-format off
    addIfEnabled<Uncompressed,
                 OneValue,
                 DynamicDictionary,
                 RLE,
                 Frequency,
                 Decimal,
                 DoubleBP,
                 Dictionary8,
                 Dictionary16>(double_schemes, cfg.doubles.schemes);
    // clang-format on
  }
  // String Schemes
  {
    using namespace legacy::strings;
    using namespace strings;
    // required schemes
    die_if(cfg.strings.schemes.isEnabled(StringSchemeType::ONE_VALUE));
    die_if(cfg.strings.schemes.isEnabled(StringSchemeType::UNCOMPRESSED));
    // optional double schemes
    // clang-format off
    addIfEnabled<Uncompressed,
                 OneValue,
                 DynamicDictionary,
                 Fsst,
                 Dictionary8,
                 Dictionary16>(string_schemes, cfg.strings.schemes);
    // clang-format on
  }
}
// -------------------------------------------------------------------------------------
unique_ptr<SchemesCollection> SchemePool::available_schemes{nullptr};
// -------------------------------------------------------------------------------------
void SchemePool::refresh() {
  available_schemes = make_unique<btrblocks::SchemesCollection>();
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
