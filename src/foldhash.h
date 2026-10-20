/*
** foldhash -- C port of Rust foldhash (github.com/orlp/foldhash)
** Fast, non-cryptographic, minimally DoS-resistant hash function.
**
** Constants derived from hexadecimal digits of pi.
** Core primitive: folded_multiply (64x64->128, XOR high/low halves).
**
** The algorithm and its constants are the work of Orson Peters. This is a
** derivative work: a C translation, verified bit-for-bit against the Rust
** original, plus a long-string sampling policy that is this port's own and is
** marked as a deviation where it appears below.
**
** ---------------------------------------------------------------------------
** Copyright (c) 2024 Orson Peters
** Copyright (c) 2026 Khashayar Fereidani (C port)
**
** This software is provided 'as-is', without any express or implied warranty.
** In no event will the authors be held liable for any damages arising from the
** use of this software.
**
** Permission is granted to anyone to use this software for any purpose,
** including commercial applications, and to alter it and redistribute it
** freely, subject to the following restrictions:
**
** 1. The origin of this software must not be misrepresented; you must not
**    claim that you wrote the original software. If you use this software in a
**    product, an acknowledgment in the product documentation would be
**    appreciated but is not required.
** 2. Altered source versions must be plainly marked as such, and must not be
**    misrepresented as being the original software.
** 3. This notice may not be removed or altered from any source distribution.
** ---------------------------------------------------------------------------
*/

#ifndef FOLDHASH_H
#define FOLDHASH_H

#include <stdint.h>
#include <string.h>

#if defined(_MSC_VER) && !defined(__SIZEOF_INT128__) && \
    (defined(_M_X64) || defined(_M_ARM64))
#include <intrin.h>
#endif

/* `inline` is C99. Lua 5.1 is otherwise C89, so spell it portably; compilers
   without any inline keyword still get correct (if unhinted) code.
   <stdint.h> is assumed available -- it is on every platform Lua 5.1 is
   built for today, including MSVC since 2010. */
#if defined(__cplusplus) || \
    (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L)
#define FH_INLINE static inline
#elif defined(__GNUC__)
#define FH_INLINE static __inline__
#elif defined(_MSC_VER)
#define FH_INLINE static __inline
#else
#define FH_INLINE static
#endif

/* Arbitrary constants with high entropy (hex digits of pi) */
#define FH_ARBITRARY0  UINT64_C(0x243f6a8885a308d3)
#define FH_ARBITRARY1  UINT64_C(0x13198a2e03707344)
#define FH_ARBITRARY2  UINT64_C(0xa4093822299f31d0)
#define FH_ARBITRARY3  UINT64_C(0x082efa98ec4e6c89)
#define FH_ARBITRARY4  UINT64_C(0x452821e638d01377)
#define FH_ARBITRARY5  UINT64_C(0xbe5466cf34e90c6c)
#define FH_ARBITRARY6  UINT64_C(0xc0ac29b7c97c50dd)
#define FH_ARBITRARY7  UINT64_C(0x3f84d5b5b5470917)
#define FH_ARBITRARY8  UINT64_C(0x9216d5d98979fb1b)
#define FH_ARBITRARY9  UINT64_C(0xd1310ba698dfb5ac)
#define FH_ARBITRARY10 UINT64_C(0x2ffd72dbd01adfb7)
#define FH_ARBITRARY11 UINT64_C(0xb8e1afed6a267e96)

/* Shared seed (equivalent to foldhash FIXED_GLOBAL_SEED) */
static const uint64_t fh_seeds[6] = {
    FH_ARBITRARY6,  FH_ARBITRARY7,  FH_ARBITRARY8,
    FH_ARBITRARY9,  FH_ARBITRARY10, FH_ARBITRARY11
};

/*
** Rotate right, matching Rust's u64::rotate_right: the rotation amount is
** taken modulo 64 and r == 0 is the identity. A plain (x >> r) | (x << (64-r))
** is undefined behaviour when r == 0 (shift count equal to the operand width);
** x86 and ARM happen to mask the count and produce the right answer anyway,
** but other targets and higher optimization levels are free not to.
*/
FH_INLINE uint64_t fh_rotr64(uint64_t x, unsigned r) {
    r &= 63;
    return r == 0 ? x : ((x >> r) | (x << (64 - r)));
}

/*
** Does this target have a native 64x64->128 widening multiply?
**
** foldhash uses the widening path on every 64-bit target except SPARC64 and
** WASM64; only the remaining targets fall back to the 32-bit approximation,
** which produces *different* hash values. Detect the widening multiply by
** capability (__int128 support) rather than by an architecture allow-list, so
** riscv64, ppc64, mips64, s390x, loongarch64 etc. take the reference path.
*/
#if !defined(FH_HAS_WIDE_MUL)
#if defined(__SIZEOF_INT128__) && !defined(__sparc__) && !defined(__wasm64__)
#define FH_HAS_WIDE_MUL 1
#else
#define FH_HAS_WIDE_MUL 0
#endif
#endif

/*
** folded_multiply: the core primitive.
** Compute (x * y) as a 128-bit product, then XOR the low and high 64-bit halves.
*/
FH_INLINE uint64_t fh_folded_multiply(uint64_t x, uint64_t y) {
#if FH_HAS_WIDE_MUL
    __uint128_t full = (__uint128_t)x * (__uint128_t)y;
    uint64_t lo = (uint64_t)full;
    uint64_t hi = (uint64_t)(full >> 64);
    return lo ^ hi;
#elif defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
    uint64_t hi;
    uint64_t lo = _umul128(x, y, &hi);
    return lo ^ hi;
#else
    /* 32-bit fallback: split multiply, approximate 128-bit product */
    uint32_t lx = (uint32_t)x, ly = (uint32_t)y;
    uint32_t hx = (uint32_t)(x >> 32), hy = (uint32_t)(y >> 32);
    uint64_t ll = (uint64_t)lx * ly;
    uint64_t lh = (uint64_t)lx * hy;
    uint64_t hl = (uint64_t)hx * ly;
    uint64_t hh = (uint64_t)hx * hy;
    return (hh ^ ll) ^ fh_rotr64(hl ^ lh, 32);
#endif
}

/* Load 8 bytes as a native-endian u64 (unaligned-safe).
   Matches Rust's u64::from_ne_bytes / read_unaligned: the hash value is
   endianness-dependent by design, exactly as in the reference. */
FH_INLINE uint64_t fh_load64(const uint8_t *p) {
    uint64_t v;
    memcpy(&v, p, 8);
    return v;
}

/* Load 4 bytes as a native-endian u32 (unaligned-safe) */
FH_INLINE uint32_t fh_load32(const uint8_t *p) {
    uint32_t v;
    memcpy(&v, p, 4);
    return v;
}

/*
** hash_bytes_short: hash byte sequences <= 16 bytes.
*/
FH_INLINE uint64_t fh_hash_short(const uint8_t *bytes, size_t len,
                                      uint64_t accumulator) {
    uint64_t s0 = accumulator;
    uint64_t s1 = fh_seeds[1];
    if (len >= 8) {
        s0 ^= fh_load64(bytes);
        s1 ^= fh_load64(bytes + len - 8);
    } else if (len >= 4) {
        s0 ^= (uint64_t)fh_load32(bytes);
        s1 ^= (uint64_t)fh_load32(bytes + len - 4);
    } else if (len > 0) {
        uint8_t lo = bytes[0];
        uint8_t mid = bytes[len / 2];
        uint8_t hi = bytes[len - 1];
        s0 ^= (uint64_t)lo;
        s1 ^= ((uint64_t)hi << 8) | (uint64_t)mid;
    }
    return fh_folded_multiply(s0, s1);
}

/*
** hash_bytes_long: hash byte sequences > 16 bytes.
** Faithfully ports the Rust foldhash hash_bytes_long.
** After the main accumulation loops, the tail is processed using overlapping
** reads from the front and back of the remaining data.
*/
FH_INLINE uint64_t fh_hash_long(const uint8_t *data, size_t len,
                                      uint64_t accumulator) {
    const uint8_t *v = data;
    uint64_t s0 = accumulator;
    uint64_t s1 = s0 + fh_seeds[1];

    if (len > 256) {
        uint64_t s2 = s0 + fh_seeds[2];
        uint64_t s3 = s0 + fh_seeds[3];
        uint64_t s4 = s0 + fh_seeds[4];
        uint64_t s5 = s0 + fh_seeds[5];
        while (len > 256) {
            s0 = fh_folded_multiply(fh_load64(v)       ^ s0, fh_load64(v + 48) ^ fh_seeds[0]);
            s1 = fh_folded_multiply(fh_load64(v + 8)   ^ s1, fh_load64(v + 56) ^ fh_seeds[0]);
            s2 = fh_folded_multiply(fh_load64(v + 16)  ^ s2, fh_load64(v + 64) ^ fh_seeds[0]);
            s3 = fh_folded_multiply(fh_load64(v + 24)  ^ s3, fh_load64(v + 72) ^ fh_seeds[0]);
            s4 = fh_folded_multiply(fh_load64(v + 32)  ^ s4, fh_load64(v + 80) ^ fh_seeds[0]);
            s5 = fh_folded_multiply(fh_load64(v + 40)  ^ s5, fh_load64(v + 88) ^ fh_seeds[0]);
            v += 96;
            len -= 96;
        }
        s0 ^= s4;
        s1 ^= s5;

        while (len > 128) {
            s0 = fh_folded_multiply(fh_load64(v)       ^ s0, fh_load64(v + 32) ^ fh_seeds[0]);
            s1 = fh_folded_multiply(fh_load64(v + 8)   ^ s1, fh_load64(v + 40) ^ fh_seeds[0]);
            s2 = fh_folded_multiply(fh_load64(v + 16)  ^ s2, fh_load64(v + 48) ^ fh_seeds[0]);
            s3 = fh_folded_multiply(fh_load64(v + 24)  ^ s3, fh_load64(v + 56) ^ fh_seeds[0]);
            v += 64;
            len -= 64;
        }
        s0 ^= s2;
        s1 ^= s3;
    } else if (len > 128) {
        uint64_t s2 = s0 + fh_seeds[2];
        uint64_t s3 = s0 + fh_seeds[3];
        while (len > 128) {
            s0 = fh_folded_multiply(fh_load64(v)       ^ s0, fh_load64(v + 32) ^ fh_seeds[0]);
            s1 = fh_folded_multiply(fh_load64(v + 8)   ^ s1, fh_load64(v + 40) ^ fh_seeds[0]);
            s2 = fh_folded_multiply(fh_load64(v + 16)  ^ s2, fh_load64(v + 48) ^ fh_seeds[0]);
            s3 = fh_folded_multiply(fh_load64(v + 24)  ^ s3, fh_load64(v + 56) ^ fh_seeds[0]);
            v += 64;
            len -= 64;
        }
        s0 ^= s2;
        s1 ^= s3;
    }

    /* Tail: overlapping reads from front and back of remaining data.
    ** len is now > 16 and <= 256 (or <= 128). */
    s0 = fh_folded_multiply(fh_load64(v)       ^ s0, fh_load64(v + len - 16) ^ fh_seeds[0]);
    s1 = fh_folded_multiply(fh_load64(v + 8)   ^ s1, fh_load64(v + len - 8)  ^ fh_seeds[0]);
    if (len >= 32) {
        s0 = fh_folded_multiply(fh_load64(v + 16)  ^ s0, fh_load64(v + len - 32) ^ fh_seeds[0]);
        s1 = fh_folded_multiply(fh_load64(v + 24)  ^ s1, fh_load64(v + len - 24) ^ fh_seeds[0]);
        if (len >= 64) {
            s0 = fh_folded_multiply(fh_load64(v + 32)  ^ s0, fh_load64(v + len - 48) ^ fh_seeds[0]);
            s1 = fh_folded_multiply(fh_load64(v + 40)  ^ s1, fh_load64(v + len - 40) ^ fh_seeds[0]);
            if (len >= 96) {
                s0 = fh_folded_multiply(fh_load64(v + 48)  ^ s0, fh_load64(v + len - 64) ^ fh_seeds[0]);
                s1 = fh_folded_multiply(fh_load64(v + 56)  ^ s1, fh_load64(v + len - 56) ^ fh_seeds[0]);
            }
        }
    }

    return s0 ^ s1;
}

/*
** foldhash_bytes: equivalent of foldhash::fast::FoldHasher::write() followed by
** finish() for a single byte-slice write, with `seed` as the per-hasher seed
** (seed == 0 reproduces FixedState::default(), which uses ARBITRARY3).
*/
FH_INLINE uint64_t foldhash_bytes(const char *data, size_t len, uint64_t seed) {
    const uint8_t *bytes = (const uint8_t *)data;
    /* Length-dependent rotation of the accumulator, defeating the trivial
       length-extension attacks the overlapping tail reads would otherwise
       allow. Reference: FoldHasher::write(). */
    uint64_t accumulator = fh_rotr64(seed ^ FH_ARBITRARY3, (unsigned)len);
    if (len <= 16) return fh_hash_short(bytes, len, accumulator);
    return fh_hash_long(bytes, len, accumulator);
}

/*
** =========================================================================
** DEVIATION FROM THE REFERENCE IMPLEMENTATION -- long-string sampling
** =========================================================================
**
** Everything above this point is a faithful port of Rust foldhash and is
** bit-for-bit identical to it; foldhash_bytes() is the reference entry point
** and must stay that way. What follows is Lua integration policy, not the
** foldhash algorithm, and it deliberately does NOT match upstream foldhash
** for inputs longer than FOLDHASH_FULL_LIMIT.
**
** Why it exists. Lua interns every string it creates, so luaS_newlstr()
** hashes each one. Stock Lua 5.1 reads at most ~32 sampled bytes regardless
** of length, making an intern O(1); foldhash reads every byte, making it
** O(len). That turns the extremely common Lua idiom
**
**     local s = "" ; for ... do s = s .. chunk end
**
** from O(n) total hashing into O(n^2). Measured against stock 5.1 on this
** exact loop: +70% at a 18 KB result, +139% at 72 KB, +47% at 288 KB, with
** the regression attributable entirely to the hash (verified by building the
** same tree with only lstring.c reverted). Crossover, where foldhash stops
** paying for itself, is around 700 bytes: at 20 bytes foldhash is 4.9x faster
** than the stock hash, at 8 KB it is 10.4x slower.
**
** What LuaJIT does, and how this differs. LuaJIT's primary hash
** (hash_sparse in lj_str.c) is constant time at every length: it reads four
** 32-bit words, at offsets 0, len-4, (len>>1)-2 and (len>>2)-1. Its
** full-content hash (hash_dense) is only a *secondary* algorithm, switched on
** per bucket when a chain exceeds LJ_STR_MAXCOLL (32) collisions, so linear
** hashing is paid only under actual collision pressure. That adaptive scheme
** needs a per-string "which algorithm hashed me" flag and a reworked
** luaS_resize; adding a field to TString would change its layout, which a
** drop-in Lua 5.1 replacement cannot do. So we take the constant-time half of
** LuaJIT's idea only:
**
**   len <= FOLDHASH_FULL_LIMIT : full foldhash, bit-identical to the reference
**   len >  FOLDHASH_FULL_LIMIT : three fixed windows (head, middle, tail) are
**                                hashed instead of the whole string, so the
**                                cost stops growing
**
** The windows are run through the unmodified reference fh_hash_long(), each
** chained into the next one's accumulator, and the accumulator is seeded from
** the *true* length exactly as FoldHasher::write() does -- so two strings of
** different lengths never collide just because their windows match. We sample
** 384 bytes where LuaJIT samples 16, which costs a little more but collides a
** lot less.
**
** Security note. Above the limit this is sampling, so an attacker who
** controls long strings can force collisions among them, exactly as they can
** against stock Lua 5.1 and against LuaJIT built without
** LUAJIT_SECURITY_STRHASH. Correctness is unaffected -- luaS_newlstr still
** confirms every hash match with memcmp -- but a collision chain degrades to
** a linear scan. Strings at or below the limit keep foldhash's full mixing.
** Define FOLDHASH_FULL_LIMIT as 0 to disable sampling and hash every string
** in full (reference behaviour, at the cost of the quadratic idiom above).
*/
#if !defined(FOLDHASH_FULL_LIMIT)
/* Chosen from the measured crossover: below this foldhash is faster than the
   stock 5.1 hash, above it foldhash starts losing. */
#define FOLDHASH_FULL_LIMIT 512
#endif

/* Bytes taken from each of the head, middle and tail windows. 128 is the
   largest size for which fh_hash_long() still reads every byte handed to it
   (its tail block covers offsets 0..63 and len-64..len-1). */
#define FOLDHASH_WINDOW 128

/* The three windows are only in bounds once len > FOLDHASH_WINDOW, and the
   sampling path is entered at len > FOLDHASH_FULL_LIMIT, so the limit may
   never be lowered below the window size. */
#if FOLDHASH_FULL_LIMIT && (FOLDHASH_FULL_LIMIT) < (FOLDHASH_WINDOW)
#error "FOLDHASH_FULL_LIMIT must be 0 (disabled) or >= FOLDHASH_WINDOW"
#endif

/*
** foldhash_str: hash a byte sequence of given length with a per-instance seed.
** Returns a 32-bit hash suitable for Lua's string table. The reference returns
** a full u64; XOR-folding the halves keeps the high bits' entropy in the low
** bits, which is what Lua's lmod()/hashpow2() masking consumes.
*/
FH_INLINE unsigned int foldhash_str(const char *data, size_t len, uint64_t seed) {
    uint64_t h;
#if FOLDHASH_FULL_LIMIT
    if (len > (size_t)FOLDHASH_FULL_LIMIT) {
        const uint8_t *p = (const uint8_t *)data;
        /* Same length-dependent accumulator rotation the reference applies in
           FoldHasher::write(), using the true length rather than the number of
           bytes we go on to read. */
        uint64_t acc = fh_rotr64(seed ^ FH_ARBITRARY3, (unsigned)len);
        acc = fh_hash_long(p, FOLDHASH_WINDOW, acc);
        acc = fh_hash_long(p + (len >> 1) - (FOLDHASH_WINDOW / 2),
                           FOLDHASH_WINDOW, acc);
        acc = fh_hash_long(p + len - FOLDHASH_WINDOW, FOLDHASH_WINDOW, acc);
        h = acc;
    } else {
        h = foldhash_bytes(data, len, seed);
    }
#else
    h = foldhash_bytes(data, len, seed);
#endif
    return (unsigned int)(h ^ (h >> 32));
}

#endif /* FOLDHASH_H */
