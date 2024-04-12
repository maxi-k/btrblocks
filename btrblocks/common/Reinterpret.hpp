#pragma once
#include "Units.hpp"
// -------------------------------------------------------------------------------------
#define RD(num) *reinterpret_cast<const DOUBLE*>(&(num))
#define RU64(num) *reinterpret_cast<const u64*>(&(num))
// -------------------------------------------------------------------------------------
