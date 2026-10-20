#!/usr/bin/env python3
"""Differential fuzzer: this fork against stock Lua 5.1.5.

Generates random Lua programs, runs each under both interpreters, and compares
stdout+stderr byte for byte. Any difference is a bug in this fork -- stock is
the specification. Programs that differ are written to tests/.fuzzwork/.

    python3 tests/fuzz_differential.py <seed> [count] [--stock PATH]

Stock defaults to lua-5.1.5/src/lua under the repository root. Build both with
the same compiler and flags; a difference in error message wording between
compilers would otherwise read as a failure.

The generator leans on what this fork actually touched -- number and string
formatting, table lookup and chaining, metamethod fallbacks, coroutines, debug
hooks -- plus arithmetic on non-numbers, where coercion and metamethods meet.
"""
import os
import random
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OURS = os.path.join(ROOT, "src", "lua")
WORK = os.path.join(ROOT, "tests", ".fuzzwork")

argv = list(sys.argv[1:])
stock = os.path.join(ROOT, "lua-5.1.5", "src", "lua")
if "--stock" in argv:
    i = argv.index("--stock")
    stock = argv[i + 1]
    del argv[i:i + 2]
seed = int(argv[0]) if argv else 1
count = int(argv[1]) if len(argv) > 1 else 300
random.seed(seed)
os.makedirs(WORK, exist_ok=True)

HEADER = [
    'local R = {}',
    'local function o(...) local n=select("#",...) local t={} '
    'for i=1,n do t[i]=tostring((select(i,...))) end R[#R+1]=table.concat(t,"|") end',
    'local MT = setmetatable({}, {__add=function() return "A" end,'
    '__mul=function() return "M" end, __pow=function() return "P" end,'
    '__unm=function() return "U" end, __concat=function() return "C" end,'
    '__len=function() return 7 end, __index=function(_,k) return "I:"..tostring(k) end,'
    '__eq=function() return true end, __lt=function() return true end,'
    '__call=function() return "CALL" end})',
    'local S = "10"',
    'local BASE = {inherited = "B", shared = 1}',
]


def gen():
    L = list(HEADER)
    nv = 0

    def v():
        return "v%d" % random.randint(0, nv - 1) if nv else "1"

    def any_():
        return random.choice([v(), "S", "MT", str(random.randint(-3, 9)), '"x"', "nil"])

    for _ in range(random.randint(12, 30)):
        c = random.randint(0, 22)
        if c == 0:
            L.append("local v%d = %d" % (nv, random.randint(-5, 20))); nv += 1
        elif c == 1:
            L.append("local v%d = %s %s %s" % (nv, v(), random.choice("+-*"),
                                               random.choice([v(), str(random.randint(1, 4))]))); nv += 1
        elif c == 2:
            L.append("if %s > %d then o(%s) else o(-1) end" % (v(), random.randint(0, 5), v()))
        elif c == 3:
            L.append("for i=1,%d do o(%s+i) end" % (random.randint(1, 4), v()))
        elif c == 4:
            L.append("do local t={%s,%s,%s} o(t[2], #t) end" % (v(), v(), v()))
        elif c == 5:
            L.append("local function fn%d(a,b) return a*2-b end o(fn%d(%s,%s))" % (nv, nv, v(), v())); nv += 1
        elif c == 6:
            L.append("local a%d,b%d = (function() return %s,%s end)() o(a%d,b%d)" % (nv, nv, v(), v(), nv, nv)); nv += 1
        elif c == 7:
            L.append("local w=%s while w>0 do o(w) w=w-1 if w==2 then break end end" % v())
        elif c == 8:
            L.append("local function va%d(...) local p,q,r=... return (r or 0)+(q or 0)+(p or 0) end "
                     "o(va%d(%s,%s,%s))" % (nv, nv, v(), v(), v())); nv += 1
        elif c == 9:
            L.append('o("s" .. %s .. "e")' % v())
        elif c == 10:                                   # number formatting
            L.append('o(tostring(%s), string.format("%%d|%%.14g|%%s|%%5.2f", %s, %s, tostring(%s), %s))'
                     % (v(), v(), v(), v(), v()))
        elif c == 11:
            L.append("o(%s %% 3, %s ^ 2, -%s)" % (v(), v(), v()))
        elif c == 12:                                   # arithmetic on non-numbers
            L.append("o(pcall(function() return %s %s %s end))"
                     % (any_(), random.choice(["+", "*", "^", "-", "..", "<"]), any_()))
        elif c == 13:
            L.append("o(pcall(function() return -(%s) end), pcall(function() return #(%s) end))"
                     % (any_(), any_()))
        elif c == 14:                                   # string<->number coercion
            L.append('local q%d = %s * 1 o(type(q%d), q%d, tonumber("%s"), tonumber("0x1f"))'
                     % (nv, any_(), nv, nv, random.choice(["12", "3.5", "nan", "inf", "z"]))); nv += 1
        elif c == 15:                                   # table chaining under collisions
            L.append("local h%d={} for i=1,%d do h%d['k'..i]=i end "
                     "for i=1,%d,2 do h%d['k'..i]=nil end "
                     "local c=0 for k,x in pairs(h%d) do c=c+x end o(c)"
                     % (nv, random.randint(8, 60), nv, random.randint(8, 40), nv, nv)); nv += 1
        elif c == 16:                                   # __index fallback chains
            L.append("local m%d = setmetatable({own=%s}, {__index=BASE}) "
                     "o(m%d.own, m%d.inherited, m%d.missing)" % (nv, v(), nv, nv, nv)); nv += 1
        elif c == 17:                                   # method dispatch through SELF
            L.append("local C%d={} C%d.__index=C%d function C%d.new(x) return setmetatable({x=x},C%d) end "
                     "function C%d:get() return self.x end local i%d=C%d.new(%s) o(i%d:get())"
                     % (nv, nv, nv, nv, nv, nv, nv, nv, v(), nv)); nv += 1
        elif c == 18:                                   # coroutines
            L.append("local co%d = coroutine.create(function(a) local b = coroutine.yield(a*2) "
                     "return b, a end) o(coroutine.resume(co%d, %s)) o(coroutine.resume(co%d, %s)) "
                     "o(coroutine.status(co%d))" % (nv, nv, v(), nv, v(), nv)); nv += 1
        elif c == 19:                                   # coroutine errors and wrap
            L.append("local cw%d = coroutine.wrap(function() coroutine.yield(%s) error('x') end) "
                     "o(cw%d()) o(pcall(cw%d))" % (nv, v(), nv, nv)); nv += 1
        elif c == 20:                                   # debug hooks
            L.append("local hc%d=0 debug.sethook(function() hc%d=hc%d+1 end,'',%d) "
                     "local s=0 for i=1,%d do s=s+i end debug.sethook() o(s, hc%d>0)"
                     % (nv, nv, nv, random.randint(1, 20), random.randint(5, 60), nv)); nv += 1
        elif c == 21:                                   # numeric keys, incl. out-of-int-range
            L.append("local nk%d={} nk%d[%s]=1 nk%d[2^40]=2 nk%d[-2^40]=3 nk%d[1/0]=4 nk%d[2.5]=5 "
                     "o(nk%d[%s], nk%d[2^40], nk%d[1/0], nk%d[2.5])"
                     % (nv, nv, v(), nv, nv, nv, nv, nv, v(), nv, nv, nv)); nv += 1
        else:                                           # string library surface
            L.append('o(("%s"):rep(%d), ("abcdef"):sub(%d,%d), ("a,b,c"):gsub(",","-"), '
                     '("%%s"):format(tostring(%s)))'
                     % (random.choice("xyz"), random.randint(1, 4),
                        random.randint(1, 3), random.randint(3, 6), v()))
    L.append('print(table.concat(R, "\\n"))')
    return "\n".join(L)


def main():
    if not os.path.exists(stock):
        print("stock Lua not found at %s (pass --stock PATH)" % stock)
        return 1
    src = os.path.join(WORK, "f.lua")
    bad = 0
    for n in range(count):
        text = gen()
        open(src, "w").write(text)
        a = subprocess.run([stock, src], capture_output=True, timeout=60, cwd=ROOT)
        b = subprocess.run([OURS, src], capture_output=True, timeout=60, cwd=ROOT)
        # Each interpreter prefixes errors with its own argv[0]; that is the
        # binary's path, not a semantic difference, so normalize it away.
        ae, be = a.stderr.replace(stock.encode(), b"lua"), b.stderr.replace(OURS.encode(), b"lua")
        if a.stdout != b.stdout or ae != be:
            bad += 1
            path = os.path.join(WORK, "fail_%d_%d.lua" % (seed, n))
            open(path, "w").write(text)
            print("MISMATCH #%d -> %s" % (n, path))
            for label, x, y in (("stdout", a.stdout, b.stdout), ("stderr", ae, be)):
                if x != y:
                    print("  stock %s: %r" % (label, x[:200]))
                    print("  ours  %s: %r" % (label, y[:200]))
    print("seed %d: %d programs, %d mismatches" % (seed, count, bad))
    return 1 if bad else 0


sys.exit(main())
