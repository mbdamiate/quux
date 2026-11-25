# Vendored libraries (D-37): compiled without the project's warnings; BLAKE3 in its portable
# variant only (no SIMD assembly), so the same code runs on every target.
add_library(quux_blake3 STATIC
  ${CMAKE_SOURCE_DIR}/third_party/blake3/blake3.c
  ${CMAKE_SOURCE_DIR}/third_party/blake3/blake3_dispatch.c
  ${CMAKE_SOURCE_DIR}/third_party/blake3/blake3_portable.c)
target_include_directories(quux_blake3 SYSTEM PUBLIC ${CMAKE_SOURCE_DIR}/third_party/blake3)
target_compile_definitions(quux_blake3 PRIVATE
  BLAKE3_NO_SSE2 BLAKE3_NO_SSE41 BLAKE3_NO_AVX2 BLAKE3_NO_AVX512 BLAKE3_USE_NEON=0)
target_compile_options(quux_blake3 PRIVATE -w)

add_library(quux_hashing STATIC quux_hashing.c)
target_include_directories(quux_hashing PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_include_directories(quux_hashing SYSTEM PRIVATE ${CMAKE_SOURCE_DIR}/third_party/xxhash)
target_link_libraries(quux_hashing PRIVATE quux_blake3)
