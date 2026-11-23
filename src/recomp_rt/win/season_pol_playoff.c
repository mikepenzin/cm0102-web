/* The I liga's promotion play-off waits for the Ekstraklasa's last stage (0.6.2 open item 104).
 *
 * PolFirst (the Ekstraklasa) plays two stages: the groups (its own table, 14 rounds to November) and the spring stage
 * (stage 1, [+0xc][1], 16 clubs). The spring stage marks its places 13 and 14 for the relegation play-off (status +0x37
 * 2 in the stage's table rows, League::update_positions_and_status 0x684cd0). PolSecond (the I liga) plays its own
 * table and then one stage, the play-off of its places 3 and 4 against those two clubs.
 *
 * League::update_for_date 0x6847c0 calls the I liga's advance_stage (vtable +0x28, 0x6849e0) on each day that its own
 * table and built stages are complete (check_complete at 0x684994 and 0x6849be) and a stage is still to come.
 * PolSecond::advance_stage 0x7cc1c0 tests the Ekstraklasa ([0x9cf858]) with check_complete(0, 1) (0x7cc24a..0x7cc253):
 * the first argument 0 skips its stages (League::check_complete 0x689ad0, the stage test 0x689bb2..0x689be8 runs only
 * for 1), so the test passes from November, when the groups end. It then calls update_positions_and_status of the
 * spring stage (vtable +0x94 at 0x7cc270) and makes the play-off (create_play_off_stage 0x7cc290). That function
 * takes the I liga rows with status 1 and the spring stage rows with status 2 into four team slots (0x7cc300..0x7cc375)
 * and gives them to the Cup_Stage (0x522e00). When the spring stage has not ended, only the places already decided
 * carry status 2, so a slot stays 0. Cup::draw_round 0x51c410 then moves the draw one day on each day while a slot of
 * the round is empty (0x51c47c..0x51c549), so the play-off is never drawn, the I liga is never complete, the
 * Ekstraklasa's chain test (Comp::all_stages_complete 0x48ce10) fails on the Polish update day (entry 25, day 170) and
 * both leagues keep their edition for a year (league-stall).
 *
 * In the base data the spring stage ends about a week before the I liga's last round (day 136 against day 143 in
 * 2200), so the test of the groups alone was enough. In the date jump to 2198 (run set pass 2, jump2198, 2199/00) an
 * Ekstraklasa match of the spring stage came after the I liga's last round on day 143: the play-off was made that day
 * while the spring stage's row in place 13 still had status 255 (open) and only the row in place 14 had status 2, so
 * its fourth slot was 0, and its draw moved from day 144 to day 200 and on. Whether a spring match comes late depends
 * on the order of the days' matches: the same leg loaded on day 120 played its spring stage to the end on day 141.
 *
 * So in a season game the I liga's advance_stage waits, one day at a time as update_for_date calls it, until every
 * built stage of the Ekstraklasa is complete and its last stage is built (the test that check_complete(1, ...) makes,
 * read here without its side effects: check_complete sets the complete flag +0xdd and calls vtable +0x94 the first
 * time, 0x689bea..0x689c06). The play-off then takes the final places. Its dates follow from the day it is made when
 * the dates of create_schedule have passed or end on or after the Polish update day (dates_from_today below), and the
 * Polish season change waits for a play-off that still runs on its update day (season_pol_playoff_unfinished). A game
 * where the spring stage ends first is not changed: the test passes on the same day as the original's, and the
 * original's dates (draw about day 144, legs about days 150 and 154) are ahead of that day and before day 170. The
 * base game and RECOMP_SEASON_CODE=off keep the original (Q152). */
#include "win.h"
#include "season_names.h"
#include "season_pol_playoff.h"
#include "season_pol_first.h"

#include <stdlib.h>

#define COMP_OBJECTS 0x00adadfcu        /* g_comp_object_table */
#define COMP_SLOTS 0x00adadf8u
#define KEY_POL_FIRST 0x009cf858u       /* the Ekstraklasa's competition id (0x7cc245) */
#define TODAY 0x00ae2c90u               /* Date: day of the year +0, year +2 */
#define POL_SECOND_VTABLE 0x0096e4b4u   /* PolSecond (tools/seasons/make_comp_classes.py) */

static int readable(uint32_t p) { return rt_readable(p); }

/* League::check_complete 0x689ad0 of one league or stage, read only: a table (+0xb1) that is not locked (+0xb5) of a
 * league that is made (+0x45), and every row (0x3b bytes) with its played count (s8 +0x11) at the matches per club
 * (+0xa7; -1 before the first test, then (clubs - 1) * meetings, 0x689b1d..0x689b36). A league whose complete flag
 * +0xdd is set is complete (0x689b01..0x689b0f). */
static int league_complete(uint32_t lg)
{
    if (!readable(lg) || !readable(rd32(g_mem, lg + 0xb1u)) || g_mem[lg + 0xb5u] || !rd32(g_mem, lg + 0x45u))
        return 0;
    if (rd32(g_mem, lg + 0xddu)) return 1;
    int clubs = (int16_t)rd16(g_mem, lg + 0x3eu), matches = (int16_t)rd16(g_mem, lg + 0xa7u);
    if (matches == -1 && clubs > 0) matches = (int16_t)((clubs - 1) * (int16_t)rd16(g_mem, lg + 0x3cu));
    uint32_t rows = rd32(g_mem, lg + 0xb1u);
    for (int i = 0; i < clubs; i++)
        if ((int8_t)g_mem[rows + 0x3bu * (uint32_t)i + 0x11u] < matches) return 0;
    return 1;
}

/* The stage part of check_complete(1, ...) (0x689bb6..0x689be8, run for a parent league, +0x44 0xff, as the Ekstraklasa
 * is): every built stage (0 to +0x30) complete, and the last stage (+0x2c - 1) built. */
static int pol_first_done(uint32_t lg)
{
    if (!league_complete(lg)) return 0;
    int32_t built = (int32_t)rd32(g_mem, lg + 0x30u), count = (int32_t)rd32(g_mem, lg + 0x2cu);
    uint32_t stages = rd32(g_mem, lg + 0xcu);
    if (built < count - 1) return 0;
    for (int32_t i = 0; i <= built && i < 64; i++)
        if (!readable(stages) || !league_complete(rd32(g_mem, stages + 4u * (uint32_t)i))) return 0;
    return 1;
}


/* ---- The play-off's dates after a wait ---- */

#define ENTRIES 0x00b63d60u             /* league table entries, 0x48 bytes (game_season_update_pass 0x5d5bc8) */
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_UPDATE_DAY 0x15u          /* the day of the year of the entry's season change (0x5d5bc8) */
#define DRAW_LEAD 3                     /* days from the draw to the first leg when the dates follow from today: the
                                           original's 6 (25 to 31 May) would put the second leg of a play-off made on
                                           day 160 on day 171, after the update day 170 */

static long abs_day(int y, int d) { return 365L * y + (y - 1) / 4 - (y - 1) / 100 + (y - 1) / 400 + d; }
static void from_abs(long a, int base, int *y, int *d)
{
    int yy = base - 1;
    while (abs_day(yy + 1, 0) <= a) yy++;
    *y = yy, *d = (int)(a - abs_day(yy, 0));
}

/* The update day of the entry that holds a competition, as an absolute day on or after today; -1 for none. */
static long update_day_of(uint32_t comp)
{
    int ty = (int16_t)rd16(g_mem, TODAY + 2u), td = (int16_t)rd16(g_mem, TODAY);
    for (int e = 0; e < ENTRY_COUNT; e++) {
        uint32_t at = ENTRIES + (uint32_t)e * ENTRY_SIZE, arr = rd32(g_mem, at + 0x10u);
        int32_t n = (int32_t)rd32(g_mem, at + 0xcu);
        for (int32_t k = 0; readable(arr) && k < n && k < 256; k++)
            if (rd32(g_mem, arr + 4u * (uint32_t)k) == comp) {
                int upd = (int16_t)rd16(g_mem, at + ENTRY_UPDATE_DAY);
                if (upd < 0 || upd > 365) return -1;
                return abs_day(upd >= td ? ty : ty + 1, upd);
            }
    }
    return -1;
}

/* abs_day: the days from a fixed origin (leap years by the Gregorian rule, as Date::Date 0x549f70).
 *
 * The play-off's dates come from create_schedule: draw 25 May, first leg 31 May, the second leg the gap +0x22 later,
 * 4 days (PolSecond::create_schedule 0x7cbd6a..0x7cbde0; Cup::create_tie_fixtures 0x51cb90 adds the gap to the round's
 * match date at 0x51d13c..0x51d175). When a play-off is made after those dates, the original draws at the next update
 * (Cup::update_for_date 0x51fa45..0x51fa7d) and puts each leg whose date has passed on tomorrow (0x51cf13..0x51cf46
 * and 0x51d241..0x51d284), so both legs get one day, the fault of item 90 (Cup::process_fixture box "Cup..cpp 1278";
 * a wait forced to day 160 put both on day 161, and only the item 90 rule of fixture_season_end.c moved the second to
 * day 168). And a play-off that ends on or after the Polish update day leaves the I liga open then, so the
 * Ekstraklasa's chain test 0x48ce10 fails and both leagues stall again. So the dates follow from the day the play-off
 * is made: the draw tomorrow at the earliest, the first leg DRAW_LEAD days after the draw at the earliest, both no
 * earlier than the original's, and the first leg as late as the gap allows before the update day when the second
 * would come on or after it. A play-off that still cannot end before the update day (one made in its last days) holds
 * the Polish season change until it ends (season_pol_playoff_unfinished, called by season_playoffs_hold). The round
 * table: draw day +0 and its s8 year offset +2 from +0x40, match day +3 and its offset +5 (cup_set_round_draw_date
 * 0x521e60, cup_set_round_match_date 0x521eb0, both with the I liga's +0x40, which the Cup_Stage takes at 0x7cc3b7). */
static void dates_from_today(uint32_t self, uint32_t t)
{
    if (!readable(t)) return;
    int base = (int16_t)rd16(g_mem, self + 0x40u), gap = g_mem[t + 0x22u];
    long today = abs_day((int16_t)rd16(g_mem, TODAY + 2u), (int16_t)rd16(g_mem, TODAY));
    long draw = abs_day(base + (int8_t)g_mem[t + 2u], (int16_t)rd16(g_mem, t));
    long leg1 = abs_day(base + (int8_t)g_mem[t + 5u], (int16_t)rd16(g_mem, t + 3u));
    long was_draw = draw, was_leg1 = leg1, upd = update_day_of(self);
    if (draw <= today) draw = today + 1;
    if (leg1 < draw + DRAW_LEAD) leg1 = draw + DRAW_LEAD;
    /* A first leg that would put the second on or after the update day comes earlier, as late as the gap allows and
     * at least a day after the draw (so the lead can be shorter than DRAW_LEAD). */
    if (upd >= 0 && leg1 + gap >= upd && upd - 1 - gap > draw) leg1 = upd - 1 - gap;
    if (draw == was_draw && leg1 == was_leg1) return;
    int dy, dd, ly, ld, sy, sd;
    from_abs(draw, base, &dy, &dd);
    from_abs(leg1, base, &ly, &ld);
    from_abs(leg1 + gap, base, &sy, &sd);
    wr16(g_mem, t, (uint16_t)dd);
    g_mem[t + 2u] = (uint8_t)(int8_t)(dy - base);
    wr16(g_mem, t + 3u, (uint16_t)ld);
    g_mem[t + 5u] = (uint8_t)(int8_t)(ly - base);
    fprintf(stderr, "recomp: playoffs: pol_second %d: the play-off draws on day %d of %d, legs on day "
                    "%d of %d and day %d of %d (update day %ld days after today)\n", base, dd, dy, ld, ly, sd, sy,
            upd >= 0 ? upd - today : -1L);
}

/* season_playoffs_hold: the I liga of entry e whose edition is before the game year. 0: none, or a play-off that
 * ended before the update day (so a season change that failed for another cause is not retried here); 1: its play-off
 * is complete and its second leg was due on or after the update day; 2: it is not complete (*file gets "pol_second").
 * Complete: its own table, its stage built (+0x30 at +0x2c - 1) and the Cup_Stage complete (Cup::check_complete
 * 0x521e00 reads the byte +0xb1 for a stage). All of it is in the save, so a load in the hold changes nothing. Read
 * only. */
int season_pol_playoff_unfinished(int e, int year, const char **file)
{
    if (!season_game_changes() || e < 0 || e >= ENTRY_COUNT) return 0;
    uint32_t at = ENTRIES + (uint32_t)e * ENTRY_SIZE, arr = rd32(g_mem, at + 0x10u);
    int32_t n = (int32_t)rd32(g_mem, at + 0xcu);
    for (int32_t k = 0; readable(arr) && k < n && k < 256; k++) {
        uint32_t c = rd32(g_mem, arr + 4u * (uint32_t)k), vt = readable(c) ? rd32(g_mem, c) : 0;
        if (vt != POL_SECOND_VTABLE || (int16_t)rd16(g_mem, c + 0x40u) >= year) continue;
        int32_t built = (int32_t)rd32(g_mem, c + 0x30u), count = (int32_t)rd32(g_mem, c + 0x2cu);
        uint32_t stages = rd32(g_mem, c + 0xcu), st = built >= 0 && readable(stages) ? rd32(g_mem, stages) : 0;
        int done = league_complete(c) && built >= count - 1 && (count < 1 || (readable(st) && g_mem[st + 0xb1u]));
        if (!done) {
            *file = "pol_second";
            return 2;
        }
        uint32_t t = readable(st) ? rd32(g_mem, st + 0xa3u) : 0;
        int upd = (int16_t)rd16(g_mem, at + ENTRY_UPDATE_DAY), base = (int16_t)rd16(g_mem, st + 0x40u);
        if (!readable(t) || upd < 0) return 0;
        /* The last match of the play-off: the I liga's own play-off has 2 rounds (the semi-finals, then the final 4 or
         * more days later, season_playoffs.c), the play-off against the Ekstraklasa has 1. */
        long leg2 = 0;
        for (int r = 0, rounds = season_pol_first_own_playoff(c) ? 2 : 1; r < rounds; r++) {
            uint32_t tr = t + 0x68u * (uint32_t)r;
            long d = abs_day(base + (int8_t)g_mem[tr + 5u], (int16_t)rd16(g_mem, tr + 3u)) + g_mem[tr + 0x22u];
            if (d > leg2) leg2 = d;
        }
        return leg2 >= abs_day(year, upd) ? 1 : 0;
    }
    return 0;
}

void o_007cc1c0(void);

static int waiting_edition = -1;        /* the I liga edition whose play-off waits, for the log lines only */

/* Test switch RECOMP_TEST_POL_WAIT=<day> (season games only): the play-off also waits until that day of the year, as if
 * the Ekstraklasa's spring stage ended then (tests/seasons/pol_playoff_run.py: a wait that ends after both leg dates,
 * and one that ends so late that the play-off ends after the Polish update day). */
static int test_wait_day(void)
{
    static int day = -2;
    if (day == -2) {
        const char *v = getenv("RECOMP_TEST_POL_WAIT");
        day = v && *v ? atoi(v) : -1;
    }
    return day;
}

void o_007cb4a0(void);

#define MAKE_PLAYOFF_RET 0x007cc393u   /* create_play_off_stage's call of vtable +0x3c (0x7cc390) */

/* PolSecond::create_schedule (vtable +0x3c), thiscall (s8 stage, s16 *rounds, s16 *, int *), ret 0x10: the play-off's
 * round table for stage 0 (0x7cbcc2..0x7cbde0), which create_play_off_stage 0x7cc290 calls at 0x7cc390 before the
 * Cup_Stage 0x522e00 copies it (0x522f21) and Cup::set_up_round 0x51c800 (0x7cc46d) draws it when its draw date has
 * passed. So the dates are set here, before that draw, for every play-off made in a season game (by the return
 * address, so no state that a load loses). They change only when the original's have passed or end on or after the
 * update day. */
RT_WRAP(007cb4a0, pol_second_schedule)
{
    uint32_t self = cpu.ecx, ret = rd32(g_mem, cpu.esp);
    int stage = (int8_t)g_mem[cpu.esp + 4u];
    o_007cb4a0();
    if (!rt_unwinding && ret == MAKE_PLAYOFF_RET && stage == 0) season_pol_first_playoff_round(self, cpu.eax);
    if (!rt_unwinding && ret == MAKE_PLAYOFF_RET && stage == 0 && season_game_changes())
        dates_from_today(self, cpu.eax);
}

/* PolSecond::advance_stage, thiscall, no arguments. */
RT_WRAP(007cc1c0, pol_second_advance)
{
    uint32_t self = cpu.ecx, table = rd32(g_mem, COMP_OBJECTS), id = rd32(g_mem, KEY_POL_FIRST);
    uint32_t first = readable(table) && id < rd32(g_mem, COMP_SLOTS) ? rd32(g_mem, table + 4u * id) : 0;
    /* Only the step that makes the play-off (+0x30 from -1 to 0, 0x7cc276..0x7cc281) waits. */
    if (season_game_changes() && (int32_t)rd32(g_mem, self + 0x30u) == -1 && readable(first) &&
        league_complete(first) && (!pol_first_done(first) || (int16_t)rd16(g_mem, TODAY) < test_wait_day())) {
        int edition = (int16_t)rd16(g_mem, self + 0x40u);
        if (waiting_edition != edition) {
            fprintf(stderr, "recomp: playoffs: pol_second %d: the play-off waits for the end of the Ekstraklasa's "
                            "last stage (day %d of %d)\n", edition, (int16_t)rd16(g_mem, TODAY),
                    (int16_t)rd16(g_mem, TODAY + 2u));
            waiting_edition = edition;
        }
        rt_ret_stdcall(cpu.eax, 0);
        return;
    }
    int waited = waiting_edition == (int16_t)rd16(g_mem, self + 0x40u) && (int32_t)rd32(g_mem, self + 0x30u) == -1;
    o_007cc1c0();
    if (waited && !rt_unwinding && (int32_t)rd32(g_mem, self + 0x30u) == 0) {
        fprintf(stderr, "recomp: playoffs: pol_second %d: the play-off is made on day %d of %d, after the wait\n",
                waiting_edition, (int16_t)rd16(g_mem, TODAY), (int16_t)rd16(g_mem, TODAY + 2u));
        waiting_edition = -1;
    }
}
