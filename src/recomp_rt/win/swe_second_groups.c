/* The Swedish Second Division groups take the clubs that the data holds in them (open item 224).
 *
 * SweSecond (swe_second.cpp, the extra division of Sweden) is made at a new game by its constructor 0x8916f0
 * (called at 0x837ca5). Before it, SwePrm's constructor 0x88fb10 moves each club of the six group records into the
 * Swedish Second Division record (club +0x57 = [0x9cf7c8]) and keeps the group in club +0x60. The constructor then
 * makes the North group with 0x8921b0, which sets the class count +0x3e to 12 (0x8921c2) and needs 12 North clubs
 * (else swe_second.cpp 80), and the other five groups with SweSecond::create_group_stage 0x892670 (thiscall, the
 * group 0 to 4; ret 4). That function takes the group record from [0x9cf5fc], [0x9cf604], [0x9cf600], [0x9cf608] or
 * [0x9cf5f4] (0x8926c8..0x892704), reads the count from the class +0x3e (0x892693), collects the clubs of the
 * division whose +0x60 record has that id (0x89277a..0x89281a), stops at swe_second.cpp 643 (0x283) for a club
 * over the count and at swe_second.cpp 656 (0x290, 0x89283f..0x89286d) for fewer, and makes the stage with the
 * count (0x8928f2, the second argument of 0x68b100).
 *
 * Real life: the 2001 Division 2 had six groups, five of 12 clubs and Östra Svealand of 11 (20 matches a club;
 * https://en.wikipedia.org/wiki/2001_Division_2_(Swedish_football)). The 3.9.60 data holds those 11 in East
 * Svealand, so a new game on the default 2001/02 data (3.9.60 since 0.6.1) with the Swedish extra division
 * stopped at swe_second.cpp 656. The 3.9.68 data holds 12 in each group.
 *
 * So a group with fewer clubs than the class count takes the clubs that the data holds in it: for the call, the
 * class count is the group's club count, and after it the class count is 12 again (the North group's, which the
 * class keeps for its own table). The later readers take each stage's own count (+0x3e of the stage: 0x892a10,
 * 0x892bb0, 0x893470), and a smaller stage fits their tables: 0x893470 sorts each stage in a block of 12 pointers
 * (0x30 bytes, 0x8934b4), and 0x891a60 gives each group 22 dates (0x891af8), the 22 rounds of 11 or 12 clubs.
 * A group with more clubs than the count does not fit them (13 clubs need 13 pointers and 26 rounds), so it keeps
 * the original's stop at line 643. The wrap also runs at the season rebuild (0x892410 calls 0x892670 at 0x8925a4),
 * with the same test. With 12 clubs in the group nothing changes. One log line each time a group takes another
 * count.
 *
 * The 3.9.65 data holds 13 clubs in East Svealand, so a new game with the Swedish extra division stopped at
 * swe_second.cpp 643 (the box from 0x8927ff). A 2001/02 game plays the Swedish season of 2002 first (the start
 * screen names "Sweden 2002"), and the 3.9.65 groups hold the 2002 clubs. The 13th club is Katrineholms SK
 * (club +0x5b, the previous division, is -2 in the data). The real 2002 Division 2 Östra
 * Svealand had 12 clubs without it (https://en.wikipedia.org/wiki/2002_Division_2_(Swedish_football)), and
 * Katrineholms SK FK played the 2002 Division 3 Östra Svealand
 * (https://en.wikipedia.org/wiki/2002_Division_3_(Swedish_football)). SwePrm's constructor 0x88fb10 puts each
 * club of the six group records in the Second Division record and keeps the group in club +0x60 (0x88fbb0..0x88fc1f),
 * and puts every other Swedish club in the Lower Division record [0x9cf6ac]. So before that loop, with the
 * extra division flag of Sweden (nation +0x11c bit 0x04, read at 0x837add) and more clubs in a group than the
 * class count, a club that REAL_DIVISION names in that group takes the division of real life: its club +0x57
 * becomes the record of that division, and the loop then puts it in the Lower Division with the other Third
 * Division clubs. One log line each time. Without the extra division the Second Division groups are not played,
 * and the game stays as before. */
#include "win.h"
#include "season_swe_groups.h"

#include <stdio.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club), clubs then national teams */
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u      /* the last 2 x nations rows are national teams */
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u
#define CLUB_ZONE 0x60u
#define OBJ_DIVISION 0x04u          /* the division record of the class (0x8916f0: in_ECX[1]) */
#define OBJ_CLUBS 0x3eu
#define COMP_NAME 0x04u
#define NATIONS 0x00ae23a8u         /* the nation table, 0x122 bytes a nation */
#define NATION_SIZE 0x122u
#define NATION_SELECTED 0x11cu      /* bit 0x04: the extra division (create_swedish_comps 0x8379f0 at 0x837add) */
#define EXTRA_DIVISION 0x04u
#define KEY_SWEDEN 0x009cf4a8u
#define GROUP_CLUBS 12              /* the class count (0x8921c2) */

static int readable(uint32_t p) { return rt_readable(p); }

/* The key of group 0 to 4, in the order of 0x8926c8..0x892704. */
static uint32_t group_key(int group)
{
    static const uint32_t keys[5] = {0x009cf5fcu, 0x009cf604u, 0x009cf600u, 0x009cf608u, 0x009cf5f4u};
    return keys[group >= 0 && group < 4 ? group : 4];
}

void o_00892670(void);
RT_WRAP(00892670, swe_second_group)
{
    uint32_t self = cpu.ecx;
    int group = (int8_t)(rt_arg(0) & 0xffu);
    if (season_swe_groups_skip_stage(self, group)) {   /* issue 106: two groups, only stage 0 exists */
        rt_ret_stdcall(0, 1);
        return;
    }
    uint32_t division = readable(self) ? rd32(g_mem, self + OBJ_DIVISION) : 0;
    uint32_t table = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATION_COUNT);
    uint32_t key = rd32(g_mem, group_key(group));
    int held = 0;
    uint32_t zone_at = 0;
    for (int32_t i = 0; division && readable(table) && i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, zone = rd32(g_mem, club + CLUB_ZONE);
        if (rd32(g_mem, club + CLUB_DIVISION) != division || !readable(zone) || rd32(g_mem, zone) != key) continue;
        held++;
        zone_at = zone;
    }
    int16_t was = division ? (int16_t)rd16(g_mem, self + OBJ_CLUBS) : 0;
    if (!division || held <= 0 || held >= was) {
        o_00892670();
        return;
    }
    fprintf(stderr, "recomp: swe_second: %s holds %d clubs in the data, the class takes %d: the group takes its %d\n",
            win_str(zone_at + COMP_NAME), held, was, held);
    wr16(g_mem, self + OBJ_CLUBS, (uint16_t)held);
    o_00892670();
    if (!rt_unwinding) wr16(g_mem, self + OBJ_CLUBS, (uint16_t)was);
}

/* A club that the data holds in a Second Division group, with the division where real life had it. */
static const struct { const char *club; uint32_t group_key; const char *division; } REAL_DIVISION[] = {
    {"Katrineholms SK", 0x009cf5fcu, "Swedish Third Division East Svealand"},   /* 3.9.65: 2002 Division 3 */
};

static int sweden_extra(void)
{
    uint32_t nations = rd32(g_mem, NATIONS);
    int32_t id = (int32_t)rd32(g_mem, KEY_SWEDEN);
    if (!readable(nations) || id < 0) return 0;
    return (g_mem[nations + (uint32_t)id * NATION_SIZE + NATION_SELECTED] & EXTRA_DIVISION) != 0;
}

/* The division record of a club, if it is readable and its id is the id that a key holds. */
static int in_division(uint32_t club, uint32_t key)
{
    uint32_t d = rd32(g_mem, club + CLUB_DIVISION);
    return readable(d) && rd32(g_mem, d) == rd32(g_mem, key);
}

void o_0088fb10(void);
RT_WRAP(0088fb10, swe_prm_groups)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int32_t count = (int32_t)rd32(g_mem, CLUB_COUNT) - 2 * (int32_t)rd32(g_mem, NATION_COUNT);
    for (size_t r = 0; readable(table) && sweden_extra() && r < sizeof REAL_DIVISION / sizeof REAL_DIVISION[0]; r++) {
        int held = 0;
        uint32_t club_at = 0, target = 0;
        for (int32_t i = 0; i < count; i++) {
            uint32_t club = table + (uint32_t)i * CLUB_SIZE, d = rd32(g_mem, club + CLUB_DIVISION);
            if (in_division(club, REAL_DIVISION[r].group_key)) {
                held++;
                if (!strcmp(win_str(club + 4), REAL_DIVISION[r].club)) club_at = club;
            } else if (!target && readable(d) && !strcmp(win_str(d + COMP_NAME), REAL_DIVISION[r].division)) {
                target = d;
            }
        }
        if (held <= GROUP_CLUBS || !club_at || !target) continue;
        fprintf(stderr, "recomp: swe_second: %s plays the %s (real life), the group holds %d clubs, not %d\n",
                REAL_DIVISION[r].club, REAL_DIVISION[r].division, held - 1, held);
        wr32(g_mem, club_at + CLUB_DIVISION, target);
    }
    o_0088fb10();
}
