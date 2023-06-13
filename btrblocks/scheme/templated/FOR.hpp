// #pragma once
// #include "scheme/CompressionScheme.hpp"
// #include "compression/SchemePicker.hpp"
////
///-------------------------------------------------------------------------------------
////
///-------------------------------------------------------------------------------------
////
///-------------------------------------------------------------------------------------
// namespace btrblocks {
// namespace db {
// namespace v2 {
// template<typename NumberType>
// struct FORStructure {
//    NumberType bias;
//    u8 next_scheme;
//    u8 data[];
// };
////
///-------------------------------------------------------------------------------------
// template<typename NumberType, typename SchemeType, typename StatsType,
// typename SchemeCodeType> class TFOR { public:
//    static inline double expectedCompressionRatio(StatsType &stats)
//    {
//       return 0;
//    }
//    //
//    -------------------------------------------------------------------------------------
//    static inline u32 compressColumn(const NumberType *src, const BITMAP *, u8
//    *dest, StatsType &stats, u8 allowed_cascading_level)
//    {
//       auto &col_struct = *reinterpret_cast<FORStructure *> (dest);
//       vector<NumberType> biased_output;
//       //
//       -------------------------------------------------------------------------------------
//       col_struct.bias = stats.min;
//       for ( u32 row_i = 0; row_i < stats.tuple_count; row_i++ ) {
//          if ( nullmap == nullptr || nullmap[row_i] ) {
//             biased_output.push_back(src[row_i] - col_struct.bias);
//          } else {
//             biased_output.push_back(src[row_i]);
//          }
//       }
//       //
//       -------------------------------------------------------------------------------------
//       // Next Level compression
//       auto write_ptr = col_struct.data;
//       u32 used_space;
//       IntegerSchemePicker::compress(biased_output.data(), nullmap, write_ptr,
//       biased_output.size(), allowed_cascading_level - 1, used_space,
//       col_struct.next_scheme); write_ptr += used_space;
//       //
//       -------------------------------------------------------------------------------------
//       return write_ptr - dest;
//    }
//    //
//    -------------------------------------------------------------------------------------
//    static inline
//    void decompressColumn(NumberType *dest, BITMAP *, const u8 *src, u32
//    tuple_count)
//    {
//
//    }
// }
////
///-------------------------------------------------------------------------------------
//};
//}
//}
//}