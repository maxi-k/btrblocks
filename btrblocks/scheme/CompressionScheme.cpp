// -------------------------------------------------------------------------------------
#include "CompressionScheme.hpp"
#include "btrblocks.hpp"
#include "cache/ThreadCache.hpp"
// -------------------------------------------------------------------------------------
namespace btrblocks {
// -------------------------------------------------------------------------------------
double DoubleScheme::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(DOUBLE) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    DoubleStats c_stats = DoubleStats::generateStats(
        std::get<0>(sample).data(), std::get<1>(sample).data(), std::get<0>(sample).size());
    total_before += c_stats.total_size;
    total_after += compress(std::get<0>(sample).data(), std::get<1>(sample).data(), dest.get(),
                            c_stats, allowed_cascading_level);
  }
  ThreadCache::get().estimation_level--;
  return CD(total_before) / CD(total_after);
}
// -------------------------------------------------------------------------------------
double IntegerScheme::expectedCompressionRatio(SInteger32Stats& stats, u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(INTEGER) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    SInteger32Stats c_stats = SInteger32Stats::generateStats(
        std::get<0>(sample).data(), std::get<1>(sample).data(), std::get<0>(sample).size());
    total_before += c_stats.total_size;
    total_after += compress(std::get<0>(sample).data(), std::get<1>(sample).data(), dest.get(),
                            c_stats, allowed_cascading_level);
  }
  ThreadCache::get().estimation_level--;
  return CD(total_before) / CD(total_after);
}
// -------------------------------------------------------------------------------------
double Int64Scheme::expectedCompressionRatio(SInt64Stats& stats, u8 allowed_cascading_level) {
  auto& cfg = BtrBlocksConfig::get();
  auto dest = makeBytesArray(CS(cfg.sample_size) * cfg.sample_count * sizeof(INT64) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(cfg.sample_count, cfg.sample_size);
    SInt64Stats c_stats = SInt64Stats::generateStats(
        std::get<0>(sample).data(), std::get<1>(sample).data(), std::get<0>(sample).size());
    total_before += c_stats.total_size;
    total_after += compress(std::get<0>(sample).data(), std::get<1>(sample).data(), dest.get(),
                            c_stats, allowed_cascading_level);
  }
  ThreadCache::get().estimation_level--;
  return CD(total_before) / CD(total_after);
}
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(IntegerSchemeType type) {
  switch (type) {
    case IntegerSchemeType::PFOR:
      return "PFOR";
    case IntegerSchemeType::PFOR_DELTA:
      return "PFOR_DETA";
    case IntegerSchemeType::BP:
      return "BP";
    case IntegerSchemeType::RLE:
      return "RLE";
    case IntegerSchemeType::DICT:
      return "DICT";
    case IntegerSchemeType::FREQUENCY:
      return "FREQUENCY";
    case IntegerSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case IntegerSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case IntegerSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case IntegerSchemeType::TRUNCATION_8:
      return "TRUNCATION_8";
    case IntegerSchemeType::TRUNCATION_16:
      return "TRUNCATION_16";
    case IntegerSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case IntegerSchemeType::FOR:
      return "FOR";
    default:
      throw Generic_Exception("Unknown IntegerSchemeType");
  }
}
// -------------------------------------------------------------------------------------
string ConvertSchemeTypeToString(Int64SchemeType type) {
  switch (type) {
    case Int64SchemeType::PFOR:
      return "PFOR";
    case Int64SchemeType::BP:
      return "BP";
    case Int64SchemeType::RLE:
      return "RLE";
    case Int64SchemeType::DICT:
      return "DICT";
    case Int64SchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case Int64SchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    default:
      throw Generic_Exception("Unknown Int64SchemeType");
  }
}
// ------------------------------------------------------------------------------
string ConvertSchemeTypeToString(DoubleSchemeType type) {
  switch (type) {
    case DoubleSchemeType::PSEUDODECIMAL:
      return "PSEUDODECIMAL";
    case DoubleSchemeType::ALP_RD:
      return "ALP_RD";
    case DoubleSchemeType::RLE:
      return "RLE";
    case DoubleSchemeType::DICT:
      return "DICT";
    case DoubleSchemeType::FREQUENCY:
      return "FREQUENCY";
    case DoubleSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case DoubleSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case DoubleSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case DoubleSchemeType::DOUBLE_BP:
      return "DOUBLE_BP";
    case DoubleSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    default:
      throw Generic_Exception("Unknown DoubleSchemeType");
  }
}
// ------------------------------------------------------------------------------
string ConvertSchemeTypeToString(StringSchemeType type) {
  switch (type) {
    case StringSchemeType::ONE_VALUE:
      return "ONE_VALUE";
    case StringSchemeType::DICTIONARY_8:
      return "DICTIONARY_8";
    case StringSchemeType::DICTIONARY_16:
      return "DICTIONARY_16";
    case StringSchemeType::DICT:
      return "S_DICT";
    case StringSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case StringSchemeType::FSST:
      return "FSST";
    default:
      throw Generic_Exception("Unknown StringSchemeType");
  }
}
// ------------------------------------------------------------------------------
}  // namespace btrblocks
