/*
** $Id: lobject.c,v 2.22.1.1 2007/12/27 13:02:25 roberto Exp $
** Some generic functions over Lua objects
** See Copyright Notice in lua.h
*/

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lobject_c
#define LUA_CORE

#include "ldo.h"
#include "lmem.h"
#include "lobject.h"
#include "lstate.h"
#include "lstring.h"
#include "lua.h"
#include "lvm.h"

#if defined(LUA_NANBOX)
/* the nil word: tag LUA_TNIL, zero payload */
const TValue luaO_nilobject_ = {{NB_NILWORD}};
#else
const TValue luaO_nilobject_ = {{NULL}, LUA_TNIL};
#endif

/*
** converts an integer to a "floating point byte", represented as
** (eeeeexxx), where the real value is (1xxx) * 2^(eeeee - 1) if
** eeeee != 0 and (xxx) otherwise.
*/
int luaO_int2fb(unsigned int x) {
  int e = 0; /* expoent */
  while (x >= 16) {
    x = (x + 1) >> 1;
    e++;
  }
  if (x < 8) return x;
  return ((e + 1) << 3) | (cast_int(x) - 8);
}

/* converts back */
int luaO_fb2int(int x) {
  int e = (x >> 3) & 31;
  if (e == 0) return x;
  return ((x & 7) + 8) << (e - 1);
}

int luaO_log2(unsigned int x) {
  static const lu_byte log_2[256] = {
      0, 1, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5,
      5, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
      6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
      7, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,
      8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8};
  int l = -1;
  while (x >= 256) {
    l += 8;
    x >>= 8;
  }
  return l + log_2[x];
}

int luaO_rawequalObj(const TValue* t1, const TValue* t2) {
  if (ttype(t1) != ttype(t2)) return 0;
  switch (ttype(t1)) {
    case LUA_TNIL:
      return 1;
    case LUA_TNUMBER:
      return luai_numeq(nvalue(t1), nvalue(t2));
    case LUA_TBOOLEAN:
      return bvalue(t1) == bvalue(t2); /* boolean true must be 1 !! */
    case LUA_TLIGHTUSERDATA:
      return pvalue(t1) == pvalue(t2);
    default:
      lua_assert(iscollectable(t1));
      return gcvalue(t1) == gcvalue(t2);
  }
}

/*
** Format an integral lua_Number without going through sprintf(LUA_NUMBER_FMT).
** Writes a NUL-terminated result into `s` (which must hold at least
** LUAI_MAXNUMBER2STR bytes) and returns its length, or returns 0 if the value
** is not eligible and the caller must fall back to sprintf.
**
** Output is byte-identical to "%.14g". Two edges make that non-obvious:
** "%.14g" prints 14 significant digits, so it renders an integral value as
** plain digits only up to 99999999999999 -- at 1e14 (15 digits) it switches to
** "1e+14", so the bound below is deliberately strict. And negative zero prints
** as "-0", so the sign has to survive the zero case. Non-integral values, NaN
** and the infinities all fail the tests and fall back.
**
** See tests/num2str_check.lua, which verifies the equivalence against
** string.format("%.14g", n) over millions of values.
*/
#if defined(LUAI_NUM2STR_FASTPATH)

#define NUM2STR_MAX 99999999999999.0 /* largest integral value with <= 14 digits */

int luaO_num2str(char* s, lua_Number n) {
  char tmp[24];
  long long i;
  unsigned long long u;
  int p = 0, len = 0;
  /* Everything below assumes "%.14g". The preprocessor cannot compare
     strings, so check here; compilers fold this to a constant. */
  if (strcmp(LUA_NUMBER_FMT, "%.14g") != 0) return 0;
  if (!(n >= -NUM2STR_MAX && n <= NUM2STR_MAX)) return 0; /* also rejects NaN */
  i = (long long)n;
  if ((lua_Number)i != n) return 0; /* not integral */
  if (i == 0) {
    /* distinguish 0.0 from -0.0: 1/(-0.0) is negative infinity */
    if (n == 0 && 1 / n < 0) s[len++] = '-';
    s[len++] = '0';
    s[len] = '\0';
    return len;
  }
  if (i < 0) {
    s[len++] = '-';
    u = 0ULL - (unsigned long long)i; /* negate in unsigned: safe for LLONG_MIN */
  } else {
    u = (unsigned long long)i;
  }
  do {
    tmp[p++] = (char)('0' + (int)(u % 10));
    u /= 10;
  } while (u);
  while (p > 0) s[len++] = tmp[--p];
  s[len] = '\0';
  return len;
}

#else

int luaO_num2str(char* s, lua_Number n) {
  (void)s;
  (void)n;
  return 0;
}

#endif

int luaO_str2d(const char* s, lua_Number* result) {
  char* endptr;
  *result = lua_str2number(s, &endptr);
  if (endptr == s) return 0;            /* conversion failed */
  if (*endptr == 'x' || *endptr == 'X') /* maybe an hexadecimal constant? */
    *result = cast_num(strtoul(s, &endptr, 16));
  if (*endptr == '\0') return 1; /* most common case */
  while (isspace(cast(unsigned char, *endptr))) endptr++;
  return *endptr == '\0';
}

static void pushstr(lua_State* L, const char* str) {
  setsvalue2s(L, L->top, luaS_new(L, str));
  incr_top(L);
}

/* this function handles only `%d', `%c', %f, %p, and `%s' formats */
const char* luaO_pushvfstring(lua_State* L, const char* fmt, va_list argp) {
  int n = 1;
  pushstr(L, "");
  for (;;) {
    const char* e = strchr(fmt, '%');
    if (e == NULL) break;
    setsvalue2s(L, L->top, luaS_newlstr(L, fmt, e - fmt));
    incr_top(L);
    switch (*(e + 1)) {
      case 's': {
        const char* s = va_arg(argp, char*);
        if (s == NULL) s = "(null)";
        pushstr(L, s);
        break;
      }
      case 'c': {
        char buff[2];
        buff[0] = cast(char, va_arg(argp, int));
        buff[1] = '\0';
        pushstr(L, buff);
        break;
      }
      case 'd': {
        setnvalue(L->top, cast_num(va_arg(argp, int)));
        incr_top(L);
        break;
      }
      case 'f': {
        setnvalue(L->top, cast_num(va_arg(argp, l_uacNumber)));
        incr_top(L);
        break;
      }
      case 'p': {
        char
            buff[4 * sizeof(void*) + 8]; /* should be enough space for a `%p' */
        sprintf(buff, "%p", va_arg(argp, void*));
        pushstr(L, buff);
        break;
      }
      case '%': {
        pushstr(L, "%");
        break;
      }
      default: {
        char buff[3];
        buff[0] = '%';
        buff[1] = *(e + 1);
        buff[2] = '\0';
        pushstr(L, buff);
        break;
      }
    }
    n += 2;
    fmt = e + 2;
  }
  pushstr(L, fmt);
  luaV_concat(L, n + 1, cast_int(L->top - L->base) - 1);
  L->top -= n;
  return svalue(L->top - 1);
}

const char* luaO_pushfstring(lua_State* L, const char* fmt, ...) {
  const char* msg;
  va_list argp;
  va_start(argp, fmt);
  msg = luaO_pushvfstring(L, fmt, argp);
  va_end(argp);
  return msg;
}

void luaO_chunkid(char* out, const char* source, size_t bufflen) {
  if (*source == '=') {
    strncpy(out, source + 1, bufflen); /* remove first char */
    out[bufflen - 1] = '\0';           /* ensures null termination */
  } else {                             /* out = "source", or "...source" */
    if (*source == '@') {
      size_t l;
      source++; /* skip the `@' */
      bufflen -= sizeof(" '...' ");
      l = strlen(source);
      strcpy(out, "");
      if (l > bufflen) {
        source += (l - bufflen); /* get last part of file name */
        strcat(out, "...");
      }
      strcat(out, source);
    } else {                                /* out = [string "string"] */
      size_t len = strcspn(source, "\n\r"); /* stop at first newline */
      bufflen -= sizeof(" [string \"...\"] ");
      if (len > bufflen) len = bufflen;
      strcpy(out, "[string \"");
      if (source[len] != '\0') { /* must truncate? */
        strncat(out, source, len);
        strcat(out, "...");
      } else
        strcat(out, source);
      strcat(out, "\"]");
    }
  }
}
