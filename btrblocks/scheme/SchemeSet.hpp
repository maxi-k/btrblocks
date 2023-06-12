#ifndef BTRBLOCKS_SCHEMESET_H_
#define BTRBLOCKS_SCHEMESET_H_
// ------------------------------------------------------------------------------
#include <bitset>
// ------------------------------------------------------------------------------
namespace btrblocks {
// ------------------------------------------------------------------------------
// Compact way of storing a set of schemes (e.g. enabled schemes)  in a bitset.
// ------------------------------------------------------------------------------
template <typename T, std::size_t N = static_cast<std::size_t>(T::SCHEME_MAX)>
struct SchemeSet {
  constexpr SchemeSet(std::initializer_list<T> schemes) { enable(schemes); }

  constexpr SchemeSet& enable(std::initializer_list<T> schemes) {
    for (auto& s : schemes) {
      enable(s);
    }
    return *this;
  }

  constexpr SchemeSet& disable(std::initializer_list<T>&& schemes) {
    for (auto& s : schemes) {
      disable(s);
    }
    return *this;
  }

  constexpr SchemeSet& enable(T s) {
    set.set(static_cast<std::size_t>(s));
    return *this;
  }

  constexpr SchemeSet& disable(T s) {
    set.set(static_cast<std::size_t>(s), false);
    return *this;
  }

  [[nodiscard]] constexpr bool isEnabled(T s) const {
    return set.test(static_cast<std::size_t>(s));
  }

  constexpr void enableAll() { set.set(); }

  constexpr static SchemeSet all() {
    SchemeSet s;
    s.enableAll();
    return s;
  }

 private:
  std::bitset<N> set;
};
// ------------------------------------------------------------------------------
}  // namespace btrblocks
// ------------------------------------------------------------------------------
#endif  // BTRBLOCKS_SCHEMESET_H_
