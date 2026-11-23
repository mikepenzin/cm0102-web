/* The zone of the runtime heap (src/recomp_rt/heap.c, 0.6.1): while rt_heap_zone returns 1 (the player search,
 * playersearch.c), new blocks come from RT_ZONE_BASE up and a heap block that is resized moves there. The test
 * checks that the zone time leaves the heap as it found it, apart from the blocks it frees or moves: the top, the
 * free bytes and the next block of the heap are the same with a small zone time and with a large one. It also
 * checks that zone blocks keep their bytes, grow, go back to the zone when freed after the zone time, and that a
 * second zone time reuses the memory of the first. */
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

#define ARENA (16u << 20)
#define ZONE_USE (64u << 20)            /* the part of the zone that the test can touch */

static int zone_time;
static int zone_hook(void) { return zone_time; }
static int in_zone(uint32_t a) { return a >= RT_ZONE_BASE && a < RT_ZONE_BASE + RT_ZONE_SIZE; }
static int in_heap(uint32_t a) { return a >= RT_HEAP_BASE && a < RT_HEAP_BASE + ARENA; }

#define ROWS 4000
static uint32_t rows[ROWS];

/* What a search does in the zone time: a list block that grows by 250 rows of 45 bytes, two small blocks per row,
 * a passed array, and the resize of a text block of the heap. n: the rows (the option on lists more). Returns the
 * text block, which may have moved. */
static uint32_t search(uint32_t n, uint32_t text)
{
    zone_time = 1;
    uint32_t list = 0, cap = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (i == cap) {
            cap += 250;
            list = rt_realloc(list, cap * 45, 0);
            CHECK(in_zone(list), "the list block %#x is outside the zone", list);
        }
        memset(g_mem + list + i * 45, (int)(i & 0xff), 45);
    }
    for (uint32_t i = 0; i < n && i < ROWS / 2; i++) {
        rows[2 * i] = rt_alloc(16);
        rows[2 * i + 1] = rt_alloc(48);
        CHECK(in_zone(rows[2 * i]) && in_zone(rows[2 * i + 1]), "a row block is outside the zone");
    }
    uint32_t passed = rt_alloc_zero(n * 4);
    CHECK(in_zone(passed), "the passed array is outside the zone");
    text = rt_realloc(text, 64 + n / 10, 0);
    CHECK(in_zone(text), "the resized text block %#x is outside the zone", text);
    rt_free(passed);
    for (uint32_t i = 0; i < n; i++)
        if (g_mem[list + i * 45 + 44] != (uint8_t)(i & 0xff)) { CHECK(0, "row %u of the list changed", i); break; }
    zone_time = 0;
    /* The screen closes after the zone time: its blocks go back to the zone. */
    for (uint32_t i = 0; i < n && i < ROWS / 2; i++) rt_free(rows[2 * i]), rt_free(rows[2 * i + 1]);
    rt_free(list);
    return text;
}

/* The heap's state that the game sees: the top, the free bytes and the next blocks of a few sizes. */
typedef struct { uint32_t top, free, next[4]; } HeapState;

static HeapState state(void)
{
    HeapState h = {0, 0, {0}};
    static const uint32_t sizes[4] = {16, 48, 200, 9000};
    for (int k = 0; k < 4; k++) h.next[k] = rt_alloc(sizes[k]);
    for (int k = 3; k >= 0; k--) rt_free(h.next[k]);
    h.top = rt_heap_top();
    h.free = rt_heap_free_bytes();
    return h;
}

int main(void)
{
    /* The flat memory from the zone to the end of the arena; the pages are only touched where blocks are. */
    size_t span = (size_t)(RT_HEAP_BASE - RT_ZONE_BASE) + ARENA + (1u << 20);
    uint8_t *mem = calloc(1, span);
    if (!mem) return 1;
    g_mem = mem - RT_ZONE_BASE;
    rt_heap_set_arena(ARENA);
    rt_heap_zone = zone_hook;
    if (setjmp(fatal_jump)) {
        printf("FAIL: rt_fatal: %s\n", fatal_text);
        return 1;
    }

    /* 1. The game's heap: blocks of the game and a text block of the screen code, some freed. */
    uint32_t keep[64];
    for (int i = 0; i < 64; i++) keep[i] = rt_alloc((uint32_t)(16 + i * 24));
    for (int i = 0; i < 64; i += 3) rt_free(keep[i]), keep[i] = 0;
    uint32_t text = rt_alloc(64);
    memcpy(g_mem + text, "Player Search", 14);
    CHECK(in_heap(text), "a block outside the zone time is not in the heap");

    /* 2. A search with few rows (the option off). */
    HeapState before = state();
    uint32_t text_off = search(1678, text);
    CHECK(!memcmp(g_mem + text_off, "Player Search", 14), "the moved text block lost its bytes");
    HeapState after_off = state();
    CHECK(after_off.top == before.top, "the heap top moved in the zone time: %#x, was %#x", after_off.top, before.top);
    uint32_t zone_peak = rt_heap_zone_top();

    /* 3. The same start with many rows (the option on): the heap must end the same as with few. A second text
     *    block stands in for the first one, which moved to the zone in step 2. */
    uint32_t text2 = rt_alloc(64);
    HeapState before_on = state();
    uint32_t moved = search(7250, text2);
    HeapState after_on = state();
    CHECK(after_on.top == before_on.top, "the heap top moved in the zone time");
    uint32_t text3 = rt_alloc(64);
    CHECK(text3 == text2, "the heap does not give back the moved text block: %#x, was %#x", text3, text2);
    rt_free(text3);
    rt_free(moved);
    /* The only change to the heap is the free of the resized text block (64 bytes and its 16-byte header), with few
     * rows and with many; the next blocks of other sizes are the same blocks as before. */
    CHECK(after_off.free == before.free + 80, "few rows: the free bytes of the heap %u, before %u", after_off.free, before.free);
    CHECK(after_on.free == before_on.free + 80, "many rows: the free bytes of the heap %u, before %u", after_on.free, before_on.free);
    for (int k = 0; k < 4; k++) {
        CHECK(after_off.next[k] == before.next[k], "few rows: next block %d is %#x, was %#x", k, after_off.next[k], before.next[k]);
        CHECK(after_on.next[k] == before_on.next[k], "many rows: next block %d is %#x, was %#x", k, after_on.next[k], before_on.next[k]);
        CHECK(in_heap(after_on.next[k]), "a block after the zone time is not in the heap");
    }

    /* 4. The zone reuses its memory: a second search of the same size does not raise its top. */
    uint32_t peak2 = rt_heap_zone_top();
    search(7250, rt_alloc(64));
    CHECK(rt_heap_zone_top() == peak2, "the zone top rose from %#x to %#x for the same search", peak2, rt_heap_zone_top());
    CHECK(rt_heap_zone_top() - RT_ZONE_BASE < ZONE_USE, "the zone uses %u bytes", rt_heap_zone_top() - RT_ZONE_BASE);
    CHECK(zone_peak <= peak2, "the zone top went down");

    /* 5. Zone blocks are heap blocks to the game: size, is-block, free after the zone time. */
    zone_time = 1;
    uint32_t z = rt_alloc(100);
    zone_time = 0;
    CHECK(rt_is_block(z) && rt_block_size(z) == 100, "a zone block: is-block %d, size %u", rt_is_block(z), rt_block_size(z));
    uint32_t z2 = rt_realloc(z, 30000, 0);
    CHECK(in_zone(z2), "a zone block that grows after the zone time left the zone");
    rt_free(z2);
    CHECK(!rt_is_block(z2), "a freed zone block is still a block");

    /* 6. The blocks of the game kept their bytes. */
    for (int i = 0; i < 64; i++)
        if (keep[i]) CHECK(in_heap(keep[i]) && rt_is_block(keep[i]), "block %d of the game is lost", i);

    if (failures) return 1;
    printf("ok: the zone time leaves the heap as it was (top %#x), the zone top %#x\n", rt_heap_top(), rt_heap_zone_top());
    return 0;
}
