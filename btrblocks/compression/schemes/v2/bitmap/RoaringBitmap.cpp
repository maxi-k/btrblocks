#include "RoaringBitmap.hpp"
// -------------------------------------------------------+------------------------------
#include "common/Log.hpp"
// -------------------------------------------------------+------------------------------
#include "gflags/gflags.h"
// -------------------------------------------------------------------------------------
namespace btrblocks::db::v2::bitmap {
BitmapWrapper::BitmapWrapper(const u8* src,
                             BitmapType type,
                             u32 tuple_count,
                             boost::dynamic_bitset<>* bitset)
    : m_tuple_count(tuple_count), m_bitset(bitset), m_type(type) {
  if (type == BitmapType::ALLONES) {
    this->m_cardinality = tuple_count;
    return;
  } else if (type == BitmapType::ALLZEROS) {
    this->m_cardinality = 0;
    return;
  }

  this->m_roaring = Roaring::read(reinterpret_cast<const char*>(src), false);
  this->m_cardinality = this->m_roaring.cardinality();
  if (type == BitmapType::FLIPPED) {
    this->m_cardinality = this->m_tuple_count - this->m_cardinality;
  }
}

BitmapWrapper::~BitmapWrapper() {
  delete this->m_bitset;
}

void BitmapWrapper::writeBITMAP(BITMAP* dest) {
  switch (this->m_type) {
    case BitmapType::ALLONES: {
      for (u32 i = 0; i < this->m_tuple_count; i++) {
        dest[i] = 1;
      }
      break;
    }
    case BitmapType::ALLZEROS: {
      for (u32 i = 0; i < this->m_tuple_count; i++) {
        dest[i] = 0;
      }
      break;
    }
    default: {
      for (u32 i = 0; i < this->m_tuple_count; i++) {
        dest[i] = this->get_bitset()->test(i) ? 1 : 0;
      }
      break;
    }
  }
}

std::vector<BITMAP> BitmapWrapper::writeBITMAP() {
 std::vector<BITMAP> result(this->m_tuple_count);
 writeBITMAP(result.data());
 return result;
}

void BitmapWrapper::releaseBitset() {
  this->m_bitset = nullptr;
  this->m_bitset_initialized = false;
}

boost::dynamic_bitset<>* BitmapWrapper::get_bitset() {
  if (this->m_bitset_initialized) {
    return this->m_bitset;
  }

  if (this->m_bitset == nullptr) {
    this->m_bitset = new boost::dynamic_bitset<>(this->m_tuple_count);
  }

  switch (this->m_type) {
    case BitmapType::ALLONES: {
      this->m_bitset->set();
      break;
    }
    case BitmapType::ALLZEROS: {
      this->m_bitset->reset();
      break;
    }
    case BitmapType::REGULAR: {
      this->m_bitset->reset();
      this->m_roaring.iterate(
          [](uint32_t value, void* param) {
            auto bitset = reinterpret_cast<boost::dynamic_bitset<>*>(param);
            bitset->set(value, true);
            return true;
          },
          this->m_bitset);
      break;
    }
    case BitmapType::FLIPPED: {
      this->m_bitset->set();
      this->m_roaring.iterate(
          [](uint32_t value, void* param) {
            auto bitset = reinterpret_cast<boost::dynamic_bitset<>*>(param);
            bitset->set(value, false);
            return true;
          },
          this->m_bitset);
      break;
    }
    default: {
      throw Generic_Exception("Unknown BitmapType " +
                              std::to_string(static_cast<int>(this->m_type)));
    }
  }

  this->m_bitset_initialized = true;
  return this->m_bitset;
}

// -------------------------------------------------------------------------------------
std::pair<u32, BitmapType> RoaringBitmap::compress(const BITMAP* bitmap,
                                                   u8* dest,
                                                   u32 tuple_count) {
  // Returns a pair of compressed size and the type of bitmap used

  // Determine the bitmap type by counting 1s
  u32 ones_count = 0;
  for (u32 i = 0; i < tuple_count; i++) {
    ones_count += bitmap[i];
  }

  if (ones_count == 0) {
    return {0, BitmapType::ALLZEROS};
  } else if (ones_count == tuple_count) {
    return {0, BitmapType::ALLONES};
  }

  BITMAP check_value;
  BitmapType type;
  if (ones_count < tuple_count / 2) {
    type = BitmapType::REGULAR;
    check_value = 1;
  } else {
    // There are more 1s than 0s in the Bitmap. In oder to save space and
    // computation time during decompression of the roaring bitmap we simply
    // flip the bits. (Less 1s => Smaller Roaring bitmap)
    type = BitmapType::FLIPPED;
    check_value = 0;
  }

  // Write the actual bitmap
  Roaring r;
  for (u32 row_i = 0; row_i < tuple_count; row_i++) {
    if (bitmap[row_i] == check_value) {
      r.add(row_i);
    }
  }
  r.runOptimize();
  r.setCopyOnWrite(true);
  u32 compressed_size = r.write(reinterpret_cast<char*>(dest), false);

  return {compressed_size, type};
}
// -------------------------------------------------------------------------------------
}  // namespace btrblocks::db::v2::bitmap
