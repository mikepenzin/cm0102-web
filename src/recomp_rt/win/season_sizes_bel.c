/* The Belgian Second Division at its real sizes: the row that the third tier's promotion play-off reads (0.7.2 league
 * sizes, slice 10, D22; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * The original. BelThird::advance_stage 0x423640 counts its stage to 7 (`cmp eax, 7` at 0x4236e3) and calls
 * BelThird::create_promotion_playoff_stage 0x423880 (0x4236f1). That function takes the division above by its id at
 * BelThird +0x1c (0x423a6a), its table (+0xb1, 0x423a78), and the club of row 15 (`mov edx, dword ptr [ecx + 0x375]`
 * at 0x423a80, 0x375 = 15 x 0x3b rows): the 16th of 18, the third from the bottom, into the play-off with the third
 * tier's clubs. With 19 clubs (2007/08 to 2009/10) the real league sent its 17th (sizes-check-north, BelThird row
 * 15), and with 8 to 17 row 15 is the wrong club or past the table. So the row is the count less 3 (INSNS 0x423a80 of
 * tools/recomp/league_sizes.py), which is row 15 for 18 clubs. This replaces the floor of 16 that kept row 15 in the
 * table (open item 113, ROW_READS). The wrap of 0x423880 in season_bel_second.c (open item 124) puts a third-tier
 * club in the same row: it asks season_sizes_bel_third_row.
 *
 * The base game and RECOMP_LEAGUE_SIZES=off: row 15. */
#include "win.h"
#include "season_sizes.h"

#define LEAGUES 0xadadfcu           /* the competition objects by id */
#define THIRD_ABOVE 0x1cu           /* BelThird: the id of the division above (0x423a6a) */
#define COMP_CLUBS 0x3eu            /* s16 */
#define ROW_SIZE 0x3bu
#define CLASS_ROW 15                /* 0x423a80: + 0x375 */
#define FROM_BOTTOM 3

static int readable(uint32_t p) { return rt_readable(p); }

int season_sizes_bel_third_row(uint32_t second)
{
    if (!season_sizes_on() || !readable(second)) return CLASS_ROW;
    int n = (int16_t)rd16(g_mem, second + COMP_CLUBS);
    return n > FROM_BOTTOM && n <= 60 ? n - FROM_BOTTOM : CLASS_ROW;
}

uint32_t rt_bel_third_row(uint32_t third, uint32_t original)
{
    if (!season_sizes_on() || !readable(third)) return original;
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, third + THIRD_ABOVE);
    if (!readable(leagues) || id < 0 || id > 0xffff) return original;
    uint32_t second = rd32(g_mem, leagues + 4u * (uint32_t)id);
    if (!readable(second)) return original;
    return (uint32_t)season_sizes_bel_third_row(second) * ROW_SIZE;
}
