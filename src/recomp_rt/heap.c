/* The heap inside the flat memory, from RT_HEAP_BASE up.
 *
 * Two modes:
 * - bump (rt_heap_bump = 1): never reuses memory and rounds each block to 16 bytes. The database load
 *   test (tests/recomp) uses it, so that the addresses match the Unicorn run byte for byte.
 * - free list (the default): each block has a 16-byte header before it with the asked size and the
 *   room. Small blocks (up to SMALL_MAX bytes of room) go back to a list per room size. Large blocks go
 *   back to a list sorted by address and join their free neighbours.
 *
 * The limit of the arena (0.6.0 release test). A Select All game from 2001 had 163 MB in use and its top at 380 MB
 * after one season, and reached the end of the arena of 0.5.0 (RT_HEAP_SIZE, 1024 MB) in its ninth season ("heap
 * full"). Two causes: freed small blocks stay in the list of their own room and never join, and an array of the game
 * grows by 6400 bytes at each realloc, about 843 times a season, so each old copy stays as a hole just too small for
 * the next one. So:
 * - The arena is RT_HEAP_ARENA (2032 MB, to 0xEF000000). Small blocks stop one eighth before its end, and that last
 *   eighth is kept for large blocks.
 * - When the top would pass the end, the free small blocks join the large spans (sorted by address, neighbours joined;
 *   again at each later time the arena is full). From then on a small block whose list is empty is cut from one of
 *   those spans, and a large block that grows by realloc gets half its size again as room, so that it grows in place.
 * Up to the end for small blocks (far past 1024 MB) nothing differs, so every address of a game that 0.5.0's arena
 * could hold stays the same (Q152). RECOMP_HEAP_ARENA_MB (a test switch) makes the arena smaller, so a test game
 * reaches the end early. */
#include "rt.h"
#include "rt_host.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __EMSCRIPTEN__
void rt_web_heap_top(uint32_t top);   /* host/web.c: the browser's memory reaches the new top */
#define HEAP_GREW() rt_web_heap_top(heap_top)
#else
void rt_guard_heap_top(uint32_t top);   /* rt.c: the test switch RECOMP_GUARD follows the top */
#define HEAP_GREW() rt_guard_heap_top(heap_top)
#endif

int rt_heap_bump = 0;
static uint32_t heap_top = RT_HEAP_BASE;
static uint32_t carve_addr, carve_room;         /* after the join at the end of the arena: the span that small
                                                 * blocks are cut from (heap.c, below) */

uint32_t rt_heap_top(void) { return heap_top; }

/* Bump mode: block sizes for realloc, an open hash from address to size. */
static uint32_t *size_keys, *size_vals;
static uint32_t size_cap, size_count;

static void size_put(uint32_t addr, uint32_t size)
{
    if (size_count * 2 >= size_cap) {
        uint32_t old_cap = size_cap, *ok = size_keys, *ov = size_vals;
        size_cap = size_cap ? size_cap * 2 : 1024;
        size_keys = calloc(size_cap, 4);
        size_vals = calloc(size_cap, 4);
        size_count = 0;
        for (uint32_t i = 0; i < old_cap; i++)
            if (ok[i]) size_put(ok[i], ov[i]);
        free(ok);
        free(ov);
    }
    uint32_t i = (addr >> 4) * 2654435761u & (size_cap - 1);
    while (size_keys[i] && size_keys[i] != addr) i = (i + 1) & (size_cap - 1);
    if (!size_keys[i]) size_count++;
    size_keys[i] = addr;
    size_vals[i] = size;
}

static uint32_t size_get(uint32_t addr)
{
    if (!size_cap) return 0;
    uint32_t i = (addr >> 4) * 2654435761u & (size_cap - 1);
    while (size_keys[i]) {
        if (size_keys[i] == addr) return size_vals[i];
        i = (i + 1) & (size_cap - 1);
    }
    return 0;
}

/* Free list mode. Header at block - 16: asked size, room, state. */
#define HDR 16u
#define MAGIC_USED 0x55534544u
#define MAGIC_FREE 0x46524545u
#define SMALL_MAX 4096u
#define SMALL_CLASSES (SMALL_MAX / 16 + 1)

static uint32_t small_free[SMALL_CLASSES];     /* first free block of each room; next is in its first word */

typedef struct { uint32_t addr, room; } Span;
static Span *large;                             /* free large blocks, sorted by address */
static uint32_t large_count, large_cap;

/* For the logs (season_report.c): the bytes free inside the heap (the small lists and the large list); the bytes in
 * use are heap_top - RT_HEAP_BASE less these. It walks the small lists: a log line at a season change only. */
static uint64_t small_free_bytes, small_free_blocks;

uint32_t rt_heap_free_bytes(void)
{
    uint64_t n = 0;
    small_free_bytes = small_free_blocks = 0;
    for (uint32_t c = 1; c < SMALL_CLASSES; c++)
        for (uint32_t a = small_free[c], k = 0; a && k < 10000000u; a = rd32(g_mem, a), k++)
            n += HDR + c * 16u, small_free_blocks++;
    small_free_bytes = n;
    for (uint32_t i = 0; i < large_count; i++) n += HDR + large[i].room;
    if (carve_room) n += HDR + carve_room;
    return n > 0xffffffffu ? 0xffffffffu : (uint32_t)n;
}

/* After rt_heap_free_bytes: the free small blocks (bytes, count) and the large spans (count). */
void rt_heap_free_parts(uint32_t *small_bytes, uint32_t *small_blocks, uint32_t *spans)
{
    *small_bytes = (uint32_t)small_free_bytes;
    *small_blocks = (uint32_t)small_free_blocks;
    *spans = large_count;
}

static uint32_t arena_end;                      /* RT_HEAP_BASE + the arena: RT_HEAP_ARENA, or RECOMP_HEAP_ARENA_MB */
static int joined;                              /* 1 after the join at the end of the arena */

static uint32_t heap_end(void)
{
    if (!arena_end) {
        /* RECOMP_HEAP_START_MB (a test switch): the top starts that far into the arena, so a test game has its blocks
         * at high addresses (from 1280 MB: 0xC0000000 up). */
        const char *s = getenv("RECOMP_HEAP_START_MB");
        unsigned long start = s && *s ? strtoul(s, NULL, 10) : 0;
        if (start && start < (RT_HEAP_ARENA >> 21) * 3 / 2 && heap_top == RT_HEAP_BASE) heap_top += (uint32_t)start << 20;
        HEAP_GREW();
        const char *v = getenv("RECOMP_HEAP_ARENA_MB");
        unsigned long mb = v && *v ? strtoul(v, NULL, 10) : 0;
        arena_end = RT_HEAP_BASE + (mb && mb < (RT_HEAP_ARENA >> 20) ? (uint32_t)mb << 20 : RT_HEAP_ARENA);
    }
    return arena_end;
}

uint32_t rt_heap_arena_bytes(void) { return heap_end() - RT_HEAP_BASE; }

void rt_heap_set_arena(uint32_t bytes) { arena_end = RT_HEAP_BASE + (bytes && bytes < RT_HEAP_ARENA ? bytes : RT_HEAP_ARENA); }

/* The end for small blocks: the last eighth of the arena is kept for large blocks, which cannot be cut from the
 * joined spans when they are larger than all of them. */
static uint32_t small_end(void) { return heap_end() - (((heap_end() - RT_HEAP_BASE) >> 3) & ~15u); }

static uint32_t h_size(uint32_t a) { return rd32(g_mem, a - HDR); }
static uint32_t h_room(uint32_t a) { return rd32(g_mem, a - HDR + 4); }
static uint32_t h_state(uint32_t a) { return rd32(g_mem, a - HDR + 8); }
static void h_set(uint32_t a, uint32_t size, uint32_t room, uint32_t state)
{
    wr32(g_mem, a - HDR, size);
    wr32(g_mem, a - HDR + 4, room);
    wr32(g_mem, a - HDR + 8, state);
    wr32(g_mem, a - HDR + 12, 0);
}

/* A block from the top, or 0 when it would pass end. */
static uint32_t from_top(uint32_t room, uint32_t end)
{
    uint32_t a = heap_top + HDR;
    if ((uint64_t)heap_top + HDR + room > (uint64_t)end) return 0;
    heap_top += HDR + room;
    HEAP_GREW();
    return a;
}

static void large_remove(uint32_t i)
{
    memmove(large + i, large + i + 1, (large_count - i - 1) * sizeof *large);
    large_count--;
}

static void large_insert(uint32_t addr, uint32_t room)
{
    uint32_t lo = 0, hi = large_count;
    while (lo < hi) {
        uint32_t mid = (lo + hi) / 2;
        if (large[mid].addr < addr) lo = mid + 1; else hi = mid;
    }
    /* Join with the free block after it and the free block before it. */
    if (lo < large_count && addr + room + HDR == large[lo].addr) {
        room += HDR + large[lo].room;
        large_remove(lo);
    }
    if (lo > 0 && large[lo - 1].addr + large[lo - 1].room + HDR == addr) {
        lo--;
        addr = large[lo].addr;
        room += HDR + large[lo].room;
        large_remove(lo);
    }
    if (addr + room == heap_top) {              /* the last block: give it back to the top */
        heap_top = addr - HDR;
        return;
    }
    if (large_count == large_cap) {
        large_cap = large_cap ? large_cap * 2 : 256;
        large = realloc(large, large_cap * sizeof *large);
    }
    memmove(large + lo + 1, large + lo, (large_count - lo) * sizeof *large);
    large[lo].addr = addr;
    large[lo].room = room;
    large_count++;
    h_set(addr, 0, room, MAGIC_FREE);
}

/* The best fitting large span for room, split when the rest is large enough, or 0. */
static uint32_t best_fit(uint32_t *room)
{
    uint32_t best = UINT32_MAX;
    for (uint32_t i = 0; i < large_count; i++)
        if (large[i].room >= *room && (best == UINT32_MAX || large[i].room < large[best].room)) best = i;
    if (best == UINT32_MAX) return 0;
    uint32_t a = large[best].addr, have = large[best].room;
    large_remove(best);
    if (have - *room > SMALL_MAX + HDR) large_insert(a + *room + HDR, have - *room - HDR);   /* the rest stays free */
    else *room = have;
    return a;
}

static int span_by_addr(const void *x, const void *y)
{
    uint32_t a = ((const Span *)x)->addr, b = ((const Span *)y)->addr;
    return a < b ? -1 : a > b;
}

/* 1 when a free small block waits in a list. */
static int small_waiting(void)
{
    for (uint32_t c = 1; c < SMALL_CLASSES; c++)
        if (small_free[c]) return 1;
    return 0;
}

/* The end of the arena: every free small block joins the large spans, sorted by address, neighbours joined (a span
 * at the top goes back to the top). It runs each time the arena is full while small blocks wait in their lists, so
 * the blocks freed after a join join at the next. */
static void join_small(void)
{
    uint32_t n = 0, cap = large_count + 4096, blocks = 0;
    Span *v = malloc(cap * sizeof *v);
    if (!v) rt_fatal("heap full");
    for (uint32_t c = 1; c < SMALL_CLASSES; c++) {
        for (uint32_t a = small_free[c]; a; a = rd32(g_mem, a)) {
            if (n == cap) {
                cap *= 2;
                Span *w = realloc(v, cap * sizeof *v);
                if (!w) rt_fatal("heap full");
                v = w;
            }
            v[n].addr = a;
            v[n++].room = c * 16u;
            blocks++;
        }
        small_free[c] = 0;
    }
    uint32_t spans = large_count;
    if (n + large_count + 1 > cap) {
        Span *w = realloc(v, (n + large_count + 1) * sizeof *v);
        if (!w) rt_fatal("heap full");
        v = w;
    }
    memcpy(v + n, large, large_count * sizeof *v);
    n += large_count;
    if (carve_room >= 16) {                     /* the rest of the current span joins too (cap holds one more) */
        v[n].addr = carve_addr;
        v[n++].room = carve_room;
    }
    carve_room = 0;
    qsort(v, n, sizeof *v, span_by_addr);
    uint32_t m = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (m && v[m - 1].addr + v[m - 1].room + HDR == v[i].addr) v[m - 1].room += HDR + v[i].room;
        else v[m++] = v[i];
    }
    if (m && v[m - 1].addr + v[m - 1].room == heap_top) heap_top = v[--m].addr - HDR;
    uint64_t bytes = 0;
    for (uint32_t i = 0; i < m; i++) {
        h_set(v[i].addr, 0, v[i].room, MAGIC_FREE);
        bytes += v[i].room;
    }
    free(large);
    large = v;
    large_count = m;
    large_cap = n > m ? n : m;
    joined = 1;
    /* The first three joins, then one in 100: near a full arena a join can come at many allocations. */
    static uint32_t joins;
    if (++joins <= 3 || joins % 100 == 0)
        fprintf(stderr, "recomp: heap: the end of the arena (%u MB), join %u: %u free small blocks and %u spans joined "
                "into %u spans (%u MB free)\n", (heap_end() - RT_HEAP_BASE) >> 20, joins, blocks, spans, m,
                (uint32_t)(bytes >> 20));
}

/* After the join: a block of room cut from the current span. When the current span has no room left, its rest goes
 * back to the spans and the next span with the room becomes the current one: next fit, from where the last search
 * stopped, so a game with many spans does not walk them all at each change. 0 when no span has the room. */
static uint32_t carve_next;                     /* the index in large where the next search starts */

static uint32_t carve(uint32_t *room)
{
    if (carve_room < *room) {
        if (carve_room >= 16) large_insert(carve_addr, carve_room);
        carve_room = 0;
        uint32_t found = UINT32_MAX;
        for (uint32_t k = 0; k < large_count && found == UINT32_MAX; k++) {
            uint32_t i = (carve_next + k) % large_count;
            if (large[i].room >= *room) found = i;
        }
        if (found == UINT32_MAX) return 0;
        carve_addr = large[found].addr;
        carve_room = large[found].room;
        large_remove(found);
        carve_next = found;
    }
    uint32_t a = carve_addr;
    if (carve_room - *room >= HDR + 16) {
        carve_addr = a + *room + HDR;
        carve_room -= *room + HDR;
    } else {
        *room = carve_room;
        carve_room = 0;
    }
    return a;
}

static uint32_t list_alloc(uint32_t size, uint32_t want)
{
    uint32_t room = ((want ? want : 1) + 15) & ~15u;
    uint32_t a;
    if (room <= SMALL_MAX) {
        uint32_t c = room / 16;
        a = small_free[c];
        if (a) small_free[c] = rd32(g_mem, a);
        if (!a && joined) a = carve(&room);
        if (!a) a = from_top(room, small_end());
    } else {
        a = best_fit(&room);
        if (!a) a = from_top(room, heap_end());
        if (!a && joined) a = carve(&room);
    }
    if (!a && small_waiting()) {                    /* the end of the arena: join, then try again */
        join_small();
        a = room > SMALL_MAX ? best_fit(&room) : 0;
        if (!a) a = carve(&room);
    }
    if (!a && room <= SMALL_MAX) a = carve(&room);  /* a small block before the first join: from a span */
    if (!a) a = from_top(room, heap_end());         /* the last eighth, when nothing else has the room */
    if (!a && want > size) return list_alloc(size, size);
    if (!a) {
        uint32_t most = carve_room;
        for (uint32_t i = 0; i < large_count; i++)
            if (large[i].room > most) most = large[i].room;
        fprintf(stderr, "recomp: heap full: a block of %u bytes (caller %#x), the largest free span %u bytes, %u spans, "
                "free below the top %u MB, top %u MB\n", size, cpu.esp ? rd32(g_mem, cpu.esp) : 0, most, large_count,
                rt_heap_free_bytes() >> 20, (heap_top - RT_HEAP_BASE) >> 20);
        rt_fatal("heap full");
    }
    h_set(a, size, room, MAGIC_USED);
    return a;
}

static void list_free(uint32_t a)
{
    if (a < RT_HEAP_BASE + HDR || a >= heap_top || h_state(a) != MAGIC_USED) {
        /* The CRT free 0x9452ca keeps its frame in ebp: its caller is at [ebp + 4]. The words of the stack that
         * look like return addresses of the game's code follow, for the search of the caller's callers. */
        fprintf(stderr, "recomp: free of %#x (block state %#x): caller %#x, stack:", a,
                a >= RT_HEAP_BASE + HDR && a < heap_top ? h_state(a) : 0, rd32(g_mem, cpu.ebp + 4));
        for (uint32_t k = 0, n = 0; k < 256 && n < 16; k++) {
            uint32_t v = rd32(g_mem, cpu.esp + 4 * k);
            if (v >= 0x401000u && v < 0x960000u) fprintf(stderr, " %#x", v), n++;
        }
        fprintf(stderr, "\n");
        rt_fatal("free of %#x, which is not a heap block (from %#x)", a, rd32(g_mem, cpu.esp));
    }
    uint32_t room = h_room(a);
    if (room <= SMALL_MAX) {
        h_set(a, 0, room, MAGIC_FREE);
        wr32(g_mem, a, small_free[room / 16]);
        small_free[room / 16] = a;
    } else {
        large_insert(a, room);
    }
}

uint32_t rt_alloc(uint32_t size)
{
    if (!rt_heap_bump) return list_alloc(size, size);
    uint32_t addr = heap_top;
    heap_top += ((size ? size : 1) + 15) & ~15u;
    if (heap_top > RT_HEAP_BASE + RT_HEAP_SIZE) rt_fatal("heap full");
    HEAP_GREW();
    size_put(addr, size);
    return addr;
}

uint32_t rt_alloc_zero(uint32_t size)
{
    uint32_t a = rt_alloc(size);
    memset(g_mem + a, 0, size);
    return a;
}

void rt_free(uint32_t addr)
{
    if (!addr || rt_heap_bump) return;
    list_free(addr);
}

uint32_t rt_block_size(uint32_t addr)
{
    if (rt_heap_bump) return size_get(addr);
    return h_size(addr);
}

void rt_set_block_size(uint32_t addr, uint32_t size)
{
    if (rt_heap_bump) size_put(addr, size);
    else wr32(g_mem, addr - HDR, size);
}

int rt_is_block(uint32_t addr)
{
    if (rt_heap_bump) return size_get(addr) || addr == heap_top;
    return addr >= RT_HEAP_BASE + HDR && addr < heap_top && h_state(addr) == MAGIC_USED;
}

uint32_t rt_realloc(uint32_t addr, uint32_t size, int in_place_only)
{
    if (!addr) return in_place_only ? 0 : rt_alloc(size);
    uint32_t old = rt_block_size(addr);
    if (rt_heap_bump ? size <= old : size <= h_room(addr)) {
        rt_set_block_size(addr, size);
        return addr;
    }
    if (in_place_only) return 0;
    /* After the limit a large block that grows gets half its size again as room, so that its next growth stays in
     * place: an array that grows by a little at each call otherwise needs a new hole each time. */
    uint32_t n = rt_heap_bump || !joined || size <= SMALL_MAX || size > 0x40000000u ? rt_alloc(size)
                                                                                    : list_alloc(size, size + size / 2);
    memcpy(g_mem + n, g_mem + addr, old < size ? old : size);
    rt_free(addr);
    return n;
}

uint32_t rt_alloc_str(const char *s)
{
    uint32_t n = (uint32_t)strlen(s) + 1;
    uint32_t a = rt_alloc(n);
    memcpy(g_mem + a, s, n);
    return a;
}
