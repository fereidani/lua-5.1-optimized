# Performance suite

Differential benchmarks against stock Lua 5.1.5. Every configuration must
produce identical program output; `compare.py` checks that and prints
`MISMATCH` if it does not, so a build that is fast because it is wrong cannot
hide in the table.

## Running

```sh
make linux                                        # this fork
curl -O https://www.lua.org/ftp/lua-5.1.5.tar.gz  # stock, for comparison
tar xzf lua-5.1.5.tar.gz && (cd lua-5.1.5 && make linux)
python3 tests/perf/compare.py
```

Both trees must be built with the same compiler and the same `CFLAGS`, or the
comparison measures the compiler rather than the interpreter. `REPS=n` sets
repetitions (the minimum is reported), `CPU=n` pins to a core, `--vanilla DIR`
points at the stock tree.

Two optional configurations isolate the string-hash changes. Build them by
compiling this tree with `lstring.c` reverted to the stock sampled hash
(`src/lua_oldhash`) and with `-DFOLDHASH_FULL_LIMIT=0` (`src/lua_precap`);
`compare.py` picks them up automatically if present and skips them otherwise.

## Benchmarks

`gen_*` are non-string workloads (arithmetic, table indexing, calls, closures,
sorting) that isolate the VM. `str_*` are string workloads, since this fork
replaces Lua's string hash: interning, string-keyed tables, substring
extraction, concatenation both short and long, `string.format`, `gsub`,
compiling source, and serialize/parse round-trips. `app_*` run two real
libraries — `rxi/json.lua` and a pure-Lua SHA-2 — vendored here as
`lib_json.lua` and `lib_sha2.lua`.

`corpus.lua` builds a text corpus from this repository's own C sources, so the
string benchmarks run over realistic text rather than synthetic data.

## Reference results

clang `-O2 -Wall -DLUA_USE_LINUX`, x86-64, best of 7, pinned core, machine
otherwise idle, configurations round-robined. Both `luac`s emit byte-identical
bytecode for every benchmark here, so the two configurations differ only in the
interpreter.

| group | clang 21 | gcc 15 |
|-------|----------|--------|
| non-string (`gen_*`) | **-12.7%** | -18.9% |
| string (`str_*`)     | **-20.1%** | -25.7% |
| real libraries (`app_*`) | **-6.4%** | -21.8% |
| overall              | **-15.7%** | -22.5% |

The headline depends on the toolchain, and not only because our code compiles
differently: clang builds *stock* Lua's switch interpreter noticeably better
than gcc does, so the baseline it is measured against is faster. Quote the
compiler alongside the number.

Largest individual wins under clang: `str_concat_long` -45.1%,
`str_concat_short` -45.0%, `gen_field_access` -35.6%, `gen_table_ops` -27.2%,
`str_serialize` -24.3%, `str_keys` -22.0%, `gen_sieve` -21.9%.

Reading these:

- The non-string win is the VM: inlined table fast paths, key-type
  specialization, skipping the repeated lookup on an `__index` fallback, and
  the 32-byte table Node. Threaded dispatch contributes only under gcc; see
  below.
- **Dispatch is compiler-dependent.** Computed goto is a win under gcc and a
  loss under clang, which merges the per-opcode indirect branches back
  together. `lvm.c` enables it for gcc only. Measured on clang, forcing it on
  costs up to 20% (`gen_loop_math`) versus the plain switch.
- The string win is mostly the number-to-string and `string.format` fast paths,
  plus foldhash.
- **`str_concat_long` at -55% needs a caveat.** Stock 5.1 is *bimodal* on that
  workload: its sampled hash reads ~32 bytes at a stride proportional to
  length, so `s = s .. chunk` yields near-identical samples and the string
  table degenerates into long chains -- but only at some table sizes. Which
  mode you get depends on something as incidental as the script's path length,
  because the chunk name shifts when the table crosses a growth threshold.
  Measured by varying the chunk name from 44 to 54 characters, stock 5.1 goes
  from 7.71G to 14.46G retired instructions (+87%) while this fork stays flat
  at 7.63G. So the honest claim is not "twice as fast" -- it is "at parity with
  stock's good mode, and immune to its bad one". The harness passes absolute
  paths, which happens to land stock in the bad mode.

## Measurement notes

Wall-clock on a threaded interpreter is dominated by code layout. A build with
work *removed* measured **+5.0% cycles** on `gen_fib` while retiring 5.1% fewer
instructions. Any single-digit wall-clock delta here is probably alignment, not
your change.

So: use `perf stat -e instructions` to decide whether a change does less work,
and treat wall-clock as confirmation only, at `REPS=9` or higher. Several
"wins" in this suite evaporated when re-measured that way.

## Optional: LUA_NANBOX

Packs a Lua value into one 64-bit word instead of a value union plus a separate
tag. Off by default; build with `-DLUA_NANBOX`. Requires 64-bit pointers and
`LUA_NUMBER` as double, both enforced at compile time, and implies
`LUAI_NANCANON` because the encoding is unsound without it.

    TValue  16 -> 8 bytes
    TKey    24 -> 16
    Node    40 -> 24

It is a memory/speed trade, and which side you want depends on the workload:

| | |
|---|---|
| Lua heap, 200k array + 200k hash entries | **-24.1%** (27.4 MB -> 20.8 MB) |
| L1 data cache misses | -14% to -49% on table-heavy benchmarks |
| Wall clock, 12-benchmark total | **+3.0%** |

Per benchmark it ranges from -5.6% (`gen_loop_math`) to +11.8% (`app_json`);
it wins on 6 and loses on 6. The shrink pays where the working set is large and
access is memory-bound, and costs where it is compute-bound.

Retired instructions are close to parity -- about +2% on average, and *below*
classic on `gen_fib`, `gen_loop_math` and `gen_table_ops` -- so most of the
remaining wall-clock gap is the code-layout effect described above rather than
extra work. Two rounds of tuning got it there: routing numbers through a union
instead of memcpy (which changed nothing, the compiler had already lowered it),
and reshaping the hot predicates around the tag order, which halved the
instruction overhead.

Enable it if memory is the binding constraint -- an embedded host with a large
Lua heap -- and leave it off if throughput is.

## Investigated and rejected

Recorded so they are not re-litigated. Each was measured, not reasoned about.

- **Fast path for `luaD_precall`.** It is 8.6% of `app_json` and 14.4% of
  `gen_fib`, but `perf annotate` puts most of the `app_json` share on the
  indirect call to the C function (`callq *0x20(%rax)`), not on Lua frame
  setup — that workload calls `string.format` and `table.concat`, so the cost
  is inherent. Splitting the C branch into its own function to shrink the hot
  path's frame left instruction counts flat (-0.55% to +0.82%). Inlining frame
  setup into the VM would duplicate `ldo.c` logic for a ceiling of ~3% on the
  most call-heavy benchmark; not worth the divergence risk.
- **String table growth policy.** `luaS_resize` appeared at 7.8% of
  `str_substr`, but instrumenting the call site shows only **4 resizes and 224
  buckets rehashed** for the whole run. It cannot be hot; the profile line was
  symbol misattribution from inlining. No prize here.
- **GC tuning.** `sweeplist` is 10-12% of the string benchmarks, but raising
  `LUAI_GCPAUSE` does not convert that into speed. At `REPS=9` the stock 200 is
  the fastest setting on both `app_json` (1487ms vs 1516/1509) and
  `gen_closures` (1354ms vs 1373/1378), and 400 costs ~9% more peak RSS. An
  earlier -6.4% reading at `REPS=5` was noise. Left at the stock defaults.
- **Removing the per-instruction hook check entirely** via a dispatch-table
  swap. Worth 5-10% of retired instructions, but cycles moved -10.7% to +5.0%
  across benchmarks, i.e. unmeasurable, and a wrong re-arm breaks debuggers
  silently. The cheap half — caching the flag in a register — was taken
  instead.
