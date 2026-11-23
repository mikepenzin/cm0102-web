/* The League of Ireland First Division at its real count (0.7.2 league sizes, slice 13; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md). IreFirst counts the clubs of its data (tools/recomp/league_sizes.py
 * DATA_COUNT), so the engine gives it the template of that count and the row's meetings (season_sizes.c, the wrap of
 * create_schedule 0x634f40), and these two values let IreFirst::create_table 0x635910 take them:
 * - the least count: the original takes only 9 to 12 (cmp ax, 9 at 0x63596f; cmp ax, 0xc at 0x635979) and returns
 *   0 for any other count, and the constructor stops at its box (ire_first.cpp, 0x6359e9). The 2012 to 2017 seasons
 *   have 8 clubs;
 * - the meetings: the original sets 4 for 9 or 10 clubs and 3 for 11 or 12 (0x635983..0x635997), after
 *   set_up_rules 0x635aa0 called create_schedule (0x635b21), whose template the engine made for the row's meetings.
 *   An 8-club season would get 3 meetings on a template of 4.
 *   A row with no meetings value (the unknown future rows) takes the class's meetings for the count
 *   (season_sizes_irl_class_meetings), so the template and the table agree with the original.
 * Outside a season game with the sizes on both return the original's value; the meetings also in a season whose
 * template had no room for the count. */
#include "win.h"
#include "season_sizes.h"

#include <stdio.h>
#include <string.h>

#define COMP_STAGES 0xcu        /* the stage array (IreFirst::IreFirst 0x634cb4..0x634cbe: malloc 4) */
#define COMP_STAGE_COUNT 0x2cu  /* s32: IreFirst::IreFirst sets 1 (0x634cad) */
#define COMP_STAGE_NOW 0x30u    /* s32: -1 before the play-off (IreFirst::start_new_season 0x635b96) */
#define COMP_YEAR 0x40u         /* s16: the edition */
#define COMP_MEETINGS 0x3cu     /* s16: meetings of each pair */
#define UP 0xbeu                /* s8: IreFirst::set_up_rules 0x635aa0 sets 1 (0x635add) */
#define UP_PLAYOFF 0xbfu        /* s8: IreFirst::set_up_rules 0x635aa0 sets 1 (0x635ae3) */
#define DOWN_PLAYOFF 0xc0u      /* s8: IrePrm::set_up_rules 0x63a900 sets 1 (0x63a957) */
#define MOST_FIRST 12           /* IreFirst::create_table 0x635910: cmp ax, 0xc at 0x635979 */

static int readable(uint32_t p) { return rt_readable(p); }   /* as season_sizes.c */
#define MOST_PLACES 30          /* season_sizes.c set_places: the most places of a row */
#define LEAST_CLUBS 2u          /* season_sizes.c MIN_CLUBS: the least count that the engine plans */

uint32_t rt_ire_first_least(uint32_t obj, uint32_t original)
{
    /* Not only a planned template: the club moves (season_clubs.c data_class_count) leave fewer than 9 whenever the
     * sizes are on, and a season whose template had no room must still make its table. */
    (void)obj;
    return season_sizes_on() ? LEAST_CLUBS : original;
}

uint32_t rt_ire_first_meetings(uint32_t obj, uint32_t original)
{
    if (!season_sizes_planned(obj)) return original;
    int m = (int16_t)rd16(g_mem, obj + COMP_MEETINGS);
    return m >= 1 && m <= 4 ? (uint32_t)m : original;
}

int season_sizes_irl_class_meetings(const char *file, int n, int row_meetings)
{
    if (!file || row_meetings) return 0;
    if (!strcmp(file, "ire_first")) return n == 9 || n == 10 ? 4 : 3;
    /* IrePrm::create_table 0x63a740: 4 meetings for 10 clubs (0x63a7be), 3 for 12 (0x63a7c6); grown from gap row 2. */
    if (!strcmp(file, "ire_prm")) return n == 10 ? 4 : 3;
    return 0;
}

/* Gap 43: IrePrm::create_table 0x63a740 sets 3 relegation places for any table of 12 clubs (mov byte [esi + 0xc1], 3
 * at 0x63a8b5), and IrePrm::set_up_rules 0x63a900 sets 1 and a play-off place (0x63a957, 0x63a95d). The rows of the
 * Premier Division give the direct places in "down" and the play-off place apart ("1 (pos 12)" and "pos 11:
 * relegation play-off" in 2011 to 2015). With the sizes on, the 12-club branch takes the row's direct places, so in
 * 2004 to 2006 and 2011 to 2015 the 12th goes down and the 11th plays off, as in real life. The play-off place
 * (0x63a8ae) follows the row too (item 251, below). set_places runs in the set_up_rules wrap, before create_table
 * (0x639c3c then 0x639c77; vtable +0x8c at 0x63a9ff then 0x63aa3d), so for a 12-club table these stores decide
 * +0xc0 and +0xc1. */
uint32_t rt_ire_prm_down(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    int down = season_sizes_row_down(obj, "ire_prm");
    return down >= 0 && down <= MOST_PLACES ? (uint32_t)down : original;
}

/* Open item 251: the seasons whose Premier Division row has no play-off ("none": rows wal-nir-irl-66 2003/04,
 * 166 2007/08, 391 2016/17; real life 2004, 2008 and 2017: the direct places only, then 10 to 12 or 12 to 10). An
 * edition is a calendar year and plays the row of its real season (season_sizes_edition), so these are editions 2004,
 * 2008 and 2017. Edition 2003 also reads row 2003 (row 2002 holds the real 2002-03 and 2003), but its real season had
 * a play-off, so "none" counts only in the row's own real season (season_sizes_row_playoffs). The
 * original always plays one: IrePrm::set_up_rules 0x63a900 and the 12-club branch of create_table 0x63a740 set the
 * relegation play-off place +0xc0 to 1 (0x63a957, 0x63a8ae); IreFirst::set_up_rules 0x635aa0 sets the promotion
 * play-off place +0xbf to 1 (0x635ae3), and IreFirst::IreFirst 0x634c40 the stage count +0x2c to 1 (0x634cad), so
 * League::update_for_date 0x6847c0 calls IreFirst::advance_stage 0x635cf0 when the table is complete (0x6849d1..
 * 0x6849e0). That waits for the Premier Division and builds the stage of its play-off club against the First Division
 * one (create_promotion_playoff_stage 0x635dc0). In a season of the rows with no play-off both places are 0 and the
 * stage count is 0, so the season ends at its table (0x6849da, the plain league's way). The play-off of the row is
 * the Premier Division's, read for both classes in the same edition (both start_new_season add 1 to the year). */
static int no_playoff(uint32_t obj)
{
    const char *p = season_sizes_row_playoffs(obj, "ire_prm");
    return p && !strcmp(p, "none");
}

uint32_t rt_ire_prm_playoff(uint32_t obj, uint32_t original)
{
    if (!season_sizes_on()) return original;
    return no_playoff(obj) ? 0u : original;
}

void season_sizes_irl_rules(uint32_t obj, const char *file)
{
    if (!file) return;
    /* With the sizes off the original's places stand. Only a save of a season with no play-off, loaded with the sizes
     * off, holds the stage count 0: it gets the class's 1 back below (no_playoff is 0). */
    if (!season_sizes_on() && strcmp(file, "ire_first")) return;
    if (!strcmp(file, "ire_prm")) {
        if (no_playoff(obj)) g_mem[obj + DOWN_PLAYOFF] = 0;
        return;
    }
    if (strcmp(file, "ire_first")) return;
    int none = season_sizes_on() && no_playoff(obj);
    if (none) g_mem[obj + UP_PLAYOFF] = 0;
    /* Item 251b: the row's "up" is the direct places, and the play-off with the Premier Division is apart ("2 (pos
     * 1,2)" and "pos 3: promotion/relegation play-off" in row 2010, the real 2011). IreFirst::set_up_rules writes 1
     * up and 1 play-off place (+0xbe at 0x635add, +0xbf at 0x635ae3), and set_places (season_sizes.c) reads a want
     * of 2 as 1 direct and the play-off, so in 2011 one club went up for the real two and the 2012 Premier Division
     * kept 10. The play-off takes the row of status 1 (IreFirst::create_promotion_playoff_stage 0x635dc0, cmp byte
     * [row + 0x37], 1 at 0x635e37), the place below the direct ones, so 2 direct places leave the 3rd in it. */
    int up = season_sizes_row_up(obj, file);
    if (up >= 0 && up <= MOST_PLACES) g_mem[obj + UP] = (uint8_t)up;
    /* Only before the play-off stage: the constructor and start_new_season call set_up_rules with +0x30 -1. A season
     * after one with no play-off gets the class's 1 back. A save of such a season holds +0x2c 0, and League::load
     * 0x687b10 then makes no stage array (0x688e44..0x688e5b), so it is made here as the constructor does. */
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) != -1) return;
    if (!none && !readable(rd32(g_mem, obj + COMP_STAGES))) {
        uint32_t arr = season_sizes_game_malloc(4);
        if (!arr) return;
        wr32(g_mem, arr, 0);
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    wr32(g_mem, obj + COMP_STAGE_COUNT, none ? 0u : 1u);
    if (none)
        fprintf(stderr, "recomp: league sizes: ire_first %d: no play-off with the Premier Division\n",
                (int16_t)rd16(g_mem, obj + COMP_YEAR));
}

/* The counts that the create_table of an Irish class takes: IrePrm 10 or 12 (0x63a79f..0x63a7a9), IreFirst from
 * LEAST_CLUBS (rt_ire_first_least) to 12 (0x635979). */
int season_sizes_irl_takes(const char *file, int n)
{
    if (!file) return 0;
    if (!strcmp(file, "ire_prm")) return n == 10 || n == 12;
    if (!strcmp(file, "ire_first")) return n >= (int)LEAST_CLUBS && n <= MOST_FIRST;
    return 0;
}
