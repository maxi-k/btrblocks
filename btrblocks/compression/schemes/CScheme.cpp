// -------------------------------------------------------------------------------------
#include "CScheme.hpp"
// -------------------------------------------------------------------------------------
#include "compression/cache/ThreadCache.hpp"
// -------------------------------------------------------------------------------------
DEFINE_uint32(sample_size, 64, "");
DEFINE_uint32(sample_count, 10, "");
// -------------------------------------------------------------------------------------
namespace btrblocks::db {
// -------------------------------------------------------------------------------------
double DoubleScheme::expectedCompressionRatio(DoubleStats& stats, u8 allowed_cascading_level) {
  auto dest = makeBytesArray(FLAGS_sample_size * FLAGS_sample_count * sizeof(DOUBLE) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(FLAGS_sample_count, FLAGS_sample_size);
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
  auto dest = makeBytesArray(FLAGS_sample_size * FLAGS_sample_count * sizeof(INTEGER) * 100);
  u32 total_before = 0;
  u32 total_after = 0;
  if (ThreadCache::get().estimation_level++ >= 1) {
    total_before += stats.total_size;
    total_after += compress(stats.src, stats.bitmap, dest.get(), stats, allowed_cascading_level);
  } else {
    auto sample = stats.samples(FLAGS_sample_count, FLAGS_sample_size);
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
string ConvertSchemeTypeToString(IntegerSchemeType type) {
  switch (type) {
    case IntegerSchemeType::PBP:
      return "PBP";
    case IntegerSchemeType::PBP_DELTA:
      return "PBP_DETA";
    case IntegerSchemeType::FBP:
      return "FBP";
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
// ------------------------------------------------------------------------------
string ConvertSchemeTypeToString(DoubleSchemeType type) {
  switch (type) {
    case DoubleSchemeType::DECIMAL:
      return "DECIMAL";
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
    case DoubleSchemeType::UNCOMPRESSED:
      return "UNCOMPRESSED";
    case DoubleSchemeType::FOR:
      return "FOR";
    default:
      throw Generic_Exception("Unknown IntegerSchemeType");
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
}  // namespace btrblocks::db
