#!/bin/bash
# build.sh — Build Lua 5.1.5 from a single amalgamated source file
#
# Combines all .c files into one temporary source file, compiles it
# with a single gcc command, then deletes the temporary file.
#
# Usage: ./build.sh           (build lua + luac)
#        ./build.sh lua       (build lua only)
#        ./build.sh luac      (build luac only)
#        ./build.sh clean     (remove build artifacts)

set -e

CC="${CC:-gcc}"
CFLAGS="${CFLAGS:--O2 -Wall}"
LIBS="-lm"

# Core source files (order follows Makefile dependency chain)
CORE_FILES=(
    lapi.c
    lcode.c
    ldebug.c
    ldo.c
    ldump.c
    lfunc.c
    lgc.c
    llex.c
    lmem.c
    lobject.c
    lopcodes.c
    lparser.c
    lstate.c
    lstring.c
    ltable.c
    ltm.c
    lundump.c
    lvm.c
    lzio.c
)

# Library source files
LIB_FILES=(
    lauxlib.c
    lbaselib.c
    ldblib.c
    liolib.c
    lmathlib.c
    loslib.c
    ltablib.c
    lstrlib.c
    loadlib.c
    linit.c
)

cd "$(dirname "$0")/src"

clean() {
    rm -f lua luac lua_amalg.c luac_amalg.c
}

if [ "${1:-}" = "clean" ]; then
    clean
    echo "Cleaned."
    exit 0
fi

build_target() {
    local target="$1"
    shift
    local main_files=("$@")

    local amalg="${target}_amalg.c"

    echo "Creating ${target} amalgamation..."
    {
        # Include stdio.h first to process it before ldebug.h defines
        # the getline macro, preventing a conflict with POSIX getline()
        echo '#include <stdio.h>'
        echo ''
        # luaall_c enables conditional code in luaconf.h that is normally
        # gated on per-file defines (e.g. LUA_TMPNAMBUFSIZE needs loslib_c)
        echo '#define luaall_c'
        # luac_c enables luaU_print declaration in lundump.h
        if [ "$target" = "luac" ]; then
            echo '#define luac_c'
        fi
        echo ''
        for f in "${CORE_FILES[@]}" "${LIB_FILES[@]}" "${main_files[@]}"; do
            # Rename luac.c's static writer() to avoid conflict with lstrlib.c
            if [ "$f" = "luac.c" ]; then
                echo '#define writer luac_writer'
                echo "#include \"$f\""
                echo '#undef writer'
            else
                echo "#include \"$f\""
            fi
        done
    } > "$amalg"

    echo "Compiling ${target}..."
    $CC $CFLAGS -o "$target" "$amalg" $LIBS

    rm -f "$amalg"
    echo "Built: $(pwd)/${target}"
}

case "${1:-all}" in
    lua)
        build_target lua lua.c
        ;;
    luac)
        build_target luac luac.c print.c
        ;;
    all)
        build_target lua lua.c
        build_target luac luac.c print.c
        ;;
    *)
        echo "Usage: $0 [lua|luac|all|clean]" >&2
        exit 1
        ;;
esac

echo "Done."
