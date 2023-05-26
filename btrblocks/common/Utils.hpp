#pragma once
// -------------------------------------------------------------------------------------
#include <cmath>
#include <cstring>
#include <fstream>
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
class Utils {
 public:
  static u64 alignBy(u64 num, u64 alignment, u64& diff) {
    u64 new_num = (num + alignment - 1) & ~(alignment - 1);
    diff = new_num - num;
    return new_num;
  };

  static u32 getBitsNeeded(u32 input) { return std::max(std::floor(std::log2(input)) + 1, 1.0); }
  static u32 getBitsNeeded(u64 input) { return std::max(std::floor(std::log2(input)) + 1, 1.0); }

  static u32 getBitsNeeded(s32 input) {
    if (input < 0) {
      return 32;
    }
    return std::max(std::floor(std::log2(input)) + 1, 1.0);
  }

  static void multiplyString(char* dest, const char* src, u32 len, u32 n, u32 src_n) {
    // IDEA:
    // Move this check out of here and only perform it when these cases can
    // actually occur
    if (n == 0 || len == 0) {
      return;
    }

    u32 n_written = std::min(n, src_n);
    std::memcpy(dest, src, len * n_written);
    char* write_ptr = dest + (len * n_written);

    while (n_written < n) {
      u32 write_n = std::min(n_written, n - n_written);
      std::size_t write_len = write_n * len;

      std::memcpy(write_ptr, dest, write_len);

      write_ptr += write_len;
      n_written += write_n;
    }
  }

  static void multiplyU32(u32* dest, const u32* src, u32 n) {
    multiplyString(reinterpret_cast<char*>(dest), reinterpret_cast<const char*>(src), sizeof(u32),
                   n, 1);
  }

  static u32* writeOffsetsU32(u32* dest, u32 start, u32 len, u32 n) {
    /*
     * Writes u32 bits offsets for string viewer. The first offset will be at
     * start and will increment by length. A total of at least n offsets will be
     * written.
     *
     * After the operation the memory should look like this
     * dest[0] = start
     * dest[1] = start + length
     * dest[2] = start + 2*length
     * ...
     * dest[n-1] = start + (n-1) * length
     *
     * WARNING: This may write up to 8 additional offsets past the given array.
     * Make sure to properly allocate space and to not overwrite other existing
     * data in that additional space.
     */
    static_assert(sizeof(u32) == 4);

    // First 8 writes sequentially unrolled
    dest[0] = start;
    dest[1] = start + len;
    dest[2] = start + 2 * len;
    dest[3] = start + 3 * len;
    dest[4] = start + 4 * len;
    dest[5] = start + 5 * len;
    dest[6] = start + 6 * len;
    dest[7] = start + 7 * len;

    if (n <= 8) {
      return dest + n;
    }

    // IDEA:
    // Could maybe improve performance further by
    // - moving the loadu_si256
    // - replacing the loadu_ and the first 8 elements by a write to a vector
    // register and a storu
#ifdef BTR_USE_SIMD
    auto write_ptr = dest + 8;
    auto* end = dest + n;
    const __m256i len_v = _mm256_set1_epi32(len * 8);
    __m256i current = _mm256_loadu_si256(reinterpret_cast<__m256i*>(dest));
    while (write_ptr < end) {
      current = _mm256_add_epi32(current, len_v);
      _mm256_storeu_si256(reinterpret_cast<__m256i*>(write_ptr), current);
      write_ptr += 8;
    }
    return end;
#else
    for (auto i = 8u; i != n; ++i) {
      // could change multiplication to addition addition, but adds data
      // dependency. let's trust the compiler to do the right thing.
      dest[i] = start + i * len;
    }
    return dest + n;
#endif
  }

  static void readFileToMemory(const std::string& path, std::vector<char>& target) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.good()) {
      auto msg = "Failed to open " + path;
      perror(msg.c_str());
      throw Generic_Exception(msg);
    }
    std::streamsize filesize = file.tellg();
    file.seekg(0, std::ios::beg);

    target.resize(filesize);
    file.read(target.data(), filesize);

    file.close();
    if (file.fail()) {
      auto msg = "Reading " + path + " failed";
      perror(msg.c_str());
      throw Generic_Exception(msg);
    }
  }
};
// -------------------------------------------------------------------------------------
}  // namespace btrblocks
// -------------------------------------------------------------------------------------
