#!/bin/bash
# Build every supported configuration and check they all behave identically.
#
# The differential tests are only meaningful if something actually runs them
# against every build. LUA_NANBOX in particular adds a second value
# representation in lobject.h, and nothing else in the tree compiles it -- so
# without this script it would rot silently within a few commits of anyone
# touching that header.
#
#   bash tests/run_checks.sh [path/to/stock/lua]
#
# With a stock Lua 5.1.5 binary the suites are compared against it byte for
# byte; without one, the configurations are compared against each other, which
# still catches a representation that disagrees with itself.
set -u
cd "$(dirname "$0")/.."
STOCK="${1:-}"
SUITES="tests/num2str_check.lua tests/bytecode_check.lua tests/format_check.lua tests/hook_check.lua tests/metatable_check.lua tests/table_check.lua tests/coroutine_check.lua tests/perf/app_json.lua tests/perf/str_keys.lua tests/perf/gen_method_calls.lua tests/perf/str_serialize.lua"
LIBS="-Wl,-E -ldl -lreadline -lhistory -lncurses"
WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
fail=0

build() { # name, extra cflags
    ( cd src && make -s clean >/dev/null 2>&1 \
      && make -s all MYCFLAGS="-DLUA_USE_LINUX $2" MYLIBS="$LIBS" >/dev/null 2>&1 ) \
      || { echo "FAIL: $1 does not build"; fail=1; return 1; }
    cp src/lua "$WORK/$1" || return 1
    echo "built: $1"
}

build classic "" || true
build nanbox  "-DLUA_NANBOX" || true
# LUA_NANBOX force-defines LUAI_NANCANON, so nanbox+canon is the same binary
# as nanbox. Build canonicalization on the classic representation instead --
# a configuration that genuinely differs and nothing else compiles.
build classic_canon "-DLUAI_NANCANON" || true

for suite in $SUITES; do
    ref=""; refname=""
    if [ -n "$STOCK" ] && [ -x "$STOCK" ]; then
        ref=$("$STOCK" "$suite" 2>&1); refname="stock"
    else
        ref=$("$WORK/classic" "$suite" 2>&1); refname="classic"
    fi
    for cfg in classic nanbox classic_canon; do
        [ -x "$WORK/$cfg" ] || continue
        out=$("$WORK/$cfg" "$suite" 2>&1)
        if [ "$out" != "$ref" ]; then
            echo "FAIL: $cfg disagrees with $refname on $suite"
            diff <(echo "$ref") <(echo "$out") | head -5
            fail=1
        fi
    done
done

# lua_sethook must work asynchronously: lua.c's SIGINT handler installs a hook
# while the VM is in a loop that makes no calls, and the VM must notice it on
# the loop's back-edge. One loop per jump flavour: JMP, a comparison's fused
# jump, FORLOOP.
for cfg in classic nanbox classic_canon; do
    [ -x "$WORK/$cfg" ] || continue
    for code in 'while true do end' 'repeat local x = 1 until x < 0' \
                'for i = 1, 1e300 do end'; do
        timeout --foreground -s INT -k 3 0.3 "$WORK/$cfg" -e "$code" 2>&1 | grep -q 'interrupted!' \
            || { echo "FAIL: $cfg cannot be interrupted in: $code"; fail=1; }
    done
done

# luac must keep emitting stock-compatible bytecode in every configuration
if [ -n "$STOCK" ] && [ -x "$(dirname "$STOCK")/luac" ]; then
    for f in tests/perf/gen_fib.lua tests/perf/app_json.lua; do
        "$(dirname "$STOCK")/luac" -o "$WORK/a.out" "$f" 2>/dev/null
        src/luac -o "$WORK/b.out" "$f" 2>/dev/null
        cmp -s "$WORK/a.out" "$WORK/b.out" || { echo "FAIL: bytecode differs from stock for $f"; fail=1; }
    done
fi

( cd src && make -s clean >/dev/null 2>&1 && make -s all MYCFLAGS=-DLUA_USE_LINUX MYLIBS="$LIBS" >/dev/null 2>&1 )

# Random-program differential against stock, which is where the hand-written
# suites stop. Short by default; run tests/fuzz_differential.py directly with a
# larger count before a release.
if [ -n "$STOCK" ] && [ -x "$STOCK" ] && command -v python3 >/dev/null; then
    python3 tests/fuzz_differential.py 1 60 --stock "$STOCK" | tail -1
    python3 tests/fuzz_differential.py 2 60 --stock "$STOCK" | tail -1 || fail=1
fi

if [ $fail -eq 0 ]; then
    echo "all configurations agree"
else
    echo "CHECKS FAILED"
fi
exit $fail
