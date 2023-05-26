#pragma once
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
// just a view
struct StringArrayViewer {
  struct Slot {
    INTEGER offset;
  };
  const u8* slots_ptr;

  explicit StringArrayViewer(const u8* slots_ptr) : slots_ptr(slots_ptr) {}

  inline static const str get(const u8* slots_ptr, u32 i) {
    auto slots = reinterpret_cast<const Slot*>(slots_ptr);
    return str(
        reinterpret_cast<const char*>(reinterpret_cast<const u8*>(slots_ptr) + slots[i].offset),
        slots[i + 1].offset - slots[i].offset);
  }

  [[nodiscard]] inline u32 size(u32 i) const {
    auto slots = reinterpret_cast<const Slot*>(slots_ptr);
    return slots[i + 1].offset - slots[i].offset;
  }

  inline const str operator()(u32 i) const {
    auto slots = reinterpret_cast<const Slot*>(slots_ptr);
    const u32 str_length = slots[i + 1].offset - slots[i].offset;
    return str(reinterpret_cast<const char*>(slots_ptr + slots[i].offset), str_length);
  }

  [[nodiscard]] inline const char* get_pointer(u32 i) const {
    auto slots = reinterpret_cast<const Slot*>(slots_ptr);
    return reinterpret_cast<const char*>(slots_ptr + slots[i].offset);
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
