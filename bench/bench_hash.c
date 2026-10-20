/*
** bench_hash.c — Benchmark: current Lua 5.1 hash vs foldhash
**
** Build:  gcc -O2 -o bench_hash bench_hash.c
** Usage:  ./bench_hash
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "../src/foldhash.h"

/* ─── Current Lua 5.1 string hash (exact copy from lstring.c) ─── */

static unsigned int lua_hash(const char *str, size_t l) {
    unsigned int h = (unsigned int)l;
    size_t step = (l >> 5) + 1;
    size_t l1;
    for (l1 = l; l1 >= step; l1 -= step)
        h = h ^ ((h << 5) + (h >> 2) + (unsigned char)str[l1 - 1]);
    return h;
}

/* ─── Timing helpers ─── */

static double now_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* ─── Benchmark: speed ─── */

#define ITERS 2000000

typedef struct {
    const char *name;
    size_t len;
    const char *desc;
} bench_case;

static void bench_speed(const char *label, const char *data, size_t len,
                        unsigned int (*hash_fn)(const char *, size_t)) {
    volatile unsigned int sink = 0;
    double t0 = now_sec();
    for (int i = 0; i < ITERS; i++) {
        sink = hash_fn(data, len);
    }
    double elapsed = now_sec() - t0;
    (void)sink;

    double total_mb = (double)len * ITERS / (1024.0 * 1024.0);
    double mb_per_s = total_mb / elapsed;
    double ns_per_hash = elapsed / ITERS * 1e9;

    printf("  %-24s  %7.1f MB/s  %8.1f ns/hash  (h=0x%08x)\n",
           label, mb_per_s, ns_per_hash, hash_fn(data, len));
}

/* ─── Benchmark: distribution quality ─── */

#define DIST_KEYS 100000
#define DIST_BITS 17
#define DIST_SIZE (1 << DIST_BITS)
#define DIST_MASK (DIST_SIZE - 1)

static void bench_distribution(const char *label,
                               unsigned int (*hash_fn)(const char *, size_t)) {
    int *buckets = calloc(DIST_SIZE, sizeof(int));
    if (!buckets) { perror("malloc"); exit(1); }

    char keybuf[64];
    for (int i = 0; i < DIST_KEYS; i++) {
        snprintf(keybuf, sizeof(keybuf), "key_%d_value_%d", i, i * 7);
        unsigned int h = hash_fn(keybuf, strlen(keybuf));
        buckets[h & DIST_MASK]++;
    }

    int max_chain = 0, empty = 0, total_collisions = 0;
    double sum_sq = 0;
    double expected = (double)DIST_KEYS / DIST_SIZE;
    for (int i = 0; i < DIST_SIZE; i++) {
        if (buckets[i] == 0) empty++;
        if (buckets[i] > max_chain) max_chain = buckets[i];
        if (buckets[i] > 1) total_collisions += buckets[i] - 1;
        double diff = buckets[i] - expected;
        sum_sq += diff * diff;
    }
    double chi_sq = sum_sq / expected;

    printf("  %-24s  max_chain=%-3d  empty=%-6d  collisions=%-6d  chi2=%.2f\n",
           label, max_chain, empty, total_collisions, chi_sq);
    free(buckets);
}

/* ─── Test data generators ─── */

static char *make_random_string(size_t len) {
    char *s = malloc(len + 1);
    for (size_t i = 0; i < len; i++)
        s[i] = 'a' + (rand() % 26);
    s[len] = '\0';
    return s;
}

static char *make_repeating_string(size_t len) {
    char *s = malloc(len + 1);
    const char *pattern = "abcdefghijklmnopqrstuvwxyz0123456789";
    size_t plen = strlen(pattern);
    for (size_t i = 0; i < len; i++)
        s[i] = pattern[i % plen];
    s[len] = '\0';
    return s;
}

/* ─── Main ─── */

int main(void) {
    srand(42);

    printf("=== Hash Benchmark: Lua 5.1 vs foldhash ===\n");
    printf("Iterations per test: %d\n\n", ITERS);

    /* Prepare test strings */
    char *s_short = strdup("hello");
    char *s_ident = strdup("local_variable_name");
    char *s_med   = make_random_string(64);
    char *s_long  = make_random_string(512);
    char *s_vlong = make_random_string(8192);
    char *s_rep   = make_repeating_string(1024);

    struct {
        const char *label;
        const char *data;
        size_t len;
    } cases[] = {
        { "short (5 bytes)",        s_short, 5 },
        { "identifier (20 bytes)",  s_ident, 20 },
        { "medium (64 bytes)",      s_med,   64 },
        { "long (512 bytes)",       s_long,  512 },
        { "repeating (1024 bytes)", s_rep,   1024 },
        { "very long (8192 bytes)", s_vlong, 8192 },
        { NULL, NULL, 0 }
    };

    printf("--- Throughput ---\n");
    printf("  %-24s  %14s  %16s  %s\n", "Test", "Throughput", "Latency", "Sample");
    printf("\n");

    for (int i = 0; cases[i].data; i++) {
        char label_lua[64], label_fh[64];
        snprintf(label_lua, sizeof(label_lua), "Lua    %s", cases[i].label);
        snprintf(label_fh, sizeof(label_fh), "Fold   %s", cases[i].label);

        bench_speed(label_lua, cases[i].data, cases[i].len, lua_hash);
        bench_speed(label_fh, cases[i].data, cases[i].len,
                    (unsigned int (*)(const char *, size_t))foldhash_str);

        /* Compute speedup */
        /* We re-run to get the ratio from the printed numbers would be approximate,
           so let's just run a quick comparison */
        printf("\n");
    }

    printf("\n--- Distribution Quality (100K keys, 128K buckets) ---\n");
    bench_distribution("Lua 5.1 hash", lua_hash);
    bench_distribution("Foldhash",
                       (unsigned int (*)(const char *, size_t))foldhash_str);

    printf("\n--- Distribution: sequential integer keys ---\n");
    {
        int *buckets = calloc(DIST_SIZE, sizeof(int));
        char keybuf[32];
        for (int i = 0; i < DIST_KEYS; i++) {
            snprintf(keybuf, sizeof(keybuf), "%d", i);
            unsigned int h = lua_hash(keybuf, strlen(keybuf));
            buckets[h & DIST_MASK]++;
        }
        int max1 = 0;
        for (int i = 0; i < DIST_SIZE; i++)
            if (buckets[i] > max1) max1 = buckets[i];
        memset(buckets, 0, DIST_SIZE * sizeof(int));
        for (int i = 0; i < DIST_KEYS; i++) {
            snprintf(keybuf, sizeof(keybuf), "%d", i);
            unsigned int h = foldhash_str(keybuf, strlen(keybuf), 0);
            buckets[h & DIST_MASK]++;
        }
        int max2 = 0;
        for (int i = 0; i < DIST_SIZE; i++)
            if (buckets[i] > max2) max2 = buckets[i];
        free(buckets);
        printf("  Lua 5.1  max_chain=%d  (ideal: %.1f)\n", max1, (double)DIST_KEYS * 2 / DIST_SIZE);
        printf("  Foldhash max_chain=%d  (ideal: %.1f)\n", max2, (double)DIST_KEYS * 2 / DIST_SIZE);
    }

    /* Cleanup */
    free(s_short); free(s_ident); free(s_med);
    free(s_long); free(s_vlong); free(s_rep);

    return 0;
}
