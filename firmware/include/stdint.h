/*
 * stdint.h - Use types from arch/cc.h
 */
#ifndef _STDINT_H
#define _STDINT_H

#include "arch/cc.h"

/* Types are defined in arch/cc.h */

/* Integer limits */
#define INT8_MIN   (-128)
#define INT8_MAX   127
#define UINT8_MAX  255

#define INT16_MIN  (-32768)
#define INT16_MAX  32767
#define UINT16_MAX 65535

#define INT32_MIN  (-2147483647-1)
#define INT32_MAX  2147483647
#define UINT32_MAX 4294967295U

#define INT64_MIN  (-9223372036854775807L-1)
#define INT64_MAX  9223372036854775807L
#define UINT64_MAX 18446744073709551615UL

#define SIZE_MAX   UINT64_MAX
#define INTPTR_MIN INT64_MIN
#define INTPTR_MAX INT64_MAX
#define UINTPTR_MAX UINT64_MAX

#endif /* _STDINT_H */
