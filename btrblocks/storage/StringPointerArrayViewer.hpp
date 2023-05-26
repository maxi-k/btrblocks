#pragma once
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
/*
 * This supports a minimal interface. In theory we could unify this interface
 * and the interface of StringArrayViewer
 */
struct StringPointerArrayViewer {
  struct View {
    u32 length;
    u32 offset;
  };
  static_assert(sizeof(View) == 8);
  const View* views;

  explicit StringPointerArrayViewer(const u8* data) {
    this->views = reinterpret_cast<const View*>(data);
  }

  inline str operator()(u32 i) const {
    return {reinterpret_cast<const char*>(this->views) + views[i].offset, views[i].length};
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
