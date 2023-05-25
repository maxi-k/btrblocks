// ------------------------------------------------------------------------------
// BtrBlocks - Generic C interface
// ------------------------------------------------------------------------------
#ifndef BTRBLOCKS_H_
#define BTRBLOCKS_H_

#include <stddef.h>
#include "btrblocks/config.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

size_t btrblocks_compress_ints(int* input, size_t length);
size_t btrblocks_compress_doubles(double* input, size_t length);
size_t btrblocks_compress_strings(char* input, size_t length);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BTRBLOCKS_H_
