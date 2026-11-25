#include "quux_hashing.h"

#include "blake3.h"

#define XXH_INLINE_ALL
#include "xxhash.h"

_Static_assert(sizeof(blake3_hasher) <= sizeof(quux_content_hasher),
               "quux_content_hasher storage too small for blake3_hasher");

uint64_t quux_hashing_value(const void *bytes, size_t length, uint64_t seed) {
    return XXH3_64bits_withSeed(bytes, length, seed);
}

uint64_t quux_hashing_combine(uint64_t accumulated, uint64_t next) {
    uint64_t pair[2] = {accumulated, next};
    return XXH3_64bits(pair, sizeof pair);
}

void quux_hashing_content_begin(quux_content_hasher *hasher) {
    blake3_hasher_init((blake3_hasher *)(void *)hasher->state);
}

void quux_hashing_content_update(quux_content_hasher *hasher, const void *bytes, size_t length) {
    blake3_hasher_update((blake3_hasher *)(void *)hasher->state, bytes, length);
}

void quux_hashing_content_finish(quux_content_hasher *hasher,
                                 unsigned char digest[QUUX_CONTENT_HASH_BYTES]) {
    blake3_hasher_finalize((blake3_hasher *)(void *)hasher->state, digest, QUUX_CONTENT_HASH_BYTES);
}
