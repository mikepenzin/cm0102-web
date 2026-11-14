/* The runtime heap (src/recomp_rt/heap.c) at the end of its arena (0.6.0 release test, decision O121): when the top
 * would pass the end, the free small blocks join the large spans once, and later blocks are cut from them. The test
 * runs the heap in a small arena: it fills it with blocks of many sizes, frees most of them, then asks for sizes
 * that the free lists do not hold. Before the join that stopped with "heap full"; now the blocks come from the
 * joined spans, no live block overlaps another, and a really full arena still stops with "heap full". */
#include "rt.h"
#include "rt_host.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cpu cpu;
uint8_t *g_mem;
static jmp_buf fatal_jump;
static char fatal_text[256];

void rt_fatal(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(fatal_text, sizeof fatal_text, fmt, ap);
    va_end(ap);
    longjmp(fatal_jump, 1);
}

void rt_guard_heap_top(uint32_t top) { (void)top; }   /* rt.c: the test switch RECOMP_GUARD, not in this test */

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define ARENA (8u << 20)
#define MAXB 200000

static uint32_t addr[MAXB], len[MAXB];
static int nb;

static void fill(int i) { memset(g_mem + addr[i], (int)(i * 37 + 11) & 0xff, len[i]); }
static int intact(int i)
{
    uint8_t want = (uint8_t)((i * 37 + 11) & 0xff);
    for (uint32_t k = 0; k < len[i]; k++)
        if (g_mem[addr[i] + k] != want) return 0;
    return 1;
}

int main(void)
{
    uint8_t *mem = calloc(1, ARENA + (1u << 20));
    if (!mem) return 1;
    g_mem = mem - RT_HEAP_BASE;
    rt_heap_set_arena(ARENA);
    if (setjmp(fatal_jump)) {
        printf("FAIL: rt_fatal before the end of the test: %s\n", fatal_text);
        return 1;
    }
    /* 1. Fill most of the arena with small blocks of sizes 16 to 512, then free 9 of 10: the free memory waits in
     *    the lists of those sizes. */
    uint32_t used = 0;
    while (used < ARENA - (1u << 20) && nb < MAXB) {
        len[nb] = 16u + (uint32_t)(nb % 32) * 16u;
        addr[nb] = rt_alloc(len[nb]);
        fill(nb);
        used += len[nb] + 16u;
        nb++;
    }
    int kept = 0;
    for (int i = 0; i < nb; i++) {
        if (i % 10) { rt_free(addr[i]); addr[i] = 0; }
        else kept++;
    }
    uint32_t top_before = rt_heap_top();
    /* 2. Ask for sizes that no free list holds (600 to 1944 bytes; a joined span is about 2.4 KB, since one kept
     *    block in ten splits the free memory): more than the room left above the top, so the heap must join. */
    int n2 = 0, start = nb;
    for (int k = 0; k < 3000 && nb < MAXB; k++) {
        len[nb] = 600u + (uint32_t)(k % 25) * 56u;
        addr[nb] = rt_alloc(len[nb]);
        fill(nb);
        nb++, n2++;
    }
    CHECK(n2 == 3000, "only %d of the blocks after the end", n2);
    CHECK(rt_heap_top() - RT_HEAP_BASE <= ARENA, "the top %#x is past the arena", rt_heap_top());
    /* 3. Every live block has its own bytes and lies inside the arena. */
    int bad = 0;
    for (int i = 0; i < nb; i++)
        if (addr[i] && (!intact(i) || addr[i] < RT_HEAP_BASE || addr[i] + len[i] > RT_HEAP_BASE + ARENA)) bad++;
    CHECK(!bad, "%d blocks overwritten or outside the arena", bad);
    CHECK(rt_is_block(addr[start]), "a block after the join is not a heap block");
    /* 4. Free, realloc and allocate again after the join. */
    for (int i = start; i < nb; i += 2) { rt_free(addr[i]); addr[i] = 0; }
    for (int i = start + 1; i < nb; i += 4) {
        uint32_t a = rt_realloc(addr[i], len[i] + 100, 0);
        memmove(g_mem + a, g_mem + a, 0);
        addr[i] = a;
        len[i] += 100;
        fill(i);
    }
    for (int k = 0; k < 2000 && nb < MAXB; k++) {
        len[nb] = 32u + (uint32_t)(k % 40) * 40u;
        addr[nb] = rt_alloc(len[nb]);
        fill(nb);
        nb++;
    }
    bad = 0;
    for (int i = 0; i < nb; i++)
        if (addr[i] && !intact(i)) bad++;
    CHECK(!bad, "%d blocks overwritten after free and realloc", bad);
    printf("heap: %d kept, top %u KB before the join, %u KB after; free %u KB\n", kept, (top_before - RT_HEAP_BASE) >> 10,
           (rt_heap_top() - RT_HEAP_BASE) >> 10, rt_heap_free_bytes() >> 10);
    /* 5. After the limit, a large block that grows by a little at each realloc (an array of the game grows by 6400
     *    bytes) keeps its bytes and mostly stays in place, in the last eighth of the arena kept for large blocks. */
    {
        uint32_t size = 64000, a = rt_alloc(size), moves = 0;
        for (uint32_t k = 0; k < size; k++) g_mem[a + k] = (uint8_t)(k * 7);
        for (int k = 0; k < 40; k++) {
            uint32_t b = rt_realloc(a, size + 6400, 0);
            if (b != a) moves++;
            a = b;
            for (uint32_t j = size; j < size + 6400; j++) g_mem[a + j] = (uint8_t)(j * 7);
            size += 6400;
        }
        uint32_t wrong = 0;
        for (uint32_t k = 0; k < size; k++) wrong += g_mem[a + k] != (uint8_t)(k * 7);
        CHECK(!wrong, "%u bytes of the growing block changed", wrong);
        CHECK(moves <= 8, "the growing block moved %u times in 40", moves);
        printf("heap: a block grew from 64000 to %u bytes in 40 steps and moved %u times\n", size, moves);
    }
    /* 6. A full arena still stops with "heap full". */
    if (!setjmp(fatal_jump)) {
        for (int k = 0; k < 100000; k++) rt_alloc(60000);
        CHECK(0, "no stop in a full arena");
    } else {
        CHECK(!strcmp(fatal_text, "heap full"), "the stop says %s", fatal_text);
    }
    if (failures) return 1;
    printf("heap: all checks pass\n");
    return 0;
}
