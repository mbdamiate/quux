# Turns a text file into a C header with an unsigned char array (used for the core prelude).
#   cmake -DINPUT=... -DOUTPUT=... -DNAME=... -P embed_text.cmake
file(READ "${INPUT}" content HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," content "${content}")
file(WRITE "${OUTPUT}" "/* Generated from ${INPUT}; do not edit. */\nstatic const unsigned char ${NAME}[] = {${content}};\n")
