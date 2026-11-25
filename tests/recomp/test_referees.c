/* Referee ranges outside the officials table (src/recomp_rt/win/referees.c, tools/recomp/referees.py at 0x79ca23).
 * The officials manager, its nation blocks and the nation table in a small memory, with the numbers of the save of
 * 25 November 2026: 1,317 officials, and six members of North America whose blocks hold first 1000000 and count 5.
 * Checks: a range inside the table is not changed (first 0, count 1, the last row, count 0); a range past the table
 * is; the repair gives such a nation the range of its continent with the most referees, leaves a nation with no
 * continent and every good block alone, and does nothing a second time. ctest referees. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>

Cpu cpu;
uint8_t *g_mem;

#define MEM 0x1000000u
#define MANAGER 0x100000u
#define BLOCKS 0x200000u
#define NATIONS 0x300000u
#define CONTINENTS 0x400000u            /* 0xc6 bytes a continent, +0 its id */
#define ROWS 1317u
#define N_NATIONS 6

static int failures;

static void check(int ok, const char *text)
{
    if (!ok) {
        failures++;
        fprintf(stderr, "FAIL: %s\n", text);
    }
}

static uint32_t block(int id) { return BLOCKS + (uint32_t)id * 0x102u; }

static void nation(int id, const char *name, int continent, uint32_t count, uint32_t first)
{
    uint32_t n = NATIONS + (uint32_t)id * 0x122u;
    snprintf((char *)g_mem + n + 4, 40, "%s", name);
    wr32(g_mem, n + 0x71u, continent < 0 ? 0u : CONTINENTS + (uint32_t)continent * 0xc6u);
    wr32(g_mem, block(id) + 4u, count);
    wr32(g_mem, block(id) + 8u, first);
}

int main(void)
{
    g_mem = calloc(1, MEM);
    if (!g_mem) return 2;
    for (int c = 0; c < 6; c++) wr32(g_mem, CONTINENTS + (uint32_t)c * 0xc6u, (uint32_t)c);
    wr32(g_mem, 0xb74340u, MANAGER);
    wr32(g_mem, MANAGER + 4u, ROWS);
    wr32(g_mem, MANAGER + 0xcu, BLOCKS);
    wr32(g_mem, 0xae2390u, ROWS);
    wr32(g_mem, 0xae23a8u, NATIONS);
    wr32(g_mem, 0xae2350u, N_NATIONS);
    nation(0, "Mexico", 3, 40, 900);
    nation(1, "Martinique", 3, 5, 1000000);
    nation(2, "Jamaica", 3, 12, 950);
    nation(3, "Zaire", -1, 5, 1000000);
    nation(4, "Spain", 2, 60, 100);
    nation(5, "Guadeloupe", 3, 5, 1000000);

    check(!rt_referee_out(MANAGER, 0, 1), "slot 0, one official, is inside");
    check(!rt_referee_out(MANAGER, ROWS - 1, 1), "the last row is inside");
    check(!rt_referee_out(MANAGER, ROWS - 1, 0), "count 0 reads the first slot alone");
    check(!rt_referee_out(MANAGER, 900, 40), "Mexico's range is inside");
    check(!rt_referee_out(MANAGER, 1300, 17), "a range to the end of the table is inside");
    check(rt_referee_out(MANAGER, 1300, 18), "a range one row past the end is outside");
    check(rt_referee_out(MANAGER, 1000000, 5), "first 1000000 is outside");
    check(rt_referee_out(MANAGER, 1000000 + 3, 0), "a start past the table with count 0 is outside");
    check(rt_referee_out(MANAGER, 10, (uint32_t)-1), "a negative count is outside");
    check(rt_referee_count(MANAGER) == ROWS, "the picker may take every row");
    wr32(g_mem, MANAGER + 4u, 1000);
    check(rt_referee_out(MANAGER, 1100, 1), "a row past the busy flags is outside");
    check(rt_referee_count(MANAGER) == 1000, "the busy flags bound the rows");
    wr32(g_mem, MANAGER + 4u, ROWS);
    check(!rt_referee_out(0, 1000000, 5), "no manager: no change");

    check(rt_referee_ranges_repair() == 2, "two members of North America are repaired");
    check(rd32(g_mem, block(1) + 4u) == 40 && rd32(g_mem, block(1) + 8u) == 900, "Martinique takes Mexico's range");
    check(rd32(g_mem, block(5) + 4u) == 40 && rd32(g_mem, block(5) + 8u) == 900, "Guadeloupe takes Mexico's range");
    check(rd32(g_mem, block(3) + 8u) == 1000000, "Zaire, with no continent, keeps its block");
    check(rd32(g_mem, block(2) + 4u) == 12 && rd32(g_mem, block(2) + 8u) == 950, "Jamaica keeps its range");
    check(rd32(g_mem, block(4) + 4u) == 60 && rd32(g_mem, block(4) + 8u) == 100, "Spain keeps its range");
    check(rt_referee_ranges_repair() == 0, "a second repair changes nothing");

    /* A continent with no referee: the block stays, the picker's test takes the whole table. */
    nation(1, "Martinique", 4, 5, 1000000);
    check(rt_referee_ranges_repair() == 0, "no nation of Oceania has referees: nothing to take");
    check(rd32(g_mem, block(1) + 8u) == 1000000, "Martinique keeps its block");

    if (failures) return 1;
    printf("referees: all checks passed\n");
    return 0;
}
