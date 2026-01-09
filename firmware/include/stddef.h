/*
 * stddef.h - Use types from arch/cc.h
 */
#ifndef _STDDEF_H
#define _STDDEF_H

#include "arch/cc.h"

/* size_t, ptrdiff_t, NULL are defined in arch/cc.h */

/* offsetof macro */
#ifndef offsetof
#define offsetof(type, member) ((size_t)&((type *)0)->member)
#endif

#endif /* _STDDEF_H */
