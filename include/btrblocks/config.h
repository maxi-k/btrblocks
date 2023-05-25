#ifndef BTRBLOCKS_CONFIG_H_
#define BTRBLOCKS_CONFIG_H_
#include <stddef.h>

#ifdef _cplusplus
extern "C" {
#endif // _cplusplus

struct BtrBlocksConfig {
    size_t block_size; // maximum number of tuples in a block
    unsigned cascade_depth; // maximum number of recursive compression calls
};

BtrBlocksConfig* btrblocks_config();

#ifdef _cplusplus
}
#endif // _cplusplus
#endif // BTRBLOCKS_CONFIG_H_
