/* Test of the Irish calendar move (gap row 2 of 0.7.2, src/recomp_rt/win/season_calendar.c): the state of an edition,
 * the shift of the league template, the cut of the short season, the dates of the cups, the award day and the
 * labels. Pure: no game memory.
 *
 * Usage: season_calendar_test */
#include "season_calendar.h"
#include "season_template.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

/* The template day (from 1 January of base) of a date, month 1 to 12. */
static int at(int base, int year, int month, int day)
{
    return season_template_day(base, year - base, season_calendar_doy(year, month, day));
}

/* The class's dates of edition 2002 as the game made them (run-logs ire2003 run1, leg00: "league sizes: ire_prm
 * template 2002: class 34" and "ire_first template 2002: class 36"): IrePrm::create_schedule 0x639e90 makes 34 dates
 * for 3 meetings, IreFirst::create_schedule 0x634f40 36, Sundays with a Tuesday or two (0x68a160 moves each date to its
 * weekday). */
static const int PRM_2002[][3] = {
    {2002,8,18}, {2002,8,25}, {2002,9,8}, {2002,9,15}, {2002,9,22}, {2002,9,29}, {2002,10,6}, {2002,10,13},
    {2002,10,20}, {2002,10,27}, {2002,11,3}, {2002,11,10}, {2002,11,17}, {2002,11,24}, {2002,12,1}, {2002,12,8},
    {2002,12,15}, {2002,12,22}, {2002,12,29}, {2003,1,5}, {2003,1,19}, {2003,1,26}, {2003,2,2}, {2003,2,16},
    {2003,2,23}, {2003,3,2}, {2003,3,4}, {2003,3,9}, {2003,3,16}, {2003,3,23}, {2003,3,30}, {2003,4,6}, {2003,4,13},
    {2003,4,20}
};
static const int FIRST_2002[][3] = {
    {2002,8,18}, {2002,8,20}, {2002,8,25}, {2002,9,8}, {2002,9,15}, {2002,9,22}, {2002,9,29}, {2002,10,6},
    {2002,10,13}, {2002,10,20}, {2002,10,27}, {2002,11,3}, {2002,11,10}, {2002,11,17}, {2002,11,24}, {2002,12,1},
    {2002,12,8}, {2002,12,15}, {2002,12,22}, {2002,12,29}, {2003,1,5}, {2003,1,19}, {2003,1,26}, {2003,2,2},
    {2003,2,11}, {2003,2,16}, {2003,2,23}, {2003,3,2}, {2003,3,4}, {2003,3,9}, {2003,3,16}, {2003,3,23}, {2003,3,30},
    {2003,4,6}, {2003,4,13}, {2003,4,20}
};

static int class_days(const int (*src)[3], int n, int base, int *days)
{
    for (int i = 0; i < n; i++) days[i] = at(base, src[i][0], src[i][1], src[i][2]);
    return n;
}

int main(void)
{
    /* The state of an edition (decision 9: from the start year, changing rules and the edition +0x40). */
    CHECK(season_calendar_state(2001, 0, 2001) == SEASON_CAL_WINTER && season_calendar_state(2001, 0, 2005) ==
          SEASON_CAL_WINTER, "the base game stays winter");
    CHECK(season_calendar_state(2002, 0, 2002) == SEASON_CAL_WINTER, "a 2002 start with changing rules off: winter");
    CHECK(season_calendar_state(2001, 1, 2001) == SEASON_CAL_WINTER, "2001/02 with changing rules: winter");
    CHECK(season_calendar_state(2001, 1, 2002) == SEASON_CAL_MOVE, "2002-03 with changing rules: the short season");
    CHECK(season_calendar_state(2002, 1, 2002) == SEASON_CAL_MOVE, "a 2002 start with changing rules: the short season");
    CHECK(season_calendar_state(2001, 1, 2003) == SEASON_CAL_YEAR && season_calendar_state(2001, 1, 2030) ==
          SEASON_CAL_YEAR, "2003 on with changing rules: calendar years");
    CHECK(season_calendar_state(2003, 0, 2003) == SEASON_CAL_YEAR && season_calendar_state(2010, 0, 2010) ==
          SEASON_CAL_YEAR && season_calendar_state(2010, 1, 2012) == SEASON_CAL_YEAR, "a 2003 or later start: calendar");

    /* The update day: 20 February, before day 60 (key_nation_get_club_season_year 0x669950 then gives the calendar
     * year) and after the short season's play-off (final 8 February 2003). */
    CHECK(season_calendar_update_doy() == 50, "update day 20 February is day 50, got %d", season_calendar_update_doy());

    /* The template of a calendar season: the class's dates moved by whole weeks, the last nearest 30 November. */
    for (int k = 0; k < 2; k++) {
        int days[36], n = k ? class_days(FIRST_2002, 36, 2002, days) : class_days(PRM_2002, 34, 2002, days);
        int shift = season_calendar_league_shift(2002, days, n), last = days[n - 1] + shift;
        int want = at(2002, 2002, 11, 30);
        CHECK(shift % 7 == 0 && shift < 0, "%s: a shift of whole weeks back, got %d", k ? "first" : "prm", shift);
        CHECK(last - want <= 3 && want - last <= 3, "%s: last round %d, 30 November %d", k ? "first" : "prm", last,
              want);
        CHECK(days[0] + shift >= at(2002, 2002, 2, 21), "first round after the update day");
        for (int i = 0; i < n; i++)
            CHECK(season_template_weekday(2002, days[i] + shift) == season_template_weekday(2002, days[i]),
                  "weekday of date %d kept", i);
        /* The short season plans from the class's dates before 1 February 2003: through 26 January. */
        int cut = season_calendar_move_cut(2002), before = 0;
        for (int i = 0; i < n; i++) before += days[i] < cut;
        CHECK(cut == at(2002, 2003, 2, 1) && before == (k ? 23 : 22), "%s: %d class dates before the cut",
              k ? "first" : "prm", before);
    }

    /* The FAI Cup in calendar years: final nearest 26 October (2003: first round 25 May, final 26 October). Class
     * dates of IreChalCup::create_round_table 0x634360 (draw, match of each round). */
    int fai[12], out[12];
    for (int base = 2003; base <= 2010; base++) {
        int src[12] = {at(base, base, 11, 1), at(base, base, 11, 28), at(base, base, 12, 6), at(base, base + 1, 1, 9),
                       at(base, base + 1, 1, 17), at(base, base + 1, 2, 6), at(base, base + 1, 2, 14),
                       at(base, base + 1, 3, 4), at(base, base + 1, 3, 13), at(base, base + 1, 4, 2),
                       at(base, base + 1, 4, 3), at(base, base + 1, 4, 30)};
        memcpy(fai, src, sizeof fai);
        int earliest = at(base, base, 2, 21), cut = season_calendar_cup_cut(base, SEASON_CAL_YEAR);
        int target = season_calendar_cup_target("ire_chal_cup", SEASON_CAL_YEAR, base);
        CHECK(target == at(base, base, 10, 26), "%d: FAI Cup target 26 October", base);
        int r = season_calendar_cup_plan(fai, 12, earliest, cut, target, out);
        CHECK(r == 1, "%d: FAI Cup moved by whole weeks (%d)", base, r);
        CHECK(out[11] - target <= 3 && target - out[11] <= 3 && out[0] >= earliest && (out[0] - fai[0]) % 7 == 0,
              "%d: FAI Cup final %d near %d, first draw %d", base, out[11], target, out[0]);
    }
    /* The FAI Cup of the short season: as near the real final (27 October 2002) as the first draw allows (after the
     * change of 20 June 2002), and before the cut. */
    {
        int base = 2002;
        int src[12] = {at(base, base, 11, 1), at(base, base, 11, 28), at(base, base, 12, 6), at(base, base + 1, 1, 9),
                       at(base, base + 1, 1, 17), at(base, base + 1, 2, 6), at(base, base + 1, 2, 14),
                       at(base, base + 1, 3, 4), at(base, base + 1, 3, 13), at(base, base + 1, 4, 2),
                       at(base, base + 1, 4, 3), at(base, base + 1, 4, 30)};
        int earliest = at(base, base, 6, 21), cut = season_calendar_cup_cut(base, SEASON_CAL_MOVE);
        CHECK(cut == at(base, base + 1, 2, 6), "the cups of the short season end before 6 February 2003");
        int target = season_calendar_cup_target("ire_chal_cup", SEASON_CAL_MOVE, base);
        int r = season_calendar_cup_plan(src, 12, earliest, cut, target, out);
        CHECK(r == 1 && out[0] >= earliest && out[0] - 7 < earliest && out[11] < cut,
              "FAI Cup 2002-03: first draw %d (earliest %d), final %d (cut %d)", out[0], earliest, out[11], cut);
    }
    /* A cup whose span is longer than the short season (the Munster Cup: draw 1 August to final 9 May) is pressed
     * into it: order and weekdays kept, first date from the earliest, last before the cut. */
    {
        int base = 2002;
        int src[10] = {at(base, base, 8, 1), at(base, base, 8, 23), at(base, base, 8, 25), at(base, base, 10, 18),
                       at(base, base, 10, 20), at(base, base + 1, 1, 17), at(base, base + 1, 1, 19),
                       at(base, base + 1, 3, 14), at(base, base + 1, 4, 15), at(base, base + 1, 5, 9)};
        int earliest = at(base, base, 6, 21), cut = season_calendar_cup_cut(base, SEASON_CAL_MOVE);
        int r = season_calendar_cup_plan(src, 10, earliest, cut, INT_MIN, out);
        CHECK(r == 2, "Munster Cup 2002-03 pressed (%d)", r);
        int ok = out[0] >= earliest && out[9] < cut;
        for (int i = 0; i < 10; i++) {
            ok &= (out[i] - src[i]) % 7 == 0;
            if (i) ok &= out[i] > out[i - 1];
        }
        CHECK(ok, "Munster Cup 2002-03: in order, whole weeks, within %d..%d (%d..%d)", earliest, cut, out[0], out[9]);
        /* In a calendar season it fits by a shift. */
        r = season_calendar_cup_plan(src, 10, at(base, base, 2, 21), season_calendar_cup_cut(base, SEASON_CAL_YEAR),
                                     INT_MIN, out);
        CHECK(r == 1 && out[9] < season_calendar_cup_cut(base, SEASON_CAL_YEAR) && out[9] >= src[9] - 7 * 20,
              "Munster Cup in a calendar season: the fewest weeks back (%d)", r);
    }
    /* A cup that already fits stays (the President's Cup: 5 July to 4 August). */
    {
        int base = 2002, src[2] = {at(base, base, 7, 5), at(base, base, 8, 4)};
        int r = season_calendar_cup_plan(src, 2, at(base, base, 6, 21), season_calendar_cup_cut(base, SEASON_CAL_MOVE),
                                         INT_MIN, out);
        CHECK(r == 1 && out[0] == src[0] && out[1] == src[1], "the President's Cup keeps its dates");
    }

    /* The awards: 1 May in winter (IrelandAwards::daily_update 0x63bf10), 16 February in the short and calendar
     * seasons: after the season and its play-off, before the update day. */
    int d, m;
    season_calendar_award(SEASON_CAL_WINTER, &d, &m);
    CHECK(d == 1 && m == 4, "winter awards 1 May");
    season_calendar_award(SEASON_CAL_MOVE, &d, &m);
    CHECK(d == 16 && m == 1, "short season awards 16 February");
    season_calendar_award(SEASON_CAL_YEAR, &d, &m);
    CHECK(d == 16 && m == 1, "calendar season awards 16 February");

    /* Labels (decision 6): after the move the seasons up to 2002/03 keep two years. */
    CHECK(season_calendar_two_years(SEASON_CAL_YEAR, 2002) && season_calendar_two_years(SEASON_CAL_YEAR, 1990),
          "2002/03 and earlier keep two years");
    CHECK(!season_calendar_two_years(SEASON_CAL_YEAR, 2003) && !season_calendar_two_years(SEASON_CAL_YEAR, 0),
          "2003 on one year; the year of the game date (0) is the entry's");
    CHECK(!season_calendar_two_years(SEASON_CAL_WINTER, 2002), "winter: the entry's flag");
    /* History rows: a league or a cup that moves to calendar years writes its edition, so no later label moves by
     * one. */
    CHECK(season_calendar_label("ire_prm", SEASON_CAL_YEAR, 2003) == 2003 &&
          season_calendar_label("ire_first", SEASON_CAL_YEAR, 2004) == 2004 &&
          season_calendar_label("ire_chal_cup", SEASON_CAL_YEAR, 2003) == 2003 &&
          season_calendar_label("ire_lge_cup", SEASON_CAL_YEAR, 2003) == 2003, "calendar seasons: the edition");
    /* The short season writes 2002, which the screens show "2002/03" (season_calendar_two_years). */
    CHECK(season_calendar_label("ire_prm", SEASON_CAL_MOVE, 2002) == 2002 &&
          season_calendar_label("ire_chal_cup", SEASON_CAL_MOVE, 2002) == 2002 && season_calendar_two_years(
          SEASON_CAL_YEAR, season_calendar_label("ire_prm", SEASON_CAL_MOVE, 2002)), "the short season: 2002, two years");
    CHECK(!season_calendar_label("ire_prm", SEASON_CAL_WINTER, 2001) &&
          !season_calendar_label("ire_munster_cup", SEASON_CAL_YEAR, 2003) &&
          !season_calendar_label("eng_prm", SEASON_CAL_YEAR, 2003), "the game's own label elsewhere");

    printf(failures ? "FAILED: %d\n" : "PASS\n", failures);
    return failures ? 1 : 0;
}
