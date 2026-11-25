/* Reuse: keep. The two hashes of D-03, behind one interface so the Phase 2 spike (H10) can
 * replace them (D-37): a fast 64-bit value hash (XXH3, fixed seed) and a 256-bit content hash
 * (BLAKE3, portable C). */
#ifndef QUUX_HASHING_H
#define QUUX_HASHING_H

#include <stddef.h>
#include <stdint.h>

enum { QUUX_CONTENT_HASH_BYTES = 32 };

/* Value hash of raw bytes. The seed separates kinds (a string and a symbol with the same text
 * hash differently). Fixed across runs and machines (D-37). */
uint64_t quux_hashing_value(const void *bytes, size_t length, uint64_t seed);
/* Order-dependent combination of two value hashes. */
uint64_t quux_hashing_combine(uint64_t accumulated, uint64_t next);

/* Streaming content hash. */
typedef struct quux_content_hasher {
    _Alignas(16) unsigned char state[2048];
} quux_content_hasher;

void quux_hashing_content_begin(quux_content_hasher *hasher);
void quux_hashing_content_update(quux_content_hasher *hasher, const void *bytes, size_t length);
void quux_hashing_content_finish(quux_content_hasher *hasher,
                                 unsigned char digest[QUUX_CONTENT_HASH_BYTES]);

#endif
