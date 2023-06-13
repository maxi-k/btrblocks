//
// Created by david on 10.06.22.
//
// -------------------------------------------------------------------------------------
#include "common/Units.hpp"
#include "scheme/integer/PBP.hpp"
#include "scheme/CompressionScheme.hpp"
// -------------------------------------------------------------------------------------
using namespace btrblocks::units;
// -------------------------------------------------------------------------------------
template <typename T>
static void print_vec(std::vector<T> &v, const char *msg) {
    std::cerr << msg << ": ";
    for (const T &elem : v) {
        std::cerr << elem << " ";
    }
    std::cerr << std::endl;
}
// -------------------------------------------------------------------------------------
static bool test_compression(std::vector<INTEGER> &src, btrblocks::IntegerScheme &scheme) {
    std::vector<INTEGER> dst(src.size(), 0);
    size_t original_size = src.size() * sizeof(INTEGER);
    std::vector<u8> compressed(original_size * 10); // Size * 10 just to be sure

    auto src_ptr = src.data();
    auto compressed_ptr = reinterpret_cast<u8 *>(compressed.data());
    auto dst_ptr = dst.data();

    btrblocks::SInteger32Stats stats(src_ptr, nullptr, src.size());

    u32 compressed_size = scheme.compress(src_ptr, nullptr, compressed_ptr, stats, 0);
    std::cerr << "Compression done. Old size: " << original_size << " new size: " << compressed_size << std::endl;

    scheme.decompress(dst_ptr, nullptr, compressed_ptr, stats.tuple_count, 0);
    std::cerr << "Decompression done." << std::endl;
    print_vec(src, "src");
    print_vec(dst, "dst");

    bool ret = src == dst;

    // Zero out dst
    std::fill(dst.begin(), dst.end(), 0);

    return ret;
}
// -------------------------------------------------------------------------------------
int main(void) {
    std::vector<INTEGER> src {1, -1, 0, -10, 10, -300, -2147483648, 2147483647, -2, 128};
    bool result;

    btrblocks::integers::PBP pbp;
    result = test_compression(src, pbp);
    assert(result);

    btrblocks::integers::FBP fbp;
    result = test_compression(src, fbp);
    assert(result);
}
