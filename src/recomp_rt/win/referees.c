/* Referee ranges outside the officials table (tools/recomp/referees.py).
 *
 * OfficialsManager (g_officials_manager [0xb74340]) keeps one block of 0x102 bytes per nation at [manager + 0xc]:
 * +4 the count and +8 the first slot of the nation's referees in the officials table ([0xae23e8], 0x2b bytes a row,
 * [0xae2390] rows), +0x10 the count of league entries and from +0x12 the entries (16 bytes: +0 first, +4 count,
 * +0xc the competition). set_up_officials 0x79bf60 builds them when a game is made: each block starts with first
 * 1000000 (0x79c11a, 0x79c135) and takes the lowest slot of the nation's officials (0x79c390..0x79c3cb). A nation
 * with fewer than 5 officials gets new ones (0x79c328), but officials_manager_add_random_officials 0x79c5c0 adds none
 * to a nation with no continent (0x79c5d3..0x79c5d8), and the count is set all the same (0x79c349). So a nation with
 * no continent at that moment keeps first 1000000 and a count of 1 to 5. The original never picks it: the search of
 * choose_match_official 0x79c7b0 for a referee of another nation of the continent skips a nation with no continent
 * (0x79c920..0x79c929).
 *
 * The season code gives some of those nations a continent later (season_intl.c: uefa_members, concacaf_members and
 * afc_members at a season point). Then that search can stop at one of them, the picker takes slot 1000000 or past
 * it (0x79ca04..0x79ca1f) and reads the busy flag [[manager + 8] + id] with the id of a row far past the table
 * (0x79ca3f): in the browser "memory access out of bounds", natively a referee from junk. The save of 25 November
 * 2026 (a 2025/26 game with changing rules on, 1,317 officials) had the six CONCACAF members that are not FIFA
 * members (Martinique, Guadeloupe, Bonaire, French Guiana, Saint-Martin, Sint Maarten) in North America with
 * first 1000000 and count 5, and stopped in a match day of September 2026.
 *
 * Two parts:
 * - rt_referee_out at 0x79ca23 (referees.py): a range that reaches past the table (or a busy flag array shorter
 *   than the table) takes the whole table, from the picked slot modulo its size. A range inside the table is not
 *   changed, so every game the original can play runs as before.
 * - rt_referee_ranges_repair at each season point after the new members and at a load (season_intl.c): a nation
 *   with a continent and a range past the table takes the range of the nation of its continent with the most
 *   referees. The save keeps it (off.dat, save_official_stats 0x79bb80).
 * Test switch: RECOMP_REFEREES=guard skips the repair, so a save with such a nation shows the picker's test
 * (tests/seasons/referees_run.py). */
#include "win.h"
#include "../rt.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OFFICIALS_MANAGER 0xb74340u
#define OFFICIAL_COUNT 0xae2390u
#define NATIONS 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u
#define BLOCKS 0xcu                     /* manager + 0xc */
#define BUSY_COUNT 0x4u                 /* manager + 4: the size of the busy flags [manager + 8] */
#define BLOCK_SIZE 0x102u
#define BLOCK_COUNT 0x4u
#define BLOCK_FIRST 0x8u

/* The rows the picker may take: the table, or fewer when the busy flags are shorter. */
static uint32_t table_size(uint32_t manager)
{
    uint32_t rows = rd32(g_mem, OFFICIAL_COUNT), busy = rd32(g_mem, manager + BUSY_COUNT);
    return busy < rows ? busy : rows;
}

/* 1 when the picker would read a row outside [0, size): it reads first + 0 .. count - 1, or first alone when the
 * count is 0 or 1 (0x79ca04..0x79ca1f, utils_random 0x90cfc0 gives 0 for -1). */
static int outside(uint32_t first, uint32_t count, uint32_t size)
{
    int32_t c = (int32_t)count;
    if (c < 0) return 1;
    return first >= size || (uint64_t)first + (uint64_t)(c > 1 ? c : 1) > size;
}

uint32_t rt_referee_out(uint32_t manager, uint32_t first, uint32_t count)
{
    if (!rt_readable(manager)) return 0;
    uint32_t size = table_size(manager);
    if (!size || !outside(first, count, size)) return 0;
    static unsigned logged;
    if (logged < 8) {
        logged++;
        fprintf(stderr, "recomp: referees: the range from slot %u (%d officials) is outside the table of %u "
                        "officials, so the referee comes from the whole table (day %d, year %d)\n",
                first, (int32_t)count, size, (int16_t)rd16(g_mem, 0xae2c90u), (int16_t)rd16(g_mem, 0xae2c92u));
    }
    return 1;
}

uint32_t rt_referee_count(uint32_t manager)
{
    return table_size(manager);
}

int rt_referee_ranges_repair(void)
{
    const char *test = getenv("RECOMP_REFEREES");
    if (test && !strcmp(test, "guard")) return 0;
    uint32_t manager = rd32(g_mem, OFFICIALS_MANAGER);
    if (!rt_readable(manager)) return 0;
    uint32_t blocks = rd32(g_mem, manager + BLOCKS), nations = rd32(g_mem, NATIONS);
    int32_t count = (int32_t)rd32(g_mem, NATION_COUNT);
    uint32_t size = table_size(manager);
    if (!rt_readable(blocks) || !rt_readable(nations) || count <= 0 || count > 10000 || !size) return 0;
    int done = 0;
    for (int32_t id = 0; id < count; id++) {
        uint32_t nation = nations + (uint32_t)id * NATION_SIZE, block = blocks + (uint32_t)id * BLOCK_SIZE;
        uint32_t continent = rd32(g_mem, nation + NATION_CONTINENT);
        if (!rt_readable(continent)) continue;
        if (!outside(rd32(g_mem, block + BLOCK_FIRST), rd32(g_mem, block + BLOCK_COUNT), size)) continue;
        int32_t best = -1;
        uint32_t best_count = 0;
        for (int32_t j = 0; j < count; j++) {
            uint32_t other = rd32(g_mem, nations + (uint32_t)j * NATION_SIZE + NATION_CONTINENT);
            uint32_t b = blocks + (uint32_t)j * BLOCK_SIZE, n = rd32(g_mem, b + BLOCK_COUNT);
            if (j == id || !rt_readable(other) || rd32(g_mem, other) != rd32(g_mem, continent)) continue;
            if ((int32_t)n <= 0 || outside(rd32(g_mem, b + BLOCK_FIRST), n, size) || n <= best_count) continue;
            best = j;
            best_count = n;
        }
        if (best < 0) continue;     /* no referee in the continent: the picker's test takes the whole table */
        uint32_t from = blocks + (uint32_t)best * BLOCK_SIZE;
        wr32(g_mem, block + BLOCK_COUNT, best_count);
        wr32(g_mem, block + BLOCK_FIRST, rd32(g_mem, from + BLOCK_FIRST));
        fprintf(stderr, "recomp: referees: %.40s takes the referees of %.40s (%u officials from slot %u): its own "
                        "range was outside the table of %u\n",
                win_str(nation + NATION_NAME), win_str(nations + (uint32_t)best * NATION_SIZE + NATION_NAME),
                best_count, rd32(g_mem, from + BLOCK_FIRST), size);
        done++;
    }
    return done;
}
