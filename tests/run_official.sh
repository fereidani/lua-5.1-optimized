#!/bin/bash
# Run the official Lua 5.1 test suite against this fork, with stock Lua 5.1.5
# as a control.
#
#   bash tests/run_official.sh [path/to/stock/tree]
#
# The control is not optional. The suite is from 2006 and parts of it do not
# pass on unmodified Lua 5.1.5 on a modern 64-bit Linux box, so a bare failure
# list says nothing about this fork. Running both and diffing does.
#
# Two adjustments, both applied to stock and to this fork equally:
#
#   * Built with -DLUA_USE_POSIX rather than -DLUA_USE_LINUX, i.e. without
#     readline. main.lua compares interactive-mode output byte for byte, and
#     readline echoes its input, so that test fails on stock too when readline
#     is linked in.
#   * big.lua is skipped. It concatenates 129 copies of a 32 MB string and
#     expects a size_t overflow, which only happens on 32-bit builds. On 64-bit
#     it just tries to allocate ~4 GB. Stock fails it identically.
#
# Expected differences in the output, none of them semantic:
#   - this fork prints an extra copyright line for -v
#   - each interpreter prints its own path in usage and error messages
#   - os.tmpname() names are random, and sort.lua prints its own timings
#   - memory totals are lower here (a table Node is 32 bytes, not 40), and the
#     "." progress marks come from a __gc finalizer that fires once per
#     collection, so a different footprint means a slightly different count
set -u
cd "$(dirname "$0")/.."
ROOT=$(pwd)
STOCK_TREE="${1:-$ROOT/lua-5.1.5}"
SUITE="${LUA_TESTS_DIR:-/tmp/lua5.1-tests}"
URL="https://www.lua.org/tests/lua5.1-tests.tar.gz"

if [ ! -d "$SUITE" ]; then
    echo "test suite not found at $SUITE"
    echo "fetch it with:  curl -O $URL && tar xzf lua5.1-tests.tar.gz -C /tmp"
    exit 2
fi
if [ ! -d "$STOCK_TREE/src" ]; then
    echo "stock Lua tree not found at $STOCK_TREE (pass it as the first argument)"
    exit 2
fi

POSIX_FLAGS='-DLUA_USE_POSIX'
POSIX_LIBS='-Wl,-E -ldl'
build() { ( cd "$1/src" && make -s clean >/dev/null 2>&1 &&
            make -s all MYCFLAGS="$POSIX_FLAGS" MYLIBS="$POSIX_LIBS" >/dev/null 2>&1 ); }

echo "building both without readline..."
build "$STOCK_TREE" || { echo "stock build failed"; exit 1; }
build "$ROOT"       || { echo "fork build failed"; exit 1; }

# a copy of the driver with big.lua removed
sed "s|  local f = coroutine.wrap(assert(loadfile('big.lua')))|  local f = function() return 'b' end --[[ skipped on 64-bit ]]|; \
     s|  assert(f() == 'a')|  --[[ skipped ]]|" "$SUITE/all.lua" > "$SUITE/all_nobig.lua"

run() { ( cd "$SUITE" && LUA_PATH="./?.lua;;" \
          LUA_INIT="package.path = '?;'..package.path" \
          timeout 900 "$1" all_nobig.lua ) > "$2" 2>&1; echo $?; }

echo "running stock..."; s_rc=$(run "$STOCK_TREE/src/lua" /tmp/official_stock.txt)
echo "running fork...";  o_rc=$(run "$ROOT/src/lua"       /tmp/official_ours.txt)
echo "  stock exit=$s_rc   fork exit=$o_rc"

norm() { sed -e '/Khashayar Fereidani/d' \
             -e "s|$STOCK_TREE/src/lua|LUA|g" -e "s|$ROOT/src/lua|LUA|g" \
             -e 's|/tmp/lua_[A-Za-z0-9]*|TMPNAME|g' \
             -e 's/total memory: [0-9]*K\?/total memory: N/' \
             -e 's/total time: [0-9.]*/total time: T/' \
             -e 's/ in [0-9.]* sec\./ in T sec./' \
             -e 's/\.\{20,\}/DOTS/g' \
             -e 's/test done on .*/test done on DATE/' "$1"; }

rc=0
[ "$s_rc" = 0 ] || { echo "FAIL: stock does not pass; fix the environment first"; rc=1; }
[ "$o_rc" = 0 ] || { echo "FAIL: this fork does not pass the official suite"; rc=1; }
if diff <(norm /tmp/official_stock.txt) <(norm /tmp/official_ours.txt) > /tmp/official_diff.txt; then
    echo "output identical to stock after normalization"
else
    echo "output differs from stock beyond the expected items:"
    head -20 /tmp/official_diff.txt
    rc=1
fi

echo "restoring the default build..."
( cd "$ROOT/src" && make -s clean >/dev/null 2>&1 && make -s linux >/dev/null 2>&1 )
exit $rc
