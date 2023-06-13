// ---------------------------------------------------------------------------
// BtrBlocks
// ---------------------------------------------------------------------------
#include <iostream>
#include "gtest/gtest.h"
#include "scheme/SchemePool.hpp"
#include "common/SIMD.hpp"
// ---------------------------------------------------------------------------
using namespace btrblocks;
// ---------------------------------------------------------------------------
int main(int argc, char *argv[])
{
   #ifdef BTR_USE_SIMD
   std::cout << "\033[0;35mSIMD ENABLED\033[0m" << std::endl;
   #else
   std::cout << "\033[0;31mSIMD DISABLED\033[0m" << std::endl;
   #endif
   testing::InitGoogleTest(&argc, argv);
   // -------------------------------------------------------------------------------------
   SchemePool::available_schemes = make_unique<SchemesCollection>();
   return RUN_ALL_TESTS();
}
// ---------------------------------------------------------------------------
