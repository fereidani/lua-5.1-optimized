/*
** onelua.c -- the Lua core and standard libraries as one translation unit.
**
** The interpreter's hot paths cross file boundaries all the time: table
** lookups in ltable.c, string interning in lstring.c, the library fast paths
** in lstrlib.c. Compiled as separate objects, those are calls the compiler
** cannot inline; compiled together, they are not. This needs no compiler
** support (unlike -flto) and works with every compiler and platform target.
**
** lua.c and luac.c stay separate: neither is on a hot path, and luac needs
** the internal functions to remain linkable, which is why luaall_c (which
** makes them static) is not used here.
*/

/* stdio.h before everything else: ldebug.h defines a getline macro that
   would otherwise collide with the POSIX getline declaration */
#include <stdio.h>

/* luaconf.h defines lua_tmpnam only for loslib.c; it is included once, by the
   first file below, so the gate has to be open from the start */
#define loslib_c

/* core */
#include "lapi.c"
#include "lcode.c"
#include "ldebug.c"
#include "ldo.c"
#include "ldump.c"
#include "lfunc.c"
#include "lgc.c"
#include "llex.c"
#include "lmem.c"
#include "lobject.c"
#include "lopcodes.c"
#include "lparser.c"
#include "lstate.c"
#include "lstring.c"
#include "ltable.c"
#include "ltm.c"
#include "lundump.c"
#include "lvm.c"
#include "lzio.c"

/* standard libraries */
#include "lauxlib.c"
#include "lbaselib.c"
#include "ldblib.c"
#include "liolib.c"
#include "lmathlib.c"
#include "loslib.c"
#include "ltablib.c"
#include "lstrlib.c"
#include "loadlib.c"
#include "linit.c"
