#!/usr/bin/env python3
"""Differential performance harness: this fork vs. stock Lua 5.1.5.

Compiles every benchmark with each interpreter's own luac, runs each
(interpreter, bytecode) pair, and reports the best of N wall-clock times plus
the percentage deltas. It also checks that every configuration produced
identical program output, so a "fast" build that computes the wrong answer
cannot hide in the table.

    python3 tests/perf/compare.py [glob] [--vanilla DIR]

Configurations, chosen so each change can be attributed independently:

    vanilla            stock Lua 5.1.5
    ours               this fork
    ours oldhash       this fork with lstring.c reverted to the stock 5.1
                       sampled hash -- isolates foldhash
    ours precap        this fork with FOLDHASH_FULL_LIMIT=0 -- isolates the
                       long-string sampling cap

The last two are optional; they are skipped if the binaries are absent. Build
them with the helper below.

To build everything (from the repository root):

    make linux                                    # this fork -> src/lua, src/luac
    curl -O https://www.lua.org/ftp/lua-5.1.5.tar.gz && tar xzf lua-5.1.5.tar.gz
    (cd lua-5.1.5 && make linux)                  # stock, same compiler/flags

Use identical CC and CFLAGS for both trees or the comparison is meaningless.

Environment:
    REPS=n      repetitions per measurement (default 5; the minimum is used)
    CPU=n       pin to this core with taskset (default 2, set empty to disable)
"""
import glob
import os
import subprocess
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ROOT = os.path.dirname(ROOT) if os.path.basename(ROOT) == "tests" else ROOT
SUITE = os.path.join(ROOT, "tests", "perf")
WORK = os.path.join(SUITE, ".work")
REPS = int(os.environ.get("REPS", "5"))
CPU = os.environ.get("CPU", "2")

vanilla_dir = os.path.join(ROOT, "lua-5.1.5")
argv = [a for a in sys.argv[1:]]
if "--vanilla" in argv:
    i = argv.index("--vanilla")
    vanilla_dir = argv[i + 1]
    del argv[i:i + 2]
pattern = argv[0] if argv else "*"

OURS = os.path.join(ROOT, "src")
CONFIGS = [
    ("vanilla", os.path.join(vanilla_dir, "src", "lua"), "van", True),
    ("ours", os.path.join(OURS, "lua"), "our", False),
    ("ours oldhash", os.path.join(OURS, "lua_oldhash"), "our", False),
    ("ours precap", os.path.join(OURS, "lua_precap"), "our", False),
]
LUAC_VAN = os.path.join(vanilla_dir, "src", "luac")
LUAC_OUR = os.path.join(OURS, "luac")


def benchmarks():
    # corpus.lua and lib_*.lua are support files, not benchmarks
    skip = {"corpus.lua"}
    return sorted(os.path.basename(f) for f in glob.glob(os.path.join(SUITE, pattern + ".lua"))
                  if os.path.basename(f) not in skip
                  and not os.path.basename(f).startswith("lib_"))


def compile_all(names, have_vanilla):
    os.makedirs(WORK, exist_ok=True)
    for n in names:
        src = os.path.join("tests", "perf", n)
        if have_vanilla:
            subprocess.run([LUAC_VAN, "-o", os.path.join(WORK, "van_" + n + ".out"), src],
                           check=True, cwd=ROOT)
        subprocess.run([LUAC_OUR, "-o", os.path.join(WORK, "our_" + n + ".out"), src],
                       check=True, cwd=ROOT)


def stage(configs):
    """Give every interpreter a path of the same length.

    The kernel copies the program path, the arguments and the environment onto
    a new process's initial stack, so their total length decides where the
    stack starts, and some benchmarks are acutely sensitive to that:
    str_concat_long took 1.5s or 3.2s on the same stock binary depending only
    on the length of the path it was run from. Linking each interpreter as
    WORK/cN/lua (the bytecode names already match in length) gives every
    configuration the same start. pad_env() then moves that start between
    repetitions, identically for all configurations, so best-of-N samples
    several stack offsets instead of one lucky or unlucky one.
    """
    staged = []
    for i, (label, exe, pref, is_vanilla) in enumerate(configs):
        link = os.path.join(WORK, "c%d" % i, "lua")
        os.makedirs(os.path.dirname(link), exist_ok=True)
        if os.path.lexists(link):
            os.remove(link)
        os.symlink(os.path.abspath(exe), link)
        staged.append((label, link, pref, is_vanilla))
    return staged


def pad_env(rep):
    """The environment for repetition `rep`, shifted by a padding variable."""
    return dict(os.environ, LUA_PERF_PAD="x" * (rep * 48 % 528))


def run_once(exe, bc, env):
    cmd = (["taskset", "-c", CPU] if CPU else []) + [exe, bc]
    t0 = time.perf_counter()
    p = subprocess.run(cmd, capture_output=True, cwd=ROOT, env=env)
    dt = time.perf_counter() - t0
    if p.returncode != 0:
        return None, p.stderr.decode()[:200]
    return dt, p.stdout.decode().strip()


def time_all(configs, name):
    """Round-robin the configurations across repetitions.

    Timing all reps of one configuration before moving to the next makes the
    result depend on when each configuration happened to run. This machine
    drifts between a fast and a slow mode over tens of seconds; measured in
    consecutive blocks, whichever configuration goes first absorbs the drift,
    which produced a bogus 2.2x "win" on str_concat_long where interleaved
    measurement shows parity. Round-robin spreads any drift evenly, so the
    comparison stays fair even when the machine does not hold still.
    """
    best = {c[0]: None for c in configs}
    outs = {c[0]: "" for c in configs}
    for rep in range(REPS):
        env = pad_env(rep)
        for label, exe, pref, _ in configs:
            dt, out = run_once(exe, os.path.join(WORK, pref + "_" + name + ".out"), env)
            if dt is None:
                best[label] = None
                outs[label] = out
                continue
            outs[label] = out
            if best[label] is None or dt < best[label]:
                best[label] = dt
    return best, outs


def main():
    configs = [c for c in CONFIGS if os.path.exists(c[1])]
    missing = [c[0] for c in CONFIGS if not os.path.exists(c[1])]
    if missing:
        print("skipping (binary not built): " + ", ".join(missing) + "\n")
    have_vanilla = any(c[3] for c in configs)
    if not have_vanilla:
        print("NOTE: no vanilla build found; showing this fork's configurations only.\n")
    names = benchmarks()
    compile_all(names, have_vanilla)
    configs = stage(configs)

    print("%-22s" % "benchmark" + "".join("%13s" % c[0] for c in configs) + "   output")
    rows = []
    for n in names:
        best, outmap = time_all(configs, n)
        res = [best[c[0]] for c in configs]
        outs = [outmap[c[0]] for c in configs]
        agree = "ok" if len(set(outs)) == 1 else "MISMATCH"
        rows.append((n, dict(zip([c[0] for c in configs], res))))
        print("%-22s" % n + "".join(("%12.3fs" % t) if t else "%13s" % "FAIL" for t in res)
              + "   " + agree)

    base = "vanilla" if have_vanilla else "ours"
    cols = [c[0] for c in configs if c[0] != base]
    print("\n%-22s" % "benchmark" + "".join("%13s" % ("vs " + base) if i == 0 else "%13s" % c
                                            for i, c in enumerate(cols)))
    for n, r in rows:
        if not all(r.values()):
            continue
        cells = "".join("%12s%%" % ("%+.1f" % ((r[c] - r[base]) * 100.0 / r[base])) for c in cols)
        print("%-22s" % n + cells)


main()
