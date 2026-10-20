/*
** $Id: llimits.h,v 1.69.1.1 2007/12/27 13:02:25 roberto Exp $
** Limits, basic types, and some other `installation-dependent' definitions
** See Copyright Notice in lua.h
*/

#ifndef llimits_h
#define llimits_h

#include <limits.h>
#include <stddef.h>
#include <string.h>

#include "lua.h"

typedef LUAI_UINT32 lu_int32;

typedef LUAI_UMEM lu_mem;

typedef LUAI_MEM l_mem;

/* chars used as small naturals (so that `char' is reserved for characters) */
typedef unsigned char lu_byte;

#define MAX_SIZET ((size_t)(~(size_t)0) - 2)

#define MAX_LUMEM ((lu_mem)(~(lu_mem)0) - 2)

#define MAX_INT (INT_MAX - 2) /* maximum value of an int (-2 for safety) */

/*
** conversion of pointer to integer
** this is for hashing only; there is no problem if the integer
** cannot hold the whole pointer value
*/
#define IntPoint(p) ((unsigned int)(lu_mem)(p))

/* type to ensure maximum alignment */
typedef LUAI_USER_ALIGNMENT_T L_Umaxalign;

/* result of a `usual argument conversion' over lua_Number */
typedef LUAI_UACNUMBER l_uacNumber;

/* internal assertions for in-house debugging */
#ifdef lua_assert

#define check_exp(c, e) (lua_assert(c), (e))
#define api_check(l, e) lua_assert(e)

#else

#define lua_assert(c) ((void)0)
#define check_exp(c, e) (e)
#define api_check luai_apicheck

#endif

#ifndef UNUSED
#define UNUSED(x) ((void)(x)) /* to avoid warnings */
#endif

#ifndef cast
#define cast(t, exp) ((t)(exp))
#endif

#define cast_byte(i) cast(lu_byte, (i))
#define cast_num(i) cast(lua_Number, (i))
#define cast_int(i) cast(int, (i))

/*
** type for virtual-machine instructions
** must be an unsigned with (at least) 4 bytes (see details in lopcodes.h)
*/
typedef lu_int32 Instruction;

/* maximum stack for a Lua function */
#define MAXSTACK 250

/* minimum size for the string table (must be power of 2) */
#ifndef MINSTRTABSIZE
#define MINSTRTABSIZE 32
#endif

/* minimum size for string buffer */
#ifndef LUA_MINBUFFER
#define LUA_MINBUFFER 32
#endif

#ifndef lua_lock
#define lua_lock(L) ((void)0)
#define lua_unlock(L) ((void)0)
#endif

#ifndef luai_threadyield
#define luai_threadyield(L) \
  {                         \
    lua_unlock(L);          \
    lua_lock(L);            \
  }
#endif

/*
** macro to control inclusion of some hard tests on stack reallocation
*/
#ifndef HARDSTACKTESTS
#define condhardstacktests(x) ((void)0)
#else
#define condhardstacktests(x) x
#endif

/*
** Branch prediction hints. Compilers that support __builtin_expect (GCC,
** Clang, and other GCC-compatible frontends) get real hints; on compilers
** without it (e.g. MSVC) they degrade to no-ops. Define luai_likely before
** including this header to override.
*/
/* 64-bit unsigned, for bit-level work on lua_Number */
#if defined(LLONG_MAX) || defined(__GNUC__) || defined(_MSC_VER)
typedef unsigned long long lu_int64;
#else
typedef unsigned long lu_int64;
#endif

#define LUAI_NANSIGN  0x8000000000000000ULL
#define LUAI_NANQUIET 0x7FF8000000000000ULL

/* portable "static inline" (C89 has no inline) */
#if defined(__cplusplus) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define l_inline static inline
#elif defined(__GNUC__)
#define l_inline static __inline__
#elif defined(_MSC_VER)
#define l_inline static __inline
#else
#define l_inline static
#endif

/*
** Canonicalize a NaN, preserving its sign.
**
** Groundwork for NaN-boxing: there the tag lives in the high bits of the
** word, so a NaN carrying an arbitrary payload would be read back as a tagged
** pointer. That is reachable from pure Lua -- tonumber("nan(0x7ffffffffffff)")
** yields 0x7FFFFFFFFFFFFFFF -- which would make it a type-confusion primitive
** from sandboxed script, so every number entering a TValue must be normalized.
**
** The sign bit is kept because it is observable: glibc prints a NaN as "nan"
** or "-nan" purely by sign, so stock Lua shows tostring(0/0) as "-nan" (x86
** produces a negative NaN) and tostring(tonumber("nan")) as "nan". Collapsing
** to a single NaN would change one of those. Keeping the sign leaves the two
** canonical forms 0x7FF8000000000000 and 0xFFF8000000000000, and a tag range
** of 0x1FFF1..0x1FFFF in the top 17 bits still clears both -- 15 tags for the
** 12 a TValue needs.
**
** The test is on every number store, but the branch is never taken in real
** code and the bit work happens only on the NaN path.
*/
#if defined(LUAI_NANCANON)
#define luai_canonnan(r, x)                                       \
  {                                                               \
    lua_Number c_x = (x);                                         \
    if (luai_unlikely(c_x != c_x)) {                              \
      lu_int64 c_u;                                               \
      memcpy(&c_u, &c_x, sizeof(c_u));                            \
      c_u = (c_u & LUAI_NANSIGN) | LUAI_NANQUIET;                 \
      memcpy(&c_x, &c_u, sizeof(c_x));                            \
    }                                                             \
    (r) = c_x;                                                    \
  }
#else
#define luai_canonnan(r, x) ((r) = (x))
#endif

#if !defined(luai_likely)
#if defined(__GNUC__) || defined(__clang__)
#define luai_likely(x) (__builtin_expect(!!(x), 1))
#define luai_unlikely(x) (__builtin_expect(!!(x), 0))
#else
#define luai_likely(x) (x)
#define luai_unlikely(x) (x)
#endif
#endif

#endif
