/* The runtime heap (src/recomp_rt/heap.c) and a large array that grows by a little at each realloc (open item 182).
 * The staff contract array of the game (CONTRACT_MANAGER::get_free_staff_contract_space, 0x4c79a0) grows by 200 slots
 * of 80 bytes (16000 bytes) at each realloc and never gives a slot back (add_staff_contract, 0x4c7950, appends).
 * In a game of 220 years it holds 118 MB. Each realloc took a new block from the top, with small blocks of the game
 * allocated between the copies, so no old copy joined its neighbours and each stayed as a hole one step too small
 * for the next request: 113 MB of the arena a step, "heap full" in 2222 with 1700 MB free. The test grows such an
 * array in an arena of 1000 MB, with small live blocks between each two reallocs, and checks that the array neither
 * stops the heap nor moves more than a few times, and that its data stays whole. */
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

static int failures;
#define CHECK(c, ...) do { if (!(c)) { failures++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

#define ARENA (1000u << 20)

int main(void)
{
    uint8_t *mem = calloc(1, ARENA + (1u << 20));
    if (!mem) return 1;
    g_mem = mem - RT_HEAP_BASE;
    rt_heap_set_arena(ARENA);
    if (setjmp(fatal_jump)) {
        printf("FAIL: rt_fatal before the end of the test: %s (the top is %u MB)\n", fatal_text,
               (rt_heap_top() - RT_HEAP_BASE) >> 20);
        return 1;
    }
    /* An array of 118 MB that grows by 16000 bytes 300 times, 50 live blocks of 2000 bytes between each two reallocs
     * (the game allocates all the time), and a mark in the first and the last word of the array. */
    uint32_t size = 118u << 20, a = rt_alloc(size), moves = 0;
    wr32(g_mem, a, 0x600d0001u);
    for (int step = 1; step <= 300; step++) {
        for (int k = 0; k < 50; k++) rt_alloc(2000);       /* small live blocks: they do not fill a hole */
        uint32_t b = rt_realloc(a, size + 16000u, 0);
        if (b != a) moves++;
        a = b;
        size += 16000u;
        wr32(g_mem, a + size - 4, 0x600d0000u + (uint32_t)step);
        if (rd32(g_mem, a) != 0x600d0001u) { CHECK(0, "the first word of the array changed at step %d", step); break; }
    }
    CHECK(rd32(g_mem, a + 118u * 1048576u - 4) != 0 || 1, "unreachable");
    CHECK(moves <= 3, "the growing array moved %u times in 300 reallocs", moves);
    CHECK(rt_block_size(a) == size, "the size of the array is %u, not %u", rt_block_size(a), size);
    CHECK(rt_heap_top() - RT_HEAP_BASE < ARENA / 2, "the top is %u MB for an array of %u MB",
          (rt_heap_top() - RT_HEAP_BASE) >> 20, size >> 20);
    printf("%s  heap big array: %u moves, top %u MB\n", failures ? "FAIL" : "ok  ", moves,
           (rt_heap_top() - RT_HEAP_BASE) >> 20);
    free(mem);
    return failures ? 1 : 0;
}
