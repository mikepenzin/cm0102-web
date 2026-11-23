/* Level clubs whose head-to-head order is still open (0.6.2 open item 167).
 *
 * League::update_positions_and_status 0x684cd0 (vtable +0x94) runs on each match day. It sorts the table (0x6827d0 at
 * 0x684d18), then gives each row its best place (+0x39, 0x684f07..0x684ff5) and its worst place (+0x3a,
 * 0x684ff9..0x6850cf) from the rows' points (s16 +0x19), matches played (s8 +0x11), the matches of a club (+0xa7)
 * and the points of a win (+0xc2). A row whose worst place lies in a zone gets that zone's status (+0x37) and a news
 * item (vtable +0x40, 0x686670): 0 promotion at 0x6851a5, 1 the play-off at 0x685213, 5 the title at 0x685149. A
 * status is never taken back: the promotion test takes a row of status -1 or 1 (0x68516e..0x685179), the play-off
 * test only -1 (0x685209).
 *
 * For two rows that have both played all their matches, the place tests count the lower row against the upper one
 * only while the league's tie decider (+0xe6, made by 0x682e10 from League::check_complete 0x689ad0 at 0x689ba4) is
 * set (0x684fc9..0x684fd5 for the best place, 0x6850a0..0x6850ae for the worst place). The test in that branch
 * compares the lower row's points with its own (0x68503d, 0x6850a0), so it counts every finished row below while a
 * decider exists, whatever its points, and none with no decider. With no decider the original so takes the present
 * order of two finished level clubs as final. That holds when the tie-breaks are goal difference and goals, which no
 * later match can change. It does not hold under head-to-head (tie-break 4 in +0xc5..+0xc8): the sort ranks level
 * clubs by a table of their matches against each other (0x682b30, then 0x683410), and that table holds every club on
 * those points. A club with matches left that comes to the same points or leaves them changes the table, so the order
 * of the two finished clubs can turn.
 *
 * Run set pass 3, a2026s3, I liga 2074/75 (save of leg004): places 2 and 3 ended level on 52 points, the champion
 * on 53. Both rows hold status 0 and place 4 holds status 1. One of the two was second in a three-club table, got 0,
 * then third when the table held two, where the play-off test of a row of status 0 does nothing; the other row then
 * came second and got 0 as well. PolSecond::create_play_off_stage 0x7cc290 takes the rows of status 1 into team
 * slots 0 and 2 (0x7cc300..0x7cc329), so slot 2 stayed 0, Cup::draw_round 0x51c410 moved the draw one day on each
 * day (0x51c47c..0x51c549), the I liga never completed and the Polish season change (entry 25, day 170) failed:
 * both leagues kept editions 2075 and 2074 to the end of the run. This is a cause of its own, not that of item 104
 * (a play-off made before the Ekstraklasa's spring stage ended, season_pol_playoff.c).
 *
 * Real life ranks level clubs in Poland by head-to-head first (Ekstraklasa and I liga rules), so the sort stays. The
 * place tests are what is wrong: in a season game, while two finished clubs are level and a club with matches left
 * can still end on their points exactly (level_ties_open), the call sees a decider. Then no finished club of the
 * league takes a status, a news item or a settled place (+0x39 equal to +0x3a, 0x685346) from that call: the branch
 * counts every finished row below, as it does in the original while a real decider exists. Clubs with matches left
 * are tested as before. The window ends on the day no club can still reach those points, at the latest with the
 * league's last match, and the call then gives the statuses from the final order. Readers of the statuses act only
 * on a complete league (the play-offs: season_pol_playoff.c, season_playoffs.c; the chain 0x689c20), so they see
 * the same statuses, a few days later in the news at most. The base game and
 * RECOMP_SEASON_CODE=off keep the original (Q152). */
#include "win.h"
#include "season_level_ties.h"
#include "season_names.h"

#include <stdio.h>
#include <string.h>

#define TABLE 0xb1u                 /* rows of 0x3b bytes */
#define ROW_SIZE 0x3bu
#define ROW_PLAYED 0x11u            /* s8 */
#define ROW_POINTS 0x19u            /* s16 */
#define CLUBS 0x3eu                 /* s16 */
#define MEETINGS 0x3cu              /* s16 */
#define MATCHES 0xa7u               /* s16, -1 until the first call sets it (0x684cdf..0x684cff) */
#define WIN_POINTS 0xc2u            /* s8 */
#define TIE_BREAKS 0xc5u            /* 4 bytes, 0 ends the list; 4 head-to-head (0x68284e..0x682877) */
#define TIE_DECIDER 0xe6u
#define COMPLETE 0xddu              /* the call does nothing once set (0x684d06..0x684d0e) */
#define MADE 0x45u
#define MAX_ROWS 64

static int readable(uint32_t p) { return rt_readable(p); }

static int head_to_head(uint32_t lg)
{
    for (uint32_t k = 0; k < 4 && g_mem[lg + TIE_BREAKS + k]; k++)
        if (g_mem[lg + TIE_BREAKS + k] == 4) return 1;
    return 0;
}

uint32_t season_level_ties_before(uint32_t lg)
{
    if (!season_game_changes() || !readable(lg) || rd32(g_mem, lg + TIE_DECIDER) || rd32(g_mem, lg + COMPLETE) ||
        !head_to_head(lg))
        return 0;
    uint32_t rows = rd32(g_mem, lg + TABLE);
    int n = (int16_t)rd16(g_mem, lg + CLUBS), matches = (int16_t)rd16(g_mem, lg + MATCHES);
    if (!readable(rows) || n < 3 || n > MAX_ROWS) return 0;
    if (matches == -1) {
        if (!rd32(g_mem, lg + MADE)) return 0;
        matches = (int16_t)((n - 1) * (int16_t)rd16(g_mem, lg + MEETINGS));
    }
    LevelRow t[MAX_ROWS];
    for (int i = 0; i < n; i++) {
        uint32_t r = rows + ROW_SIZE * (uint32_t)i;
        t[i].played = (int8_t)g_mem[r + ROW_PLAYED];
        t[i].points = (int16_t)rd16(g_mem, r + ROW_POINTS);
    }
    if (!level_ties_open(t, n, matches, (int8_t)g_mem[lg + WIN_POINTS])) return 0;
    /* One log line per competition record and edition (the record outlives a load, the object may not). */
    static struct { uint32_t rec; int year; } logged[32];
    uint32_t rec = rd32(g_mem, lg + 4u);
    int year = (int16_t)rd16(g_mem, lg + 0x40u), k = 0;
    while (k < 32 && logged[k].rec && !(logged[k].rec == rec && logged[k].year == year)) k++;
    if (k < 32 && !logged[k].rec) {
        fprintf(stderr, "recomp: level ties: %.51s %d: finished level clubs stay open while a club can still end on "
                        "their points\n", readable(rec) ? win_str(rec + 4u) : "?", year);
        logged[k].rec = rec, logged[k].year = year;
    } else if (k == 32) {
        memset(logged, 0, sizeof logged);
    }
    /* Any value other than 0 does: the call only tests it (0x684fcd, 0x6850a6). The league itself is a valid
     * address should anything read it as the decider. */
    wr32(g_mem, lg + TIE_DECIDER, lg);
    return 1;
}

void season_level_ties_after(uint32_t lg, uint32_t set)
{
    if (set && readable(lg) && rd32(g_mem, lg + TIE_DECIDER) == lg) wr32(g_mem, lg + TIE_DECIDER, 0);
}
