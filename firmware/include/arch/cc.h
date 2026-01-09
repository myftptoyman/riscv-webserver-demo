#ifndef ARCH_CC_H
#define ARCH_CC_H

/* Tell lwIP to NOT include system headers */
#define LWIP_NO_STDDEF_H    1
#define LWIP_NO_STDINT_H    1
#define LWIP_NO_INTTYPES_H  1
#define LWIP_NO_LIMITS_H    1
#define LWIP_NO_CTYPE_H     1
#define LWIP_NO_UNISTD_H    1

/* Provide our own type definitions - RV64 LP64 ABI:
 * char: 8 bits, short: 16 bits, int: 32 bits, long: 64 bits */
typedef unsigned char       uint8_t;
typedef signed char         int8_t;
typedef unsigned short      uint16_t;
typedef signed short        int16_t;
typedef unsigned int        uint32_t;
typedef signed int          int32_t;
typedef unsigned long       uint64_t;  /* long is 64 bits on LP64 */
typedef signed long         int64_t;
typedef unsigned long       uintptr_t;
typedef long                intptr_t;
typedef long                ptrdiff_t;
typedef unsigned long       size_t;
typedef long                ssize_t;

/* lwIP types */
typedef uint8_t     u8_t;
typedef int8_t      s8_t;
typedef uint16_t    u16_t;
typedef int16_t     s16_t;
typedef uint32_t    u32_t;
typedef int32_t     s32_t;
typedef uint64_t    u64_t;
typedef int64_t     s64_t;
typedef uintptr_t   mem_ptr_t;

#define LWIP_HAVE_INT64 1

/* System protection type (for critical sections) */
typedef int sys_prot_t;

/* printf format specifiers */
#define X8_F   "02x"
#define U16_F  "u"
#define S16_F  "d"
#define X16_F  "x"
#define U32_F  "u"
#define S32_F  "d"
#define X32_F  "x"
#define SZT_F  "lu"

/* Limits */
#define INT_MAX     2147483647
#define UINT_MAX    4294967295U
#define SSIZE_MAX   INT_MAX

/* NULL */
#ifndef NULL
#define NULL ((void*)0)
#endif

/* Compiler hints */
#define LWIP_UNUSED_ARG(x)  (void)(x)

/* Byte order - RISC-V is little endian */
#ifndef BYTE_ORDER
#define BYTE_ORDER LITTLE_ENDIAN
#endif

/* Structure packing */
#define PACK_STRUCT_FIELD(x) x
#define PACK_STRUCT_STRUCT __attribute__((packed))
#define PACK_STRUCT_BEGIN
#define PACK_STRUCT_END

/* Diagnostic output - must be defined before arch.h defaults kick in */
extern void console_printf(const char *fmt, ...);
#define LWIP_PLATFORM_DIAG(x)   do { console_printf x; } while(0)
#define LWIP_PLATFORM_ASSERT(x) do { console_printf("ASSERT FAIL: %s\n", x); while(1); } while(0)

/* Random number generator (simple LFSR) */
static inline u32_t lwip_rand(void) {
    static u32_t seed = 0x12345678;
    seed = seed * 1103515245 + 12345;
    return seed;
}
#define LWIP_RAND() lwip_rand()

/* Memory functions */
extern void *malloc(size_t size);
extern void free(void *ptr);
extern void *calloc(size_t nmemb, size_t size);

#define mem_clib_malloc malloc
#define mem_clib_free free
#define mem_clib_calloc calloc

/* Don't include errno header - use LWIP_PROVIDE_ERRNO=0 in lwipopts.h */

#endif /* ARCH_CC_H */
