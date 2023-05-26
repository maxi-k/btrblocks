#pragma once
#include "storage/Relation.hpp"
#include "compression/Datablock.hpp"
// -------------------------------------------------------------------------------------
#include "gtest/gtest.h"
// -------------------------------------------------------------------------------------
using namespace btrblocks;
// -------------------------------------------------------------------------------------
class TestHelper {
public:
   static void CheckRelationCompression(Relation &relation, CMachine &compressor, const vector<u8> expected_compression_schemes = {});
};
// -------------------------------------------------------------------------------------
