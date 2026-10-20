# Lua 5.1.5, faster

A performance fork of [Lua 5.1.5](https://www.lua.org/) that stays a drop-in
replacement: the same language, the same bytecode, the same C API. Scripts,
precompiled chunks and C modules built for stock Lua 5.1 work unchanged, and
`_VERSION` is still `"Lua 5.1"`.

Every change is tested against stock Lua 5.1.5 as the specification: the
official 5.1 test suite, hand-written differential suites and a random-program
fuzzer must all produce byte-identical output.

## What changed

- **String hashing**: foldhash (a C port of Rust's
  [foldhash](https://github.com/orlp/foldhash)) replaces Lua's sampled hash.
  It is faster per byte and does not collide on strings that differ only in
  unsampled positions. Cost is capped for very long strings.
- **Smaller tables**: a hash node's chain link is a 32-bit relative offset
  instead of a pointer, taking a node from 40 to 32 bytes, two per cache line.
- **Faster table access in the VM**: `t.field` and `t[i]` take a fast path
  without the generic lookup. A miss that falls through to `__index` (the
  normal case for `obj:method()`) no longer repeats the lookup.
- **Interpreter dispatch**: computed-goto threaded dispatch with gcc, branch
  hints on hot paths, and a cached hook flag instead of a reload per
  instruction. Ctrl-C and `lua_sethook` still interrupt tight loops.
- **Number and string formatting**: integral numbers convert to strings
  without `sprintf`. `string.format` handles `%d`, `%s` and the default number
  format directly. Output is byte-identical to stock.
- **Whole-library compilation**: the core and standard libraries build as one
  translation unit, so the compiler can inline across files. No `-flto` or
  special archiver is needed, so every platform target still works.
- **Undefined behaviour removed** from number-to-int conversion of table keys.

### Optional builds

- `-DLUA_NANBOX` packs every value into one 64-bit word (NaN-boxing), halving
  stack slots and array elements and shrinking table nodes to 24 bytes. It
  needs a 64-bit target.
- `-DLUAI_NANCANON` canonicalizes NaNs, keeping their sign. `LUA_NANBOX`
  enables it automatically.

## Performance

Best of 7 runs per benchmark against stock Lua 5.1.5, both built with the same
compiler and `-O2`, on an AMD EPYC (Rome). Negative is faster. The suite and
harness are in [`tests/perf`](tests/perf).

| benchmark        | gcc 15 | clang 21 |
|------------------|-------:|---------:|
| str_concat_short |   -47% |     -46% |
| gen_field_access |   -44% |     -40% |
| gen_table_ops    |   -36% |     -27% |
| gen_loop_math    |   -32% |      -4% |
| str_serialize    |   -31% |     -28% |
| str_keys         |   -30% |     -25% |
| gen_sort         |   -26% |     -22% |
| app_sha2         |   -25% |      -5% |
| gen_sieve        |   -19% |     -25% |
| gen_method_calls |   -17% |     -24% |
| str_format       |   -16% |     -17% |
| app_json         |   -12% |     -17% |
| str_substr       |   -12% |     -14% |
| gen_fib          |   -11% |      -7% |
| gen_closures     |   -11% |     -12% |
| str_gsub         |    -6% |      -4% |
| str_lexer        |    -5% |      -3% |
| str_concat_long† |    -2% |      -1% |
| str_intern       |     0% |      -3% |
| **geometric mean** | **-21%** | **-18%** |

† str_concat_long repeatedly appends to one string until it reaches 90 KB, and
its time is dominated by glibc returning heap memory to the kernel and faulting
it back in. Whether that happens depends on where a freed 90 KB string lands
in the heap, which can change with something as small as the script path's
length. Between runs it swung from -2% to +42% on the same binary. The
figure above was measured with `GLIBC_TUNABLES=glibc.malloc.trim_threshold=67108864`
on both interpreters, which removes that effect.

Part of the gain comes from compiling the core and libraries as one
translation unit (`src/onelua.c`), which lets the compiler inline across
files: about 6% with gcc and 11% with clang on its own. Interpreter loops are
also sensitive to code and stack layout, so measure your own workload with
your own toolchain; `compare.py` runs every interpreter from same-length paths
and varies the stack offset between runs so layout luck cannot pick a winner.

## Use cases

- **Applications that embed Lua 5.1** and want more speed without changing a
  line of script or C code, for example game engines, tools, servers or
  plugin hosts.
- **Platforms where LuaJIT is unavailable or not allowed**: no JIT under W^X
  policies, iOS and consoles, unusual architectures, or wherever a
  plain-C interpreter is required.
- **String- and table-heavy workloads** such as JSON and serialization,
  templating, text processing and data transformation, where the hashing,
  table and formatting changes pay off most.
- **Legacy Lua 5.1 code bases** that cannot move to 5.2+ semantics.

## Building

```sh
make linux          # or macosx, bsd, posix, ... (see INSTALL); gcc by default
make linux CC=clang
./build.sh          # lua and luac each from one amalgamated source file
```

On Windows, `build.bat` builds `lua51.dll`, `lua51.lib`, `lua.exe` and
`luac.exe` against the MSVC ABI with zig cc or clang.

## Testing

```sh
bash tests/run_checks.sh path/to/stock/lua    # differential suites + fuzz pass
bash tests/run_official.sh path/to/stock-tree # official 5.1 test suite
python3 tests/fuzz_differential.py 1 1000 --stock path/to/stock/lua
python3 tests/perf/compare.py --vanilla path/to/stock-tree
```

## License

MIT, like Lua itself; see [COPYRIGHT](COPYRIGHT). `src/foldhash.h` is derived
from Rust foldhash and carries its Zlib notice. The original Lua 5.1 README is
kept as [README](README).
