/*
** $Id: lvm.c,v 2.63.1.5 2011/08/17 20:43:11 roberto Exp $
** Lua virtual machine
** See Copyright Notice in lua.h
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define lvm_c
#define LUA_CORE

#include "ldebug.h"
#include "ldo.h"
#include "lfunc.h"
#include "lgc.h"
#include "lobject.h"
#include "lopcodes.h"
#include "lstate.h"
#include "lstring.h"
#include "ltable.h"
#include "ltm.h"
#include "lua.h"
#include "lvm.h"

/* limit for table tag-method chains (to avoid loops) */
#define MAXTAGLOOP 100

const TValue* luaV_tonumber(const TValue* obj, TValue* n) {
  lua_Number num;
  if (ttisnumber(obj)) return obj;
  if (ttisstring(obj) && luaO_str2d(svalue(obj), &num)) {
    setnvalue(n, num);
    return n;
  } else
    return NULL;
}

int luaV_tostring(lua_State* L, StkId obj) {
  if (!ttisnumber(obj))
    return 0;
  else {
    char s[LUAI_MAXNUMBER2STR];
    lua_Number n = nvalue(obj);
    lua_number2str(s, n);
    setsvalue2s(L, obj, luaS_new(L, s));
    return 1;
  }
}

static void traceexec(lua_State* L, const Instruction* pc) {
  lu_byte mask = L->hookmask;
  const Instruction* oldpc = L->savedpc;
  L->savedpc = pc;
  if ((mask & LUA_MASKCOUNT) && L->hookcount == 0) {
    resethookcount(L);
    luaD_callhook(L, LUA_HOOKCOUNT, -1);
  }
  if (mask & LUA_MASKLINE) {
    Proto* p = ci_func(L->ci)->l.p;
    int npc = pcRel(pc, p);
    int newline = getline(p, npc);
    /* call linehook when enter a new function, when jump back (loop),
       or when enter a new line */
    if (npc == 0 || pc <= oldpc || newline != getline(p, pcRel(oldpc, p)))
      luaD_callhook(L, LUA_HOOKLINE, newline);
  }
}

static void callTMres(lua_State* L, StkId res, const TValue* f,
                      const TValue* p1, const TValue* p2) {
  ptrdiff_t result = savestack(L, res);
  setobj2s(L, L->top, f);      /* push function */
  setobj2s(L, L->top + 1, p1); /* 1st argument */
  setobj2s(L, L->top + 2, p2); /* 2nd argument */
  luaD_checkstack(L, 3);
  L->top += 3;
  luaD_call(L, L->top - 3, 1);
  res = restorestack(L, result);
  L->top--;
  setobjs2s(L, res, L->top);
}

static void callTM(lua_State* L, const TValue* f, const TValue* p1,
                   const TValue* p2, const TValue* p3) {
  setobj2s(L, L->top, f);      /* push function */
  setobj2s(L, L->top + 1, p1); /* 1st argument */
  setobj2s(L, L->top + 2, p2); /* 2nd argument */
  setobj2s(L, L->top + 3, p3); /* 3th argument */
  luaD_checkstack(L, 4);
  L->top += 4;
  luaD_call(L, L->top - 4, 0);
}

void luaV_gettable(lua_State* L, const TValue* t, TValue* key, StkId val) {
  int loop;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue* tm;
    if (ttistable(t)) { /* `t' is a table? */
      Table* h = hvalue(t);
      const TValue* res = luaH_get(h, key); /* do a primitive get */
      if (!ttisnil(res) ||                  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) { /* or no TM? */
        setobj2s(L, val, res);
        return;
      }
      /* else will try the tag method */
    } else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
      luaG_typeerror(L, t, "index");
    if (ttisfunction(tm)) {
      callTMres(L, val, tm, t, key);
      return;
    }
    t = tm; /* else repeat with `tm' */
  }
  luaG_runerror(L, "loop in gettable");
}

/*
** Continue a table read whose primitive lookup has already been done by the
** caller and came back nil, with `tm` the __index metamethod that lookup
** found. This is the second half of luaV_gettable, split out so the VM's fast
** path does not have to repeat work it has already done.
**
** It matters more than it looks. `obj:method()` misses on the instance and
** then finds the method through __index, so a miss is the normal case, not the
** exceptional one: 33% of all string lookups in tests/perf/gen_method_calls
** are misses. Falling back into luaV_gettable made every one of those walk the
** instance's chain a second time before ever consulting __index.
**
** The caller already did luaV_gettable's first primitive lookup, so each
** iteration here starts with the __index check that ends the previous one.
** Hence loop counts lookups done (starting at 1), and the check runs before
** the bound: luaV_gettable still calls an __index function found on its
** MAXTAGLOOP-th table, so we must too.
*/
void luaV_finishget(lua_State* L, const TValue* t, TValue* key, StkId val,
                    const TValue* tm) {
  int loop;
  for (loop = 1;; loop++) {
    if (ttisfunction(tm)) {
      callTMres(L, val, tm, t, key);
      return;
    }
    if (loop == MAXTAGLOOP) break;
    t = tm; /* else repeat with `tm' */
    if (ttistable(t)) {
      Table* h = hvalue(t);
      const TValue* res = luaH_get(h, key); /* do a primitive get */
      if (!ttisnil(res) ||                  /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL) { /* or no TM? */
        setobj2s(L, val, res);
        return;
      }
      /* else will try the tag method */
    } else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_INDEX)))
      luaG_typeerror(L, t, "index");
  }
  luaG_runerror(L, "loop in gettable");
}

void luaV_settable(lua_State* L, const TValue* t, TValue* key, StkId val) {
  int loop;
  TValue temp;
  for (loop = 0; loop < MAXTAGLOOP; loop++) {
    const TValue* tm;
    if (ttistable(t)) { /* `t' is a table? */
      Table* h = hvalue(t);
      TValue* oldval = luaH_set(L, h, key); /* do a primitive set */
      if (!ttisnil(oldval) ||               /* result is no nil? */
          (tm = fasttm(L, h->metatable, TM_NEWINDEX)) == NULL) { /* or no TM? */
        setobj2t(L, oldval, val);
        h->flags = 0;
        luaC_barriert(L, h, val);
        return;
      }
      /* else will try the tag method */
    } else if (ttisnil(tm = luaT_gettmbyobj(L, t, TM_NEWINDEX)))
      luaG_typeerror(L, t, "index");
    if (ttisfunction(tm)) {
      callTM(L, tm, t, key, val);
      return;
    }
    /* else repeat with `tm' */
    setobj(L, &temp, tm); /* avoid pointing inside table (may rehash) */
    t = &temp;
  }
  luaG_runerror(L, "loop in settable");
}

static int call_binTM(lua_State* L, const TValue* p1, const TValue* p2,
                      StkId res, TMS event) {
  const TValue* tm = luaT_gettmbyobj(L, p1, event);    /* try first operand */
  if (ttisnil(tm)) tm = luaT_gettmbyobj(L, p2, event); /* try second operand */
  if (ttisnil(tm)) return 0;
  callTMres(L, res, tm, p1, p2);
  return 1;
}

static const TValue* get_compTM(lua_State* L, Table* mt1, Table* mt2,
                                TMS event) {
  const TValue* tm1 = fasttm(L, mt1, event);
  const TValue* tm2;
  if (tm1 == NULL) return NULL; /* no metamethod */
  if (mt1 == mt2) return tm1;   /* same metatables => same metamethods */
  tm2 = fasttm(L, mt2, event);
  if (tm2 == NULL) return NULL;   /* no metamethod */
  if (luaO_rawequalObj(tm1, tm2)) /* same metamethods? */
    return tm1;
  return NULL;
}

static int call_orderTM(lua_State* L, const TValue* p1, const TValue* p2,
                        TMS event) {
  const TValue* tm1 = luaT_gettmbyobj(L, p1, event);
  const TValue* tm2;
  if (ttisnil(tm1)) return -1; /* no metamethod? */
  tm2 = luaT_gettmbyobj(L, p2, event);
  if (!luaO_rawequalObj(tm1, tm2)) /* different metamethods? */
    return -1;
  callTMres(L, L->top, tm1, p1, p2);
  return !l_isfalse(L->top);
}

static int l_strcmp(const TString* ls, const TString* rs) {
  const char* l = getstr(ls);
  size_t ll = ls->tsv.len;
  const char* r = getstr(rs);
  size_t lr = rs->tsv.len;
  for (;;) {
    int temp = strcoll(l, r);
    if (temp != 0)
      return temp;
    else {                    /* strings are equal up to a `\0' */
      size_t len = strlen(l); /* index of first `\0' in both strings */
      if (len == lr)          /* r is finished? */
        return (len == ll) ? 0 : 1;
      else if (len == ll) /* l is finished? */
        return -1;        /* l is smaller than r (because r is not finished) */
      /* both strings longer than `len'; go on comparing (after the `\0') */
      len++;
      l += len;
      ll -= len;
      r += len;
      lr -= len;
    }
  }
}

int luaV_lessthan(lua_State* L, const TValue* l, const TValue* r) {
  int res;
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttisnumber(l))
    return luai_numlt(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) < 0;
  else if ((res = call_orderTM(L, l, r, TM_LT)) != -1)
    return res;
  return luaG_ordererror(L, l, r);
}

static int lessequal(lua_State* L, const TValue* l, const TValue* r) {
  int res;
  if (ttype(l) != ttype(r))
    return luaG_ordererror(L, l, r);
  else if (ttisnumber(l))
    return luai_numle(nvalue(l), nvalue(r));
  else if (ttisstring(l))
    return l_strcmp(rawtsvalue(l), rawtsvalue(r)) <= 0;
  else if ((res = call_orderTM(L, l, r, TM_LE)) != -1) /* first try `le' */
    return res;
  else if ((res = call_orderTM(L, r, l, TM_LT)) != -1) /* else try `lt' */
    return !res;
  return luaG_ordererror(L, l, r);
}

int luaV_equalval(lua_State* L, const TValue* t1, const TValue* t2) {
  const TValue* tm;
  lua_assert(ttype(t1) == ttype(t2));
  switch (ttype(t1)) {
    case LUA_TNIL:
      return 1;
    case LUA_TNUMBER:
      return luai_numeq(nvalue(t1), nvalue(t2));
    case LUA_TBOOLEAN:
      return bvalue(t1) == bvalue(t2); /* true must be 1 !! */
    case LUA_TLIGHTUSERDATA:
      return pvalue(t1) == pvalue(t2);
    case LUA_TUSERDATA: {
      if (uvalue(t1) == uvalue(t2)) return 1;
      tm = get_compTM(L, uvalue(t1)->metatable, uvalue(t2)->metatable, TM_EQ);
      break; /* will try TM */
    }
    case LUA_TTABLE: {
      if (hvalue(t1) == hvalue(t2)) return 1;
      tm = get_compTM(L, hvalue(t1)->metatable, hvalue(t2)->metatable, TM_EQ);
      break; /* will try TM */
    }
    default:
      return gcvalue(t1) == gcvalue(t2);
  }
  if (tm == NULL) return 0;         /* no TM? */
  callTMres(L, L->top, tm, t1, t2); /* call TM */
  return !l_isfalse(L->top);
}

void luaV_concat(lua_State* L, int total, int last) {
  do {
    StkId top = L->base + last + 1;
    int n = 2; /* number of elements handled in this pass (at least 2) */
    if (!(ttisstring(top - 2) || ttisnumber(top - 2)) ||
        !tostring(L, top - 1)) {
      if (!call_binTM(L, top - 2, top - 1, top - 2, TM_CONCAT))
        luaG_concaterror(L, top - 2, top - 1);
    } else if (tsvalue(top - 1)->len == 0) /* second op is empty? */
      (void)tostring(L, top - 2);          /* result is first op (as string) */
    else {
      /* at least two string values; get as many as possible */
      size_t tl = tsvalue(top - 1)->len;
      char* buffer;
      int i;
      /* collect total length */
      for (n = 1; n < total && tostring(L, top - n - 1); n++) {
        size_t l = tsvalue(top - n - 1)->len;
        if (l >= MAX_SIZET - tl) luaG_runerror(L, "string length overflow");
        tl += l;
      }
      buffer = luaZ_openspace(L, &G(L)->buff, tl);
      tl = 0;
      for (i = n; i > 0; i--) { /* concat all strings */
        size_t l = tsvalue(top - i)->len;
        memcpy(buffer + tl, svalue(top - i), l);
        tl += l;
      }
      setsvalue2s(L, top - n, luaS_newlstr(L, buffer, tl));
    }
    total -= n - 1; /* got `n' strings to create 1 new */
    last -= n - 1;
  } while (total > 1); /* repeat until only 1 result left */
}

static void Arith(lua_State* L, StkId ra, const TValue* rb, const TValue* rc,
                  TMS op) {
  TValue tempb, tempc;
  const TValue *b, *c;
  if ((b = luaV_tonumber(rb, &tempb)) != NULL &&
      (c = luaV_tonumber(rc, &tempc)) != NULL) {
    lua_Number nb = nvalue(b), nc = nvalue(c);
    switch (op) {
      case TM_ADD:
        setnvalue(ra, luai_numadd(nb, nc));
        break;
      case TM_SUB:
        setnvalue(ra, luai_numsub(nb, nc));
        break;
      case TM_MUL:
        setnvalue(ra, luai_nummul(nb, nc));
        break;
      case TM_DIV:
        setnvalue(ra, luai_numdiv(nb, nc));
        break;
      case TM_MOD:
        setnvalue(ra, luai_nummod(nb, nc));
        break;
      case TM_POW:
        setnvalue(ra, luai_numpow(nb, nc));
        break;
      case TM_UNM:
        setnvalue(ra, luai_numunm(nb));
        break;
      default:
        lua_assert(0);
        break;
    }
  } else if (!call_binTM(L, rb, rc, ra, op))
    luaG_aritherror(L, rb, rc);
}

/*
** some macros for common tasks in `luaV_execute'
*/

/* Skips the instruction when the check fails. Must be vmbreak, not break:
   with threaded dispatch there is no switch, and a bare break would leave the
   interpreter loop and return from luaV_execute without unwinding the call. */
#define runtime_check(L, c) \
  {                         \
    if (!(c)) vmbreak;      \
  }

#define RA(i) (base + GETARG_A(i))
/* to be used after possible stack reallocation */
#define RB(i) check_exp(getBMode(GET_OPCODE(i)) == OpArgR, base + GETARG_B(i))
#define RC(i) check_exp(getCMode(GET_OPCODE(i)) == OpArgR, base + GETARG_C(i))
#define RKB(i)                                 \
  check_exp(getBMode(GET_OPCODE(i)) == OpArgK, \
            ISK(GETARG_B(i)) ? k + INDEXK(GETARG_B(i)) : base + GETARG_B(i))
#define RKC(i)                                 \
  check_exp(getCMode(GET_OPCODE(i)) == OpArgK, \
            ISK(GETARG_C(i)) ? k + INDEXK(GETARG_C(i)) : base + GETARG_C(i))
#define KBx(i) check_exp(getBMode(GET_OPCODE(i)) == OpArgK, k + GETARG_Bx(i))

#define dojump(L, pc, i) \
  {                      \
    (pc) += (i);         \
    luai_threadyield(L); \
  }

#define Protect(x)   \
  {                  \
    L->savedpc = pc; \
    {                \
      x;             \
    };               \
    base = L->base;  \
  }

#define arith_op(op, tm)                                  \
  {                                                       \
    TValue* rb = RKB(i);                                  \
    TValue* rc = RKC(i);                                  \
    if (luai_likely(ttisnumber(rb) && ttisnumber(rc))) {  \
      lua_Number nb = nvalue(rb), nc = nvalue(rc);        \
      setnvalue(ra, op(nb, nc));                          \
    } else                                                \
      Protect(Arith(L, ra, rb, rc, tm));                  \
  }

/*
** Resolve a table lookup without going through luaH_get's type switch.
**
** luaH_get switches on the key type and then calls the specialized accessor;
** from the VM the two shapes that matter are known cheaply and dominate real
** code: t.field, where the key is a string constant, and t[i] with an integer
** in the array part. The string case dispatches straight to luaH_getstr, and
** the integer case inlines luaH_getnum's array probe so an array read needs no
** call at all. Everything else falls back to luaH_get unchanged.
**
** Sets `res` to the value, which is luaO_nilobject when the key is absent --
** the caller still has to consult __index for that case.
*/
#define luaV_fastget(h, key, res)                                        \
  {                                                                      \
    const TValue* fk_ = (key);                                           \
    if (luai_likely(ttisstring(fk_)))                                    \
      (res) = luaH_getstr(h, rawtsvalue(fk_));                           \
    else if (ttisnumber(fk_)) {                                          \
      int fi_;                                                           \
      if (luaO_num2int(nvalue(fk_), &fi_))                               \
        (res) = (cast(unsigned int, fi_) - 1u <                          \
                 cast(unsigned int, (h)->sizearray))                     \
                    ? &(h)->array[fi_ - 1]                               \
                    : luaH_getnum(h, fi_);                               \
      else                                                               \
        (res) = luaH_get(h, fk_);                                        \
    } else                                                               \
      (res) = luaH_get(h, fk_);                                          \
  }

/*
** Instruction dispatch.
**
** With GCC/Clang we use computed goto ("labels as values") for threaded
** dispatch: each opcode gets its own branch site at its own dispatch point,
** so the branch predictor tracks every opcode's successor independently
** instead of one shared branch for the whole switch. This is the single
** biggest interpreter speedup available without touching opcodes (~10-25%).
** Define LUA_NO_THREADED_VM to force the plain switch (e.g. on compilers
** without the extension). Behaviour is identical either way.
*/
/*
** GCC only, deliberately. Clang defines __GNUC__ too, but measurably does
** worse with computed goto here than with the plain switch -- it merges the
** per-opcode indirect branches back together, which removes exactly the
** per-opcode branch prediction the threading exists to buy, and costs
** register pressure on top. Measured on clang 21, ours-with-goto against
** ours-with-switch: gen_loop_math +20%, gen_table_ops +11.8%, gen_sieve
** +11.7%, gen_field_access +7.6%, str_keys +6.2%, app_sha2 +2.4%,
** app_json +1.3%; only gen_fib (-2.6%) and gen_method_calls (-0.4%) favoured
** goto, both marginally. GCC shows the opposite, which is why the threading
** is kept rather than removed.
**
** Define LUA_USE_THREADED_VM to force it on, or LUA_NO_THREADED_VM to force
** it off, if your compiler and workload disagree with these measurements.
*/
#if defined(__GNUC__) && !defined(__clang__) && !defined(LUA_NO_THREADED_VM)
#define LUA_USE_THREADED_VM
#endif

/* Fetch the next instruction, run line/count hooks, and set `ra`. Shared by
   the loop prologue and (in threaded mode) by every opcode's tail. */
#define vmfetch()                                                              \
  do {                                                                         \
    i = *pc++;                                                                 \
    if (luai_unlikely((L->hookmask & (LUA_MASKLINE | LUA_MASKCOUNT)) &&        \
                      (--L->hookcount == 0 || L->hookmask & LUA_MASKLINE))) {  \
      traceexec(L, pc);                                                        \
      if (L->status == LUA_YIELD) {                                            \
        L->savedpc = pc - 1;                                                   \
        return;                                                                \
      }                                                                        \
      base = L->base;                                                          \
    }                                                                          \
    ra = RA(i);                                                                \
  } while (0)

#if defined(LUA_USE_THREADED_VM)
/* GET_OPCODE yields 0..63 (SIZE_OP bits), but only NUM_OPCODES entries exist.
   Lua 5.1's undump does not validate opcodes, so a corrupt or hostile
   precompiled chunk can produce an out-of-range value; the plain switch simply
   fell through, whereas an unguarded `goto *` would branch through whatever
   follows the table. Route anything out of range to the same no-op behaviour
   the switch had. */
#define vmdispatch(x)                                     \
  do {                                                    \
    unsigned int op_ = cast(unsigned int, (x));           \
    if (luai_unlikely(op_ >= cast(unsigned int, NUM_OPCODES))) goto L_vmnop; \
    goto* vmexec[op_];                                    \
  } while (0)
#define vmcase(op) L_##op:                 /* opcode label                 */
#define vmbreak do { vmfetch(); vmdispatch(GET_OPCODE(i)); } while (0)
#define vmswitchend /* threaded dispatch needs no enclosing brace */
#else
#define vmdispatch(x) switch (x) {
#define vmcase(op) case op:
#define vmbreak continue
#define vmswitchend }
#endif

void luaV_execute(lua_State* L, int nexeccalls) {
  LClosure* cl;
  StkId base;
  TValue* k;
  const Instruction* pc;
reentry: /* entry point */
  lua_assert(isLua(L->ci));
  pc = L->savedpc;
  cl = &clvalue(L->ci->func)->l;
  base = L->base;
  k = cl->p->k;
  /* main loop of interpreter */
#if defined(LUA_USE_THREADED_VM)
  /* dispatch table: opcode -> handler label (computed-goto threaded code) */
  static const void* const vmexec[NUM_OPCODES] = {
      [OP_MOVE] = &&L_OP_MOVE,         [OP_LOADK] = &&L_OP_LOADK,
      [OP_LOADBOOL] = &&L_OP_LOADBOOL, [OP_LOADNIL] = &&L_OP_LOADNIL,
      [OP_GETUPVAL] = &&L_OP_GETUPVAL, [OP_GETGLOBAL] = &&L_OP_GETGLOBAL,
      [OP_GETTABLE] = &&L_OP_GETTABLE, [OP_SETGLOBAL] = &&L_OP_SETGLOBAL,
      [OP_SETUPVAL] = &&L_OP_SETUPVAL, [OP_SETTABLE] = &&L_OP_SETTABLE,
      [OP_NEWTABLE] = &&L_OP_NEWTABLE, [OP_SELF] = &&L_OP_SELF,
      [OP_ADD] = &&L_OP_ADD,           [OP_SUB] = &&L_OP_SUB,
      [OP_MUL] = &&L_OP_MUL,           [OP_DIV] = &&L_OP_DIV,
      [OP_MOD] = &&L_OP_MOD,           [OP_POW] = &&L_OP_POW,
      [OP_UNM] = &&L_OP_UNM,           [OP_NOT] = &&L_OP_NOT,
      [OP_LEN] = &&L_OP_LEN,           [OP_CONCAT] = &&L_OP_CONCAT,
      [OP_JMP] = &&L_OP_JMP,           [OP_EQ] = &&L_OP_EQ,
      [OP_LT] = &&L_OP_LT,             [OP_LE] = &&L_OP_LE,
      [OP_TEST] = &&L_OP_TEST,         [OP_TESTSET] = &&L_OP_TESTSET,
      [OP_CALL] = &&L_OP_CALL,         [OP_TAILCALL] = &&L_OP_TAILCALL,
      [OP_RETURN] = &&L_OP_RETURN,     [OP_FORLOOP] = &&L_OP_FORLOOP,
      [OP_FORPREP] = &&L_OP_FORPREP,   [OP_TFORLOOP] = &&L_OP_TFORLOOP,
      [OP_SETLIST] = &&L_OP_SETLIST,   [OP_CLOSE] = &&L_OP_CLOSE,
      [OP_CLOSURE] = &&L_OP_CLOSURE,   [OP_VARARG] = &&L_OP_VARARG,
  };
#endif
  for (;;) {
    Instruction i;
    StkId ra;
    vmfetch();
    /* warning!! several calls may realloc the stack and invalidate `ra' */
    lua_assert(base == L->base && L->base == L->ci->base);
    lua_assert(base <= L->top && L->top <= L->stack + L->stacksize);
    lua_assert(L->top == L->ci->top || luaG_checkopenop(i));
    vmdispatch(GET_OPCODE(i));
      vmcase(OP_MOVE) {
        setobjs2s(L, ra, RB(i));
        vmbreak;
      }
      vmcase(OP_LOADK) {
        setobj2s(L, ra, KBx(i));
        vmbreak;
      }
      vmcase(OP_LOADBOOL) {
        setbvalue(ra, GETARG_B(i));
        if (GETARG_C(i)) pc++; /* skip next instruction (if C) */
        vmbreak;
      }
      vmcase(OP_LOADNIL) {
        TValue* rb = RB(i);
        do {
          setnilvalue(rb--);
        } while (rb >= ra);
        vmbreak;
      }
      vmcase(OP_GETUPVAL) {
        int b = GETARG_B(i);
        setobj2s(L, ra, cl->upvals[b]->v);
        vmbreak;
      }
      vmcase(OP_GETGLOBAL) {
        TValue g;
        TValue* rb = KBx(i);
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(rb));
        Protect(luaV_gettable(L, &g, rb, ra));
        vmbreak;
      }
      vmcase(OP_GETTABLE) {
        const TValue* rb = RB(i);
        if (luai_likely(ttistable(rb))) {
          Table* h = hvalue(rb);
          const TValue* res;
          const TValue* tm;
          luaV_fastget(h, RKC(i), res);
          /* fast path: table, result present (or no __index to consult) */
          if (luai_likely(!ttisnil(res) ||
                          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL)) {
            setobj2s(L, ra, res);
            vmbreak;
          }
          /* absent, but there is an __index: continue from there rather than
             letting luaV_gettable repeat the lookup we just did */
          Protect(luaV_finishget(L, rb, RKC(i), ra, tm));
          vmbreak;
        }
        Protect(luaV_gettable(L, rb, RKC(i), ra));
        vmbreak;
      }
      vmcase(OP_SETGLOBAL) {
        TValue g;
        sethvalue(L, &g, cl->env);
        lua_assert(ttisstring(KBx(i)));
        Protect(luaV_settable(L, &g, KBx(i), ra));
        vmbreak;
      }
      vmcase(OP_SETUPVAL) {
        UpVal* uv = cl->upvals[GETARG_B(i)];
        setobj(L, uv->v, ra);
        luaC_barrier(L, uv, ra);
        vmbreak;
      }
      vmcase(OP_SETTABLE) {
        if (luai_likely(ttistable(ra))) {
          Table* h = hvalue(ra);
          const TValue* slot;
          luaV_fastget(h, RKB(i), slot);
          /* fast path: table, key already present, no __newindex. Avoids the
             rehash/insert path and the luaV_settable call. */
          if (luai_likely(!ttisnil(slot) &&
                          fasttm(L, h->metatable, TM_NEWINDEX) == NULL)) {
            setobj2t(L, cast(TValue*, slot), RKC(i));
            h->flags = 0;
            luaC_barriert(L, h, RKC(i));
            vmbreak;
          }
        }
        Protect(luaV_settable(L, ra, RKB(i), RKC(i)));
        vmbreak;
      }
      vmcase(OP_NEWTABLE) {
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        sethvalue(L, ra, luaH_new(L, luaO_fb2int(b), luaO_fb2int(c)));
        Protect(luaC_checkGC(L));
        vmbreak;
      }
      vmcase(OP_SELF) {
        StkId rb = RB(i);
        setobjs2s(L, ra + 1, rb);
        if (luai_likely(ttistable(rb))) {
          Table* h = hvalue(rb);
          const TValue* res;
          const TValue* tm;
          luaV_fastget(h, RKC(i), res);
          if (luai_likely(!ttisnil(res) ||
                          (tm = fasttm(L, h->metatable, TM_INDEX)) == NULL)) {
            setobj2s(L, ra, res);
            vmbreak;
          }
          /* method lookup missing on the receiver is the normal case for
             obj:method(); go straight to __index */
          Protect(luaV_finishget(L, rb, RKC(i), ra, tm));
          vmbreak;
        }
        Protect(luaV_gettable(L, rb, RKC(i), ra));
        vmbreak;
      }
      vmcase(OP_ADD) {
        arith_op(luai_numadd, TM_ADD);
        vmbreak;
      }
      vmcase(OP_SUB) {
        arith_op(luai_numsub, TM_SUB);
        vmbreak;
      }
      vmcase(OP_MUL) {
        arith_op(luai_nummul, TM_MUL);
        vmbreak;
      }
      vmcase(OP_DIV) {
        arith_op(luai_numdiv, TM_DIV);
        vmbreak;
      }
      vmcase(OP_MOD) {
        arith_op(luai_nummod, TM_MOD);
        vmbreak;
      }
      vmcase(OP_POW) {
        arith_op(luai_numpow, TM_POW);
        vmbreak;
      }
      vmcase(OP_UNM) {
        TValue* rb = RB(i);
        if (luai_likely(ttisnumber(rb))) {
          lua_Number nb = nvalue(rb);
          setnvalue(ra, luai_numunm(nb));
        } else {
          Protect(Arith(L, ra, rb, rb, TM_UNM));
        }
        vmbreak;
      }
      vmcase(OP_NOT) {
        int res = l_isfalse(RB(i)); /* next assignment may change this value */
        setbvalue(ra, res);
        vmbreak;
      }
      vmcase(OP_LEN) {
        const TValue* rb = RB(i);
        switch (ttype(rb)) {
          case LUA_TTABLE: {
            setnvalue(ra, cast_num(luaH_getn(hvalue(rb))));
            break;
          }
          case LUA_TSTRING: {
            setnvalue(ra, cast_num(tsvalue(rb)->len));
            break;
          }
          default: { /* try metamethod */
            Protect(if (!call_binTM(L, rb, luaO_nilobject, ra, TM_LEN))
                        luaG_typeerror(L, rb, "get length of");)
          }
        }
        vmbreak;
      }
      vmcase(OP_CONCAT) {
        int b = GETARG_B(i);
        int c = GETARG_C(i);
        Protect(luaV_concat(L, c - b + 1, c); luaC_checkGC(L));
        setobjs2s(L, RA(i), base + b);
        vmbreak;
      }
#if defined(LUA_USE_THREADED_VM)
    L_vmnop: /* out-of-range opcode: ignore, exactly as the switch used to */
      vmbreak;
#endif
      vmcase(OP_JMP) {
        dojump(L, pc, GETARG_sBx(i));
        vmbreak;
      }
      vmcase(OP_EQ) {
        TValue* rb = RKB(i);
        TValue* rc = RKC(i);
        Protect(if (equalobj(L, rb, rc) == GETARG_A(i))
                    dojump(L, pc, GETARG_sBx(*pc));) pc++;
        vmbreak;
      }
      vmcase(OP_LT) {
        Protect(if (luaV_lessthan(L, RKB(i), RKC(i)) == GETARG_A(i))
                    dojump(L, pc, GETARG_sBx(*pc));) pc++;
        vmbreak;
      }
      vmcase(OP_LE) {
        Protect(if (lessequal(L, RKB(i), RKC(i)) == GETARG_A(i))
                    dojump(L, pc, GETARG_sBx(*pc));) pc++;
        vmbreak;
      }
      vmcase(OP_TEST) {
        if (l_isfalse(ra) != GETARG_C(i)) dojump(L, pc, GETARG_sBx(*pc));
        pc++;
        vmbreak;
      }
      vmcase(OP_TESTSET) {
        TValue* rb = RB(i);
        if (l_isfalse(rb) != GETARG_C(i)) {
          setobjs2s(L, ra, rb);
          dojump(L, pc, GETARG_sBx(*pc));
        }
        pc++;
        vmbreak;
      }
      vmcase(OP_CALL) {
        int b = GETARG_B(i);
        int nresults = GETARG_C(i) - 1;
        if (b != 0) L->top = ra + b; /* else previous instruction set top */
        L->savedpc = pc;
        switch (luaD_precall(L, ra, nresults)) {
          case PCRLUA: {
            nexeccalls++;
            goto reentry; /* restart luaV_execute over new Lua function */
          }
          case PCRC: {
            /* it was a C function (`precall' called it); adjust results */
            if (nresults >= 0) L->top = L->ci->top;
            base = L->base;
            vmbreak;
          }
          default: {
            return; /* yield */
          }
        }
      }
      vmcase(OP_TAILCALL) {
        int b = GETARG_B(i);
        if (b != 0) L->top = ra + b; /* else previous instruction set top */
        L->savedpc = pc;
        lua_assert(GETARG_C(i) - 1 == LUA_MULTRET);
        switch (luaD_precall(L, ra, LUA_MULTRET)) {
          case PCRLUA: {
            /* tail call: put new frame in place of previous one */
            CallInfo* ci = L->ci - 1; /* previous frame */
            int aux;
            StkId func = ci->func;
            StkId pfunc = (ci + 1)->func; /* previous function index */
            if (L->openupval) luaF_close(L, ci->base);
            L->base = ci->base = ci->func + ((ci + 1)->base - pfunc);
            for (aux = 0; pfunc + aux < L->top; aux++) /* move frame down */
              setobjs2s(L, func + aux, pfunc + aux);
            ci->top = L->top = func + aux; /* correct top */
            lua_assert(L->top == L->base + clvalue(func)->l.p->maxstacksize);
            ci->savedpc = L->savedpc;
            ci->tailcalls++; /* one more call lost */
            L->ci--;         /* remove new frame */
            goto reentry;
          }
          case PCRC: { /* it was a C function (`precall' called it) */
            base = L->base;
            vmbreak;
          }
          default: {
            return; /* yield */
          }
        }
      }
      vmcase(OP_RETURN) {
        int b = GETARG_B(i);
        if (b != 0) L->top = ra + b - 1;
        if (L->openupval) luaF_close(L, base);
        L->savedpc = pc;
        b = luaD_poscall(L, ra);
        if (--nexeccalls == 0) /* was previous function running `here'? */
          return;              /* no: return */
        else {                 /* yes: continue its execution */
          if (b) L->top = L->ci->top;
          lua_assert(isLua(L->ci));
          lua_assert(GET_OPCODE(*((L->ci)->savedpc - 1)) == OP_CALL);
          goto reentry;
        }
      }
      vmcase(OP_FORLOOP) {
        lua_Number step = nvalue(ra + 2);
        lua_Number idx = luai_numadd(nvalue(ra), step); /* increment index */
        lua_Number limit = nvalue(ra + 1);
        if (luai_numlt(0, step) ? luai_numle(idx, limit)
                                : luai_numle(limit, idx)) {
          dojump(L, pc, GETARG_sBx(i)); /* jump back */
          setnvalue(ra, idx);           /* update internal index... */
          setnvalue(ra + 3, idx);       /* ...and external index */
        }
        vmbreak;
      }
      vmcase(OP_FORPREP) {
        const TValue* init = ra;
        const TValue* plimit = ra + 1;
        const TValue* pstep = ra + 2;
        L->savedpc = pc; /* next steps may throw errors */
        if (!tonumber(init, ra))
          luaG_runerror(L, LUA_QL("for") " initial value must be a number");
        else if (!tonumber(plimit, ra + 1))
          luaG_runerror(L, LUA_QL("for") " limit must be a number");
        else if (!tonumber(pstep, ra + 2))
          luaG_runerror(L, LUA_QL("for") " step must be a number");
        setnvalue(ra, luai_numsub(nvalue(ra), nvalue(pstep)));
        dojump(L, pc, GETARG_sBx(i));
        vmbreak;
      }
      vmcase(OP_TFORLOOP) {
        StkId cb = ra + 3; /* call base */
        setobjs2s(L, cb + 2, ra + 2);
        setobjs2s(L, cb + 1, ra + 1);
        setobjs2s(L, cb, ra);
        L->top = cb + 3; /* func. + 2 args (state and index) */
        Protect(luaD_call(L, cb, GETARG_C(i)));
        L->top = L->ci->top;
        cb = RA(i) + 3;             /* previous call may change the stack */
        if (!ttisnil(cb)) {         /* continue loop? */
          setobjs2s(L, cb - 1, cb); /* save control variable */
          dojump(L, pc, GETARG_sBx(*pc)); /* jump back */
        }
        pc++;
        vmbreak;
      }
      vmcase(OP_SETLIST) {
        int n = GETARG_B(i);
        int c = GETARG_C(i);
        int last;
        Table* h;
        if (n == 0) {
          n = cast_int(L->top - ra) - 1;
          L->top = L->ci->top;
        }
        if (c == 0) c = cast_int(*pc++);
        runtime_check(L, ttistable(ra));
        h = hvalue(ra);
        last = ((c - 1) * LFIELDS_PER_FLUSH) + n;
        if (last > h->sizearray)        /* needs more space? */
          luaH_resizearray(L, h, last); /* pre-alloc it at once */
        for (; n > 0; n--) {
          TValue* val = ra + n;
          setobj2t(L, luaH_setnum(L, h, last--), val);
          luaC_barriert(L, h, val);
        }
        vmbreak;
      }
      vmcase(OP_CLOSE) {
        luaF_close(L, ra);
        vmbreak;
      }
      vmcase(OP_CLOSURE) {
        Proto* p;
        Closure* ncl;
        int nup, j;
        p = cl->p->p[GETARG_Bx(i)];
        nup = p->nups;
        ncl = luaF_newLclosure(L, nup, cl->env);
        ncl->l.p = p;
        for (j = 0; j < nup; j++, pc++) {
          if (GET_OPCODE(*pc) == OP_GETUPVAL)
            ncl->l.upvals[j] = cl->upvals[GETARG_B(*pc)];
          else {
            lua_assert(GET_OPCODE(*pc) == OP_MOVE);
            ncl->l.upvals[j] = luaF_findupval(L, base + GETARG_B(*pc));
          }
        }
        setclvalue(L, ra, ncl);
        Protect(luaC_checkGC(L));
        vmbreak;
      }
      vmcase(OP_VARARG) {
        int b = GETARG_B(i) - 1;
        int j;
        CallInfo* ci = L->ci;
        int n = cast_int(ci->base - ci->func) - cl->p->numparams - 1;
        if (b == LUA_MULTRET) {
          Protect(luaD_checkstack(L, n));
          ra = RA(i); /* previous call may change the stack */
          b = n;
          L->top = ra + n;
        }
        for (j = 0; j < b; j++) {
          if (j < n) {
            setobjs2s(L, ra + j, ci->base - n + j);
          } else {
            setnilvalue(ra + j);
          }
        }
        vmbreak;
      }
    vmswitchend
  }
}
