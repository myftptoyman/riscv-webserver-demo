/*
 * inttypes.h - Minimal implementation for bare-metal
 */

#ifndef INTTYPES_H
#define INTTYPES_H

#include <stdint.h>

/* Format macros for printf */
#define PRIu8  "u"
#define PRIu16 "u"
#define PRIu32 "u"
#define PRIu64 "llu"

#define PRId8  "d"
#define PRId16 "d"
#define PRId32 "d"
#define PRId64 "lld"

#define PRIx8  "x"
#define PRIx16 "x"
#define PRIx32 "x"
#define PRIx64 "llx"

#define PRIX8  "X"
#define PRIX16 "X"
#define PRIX32 "X"
#define PRIX64 "llX"

#endif /* INTTYPES_H */
