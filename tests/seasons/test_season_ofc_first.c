/* ctest season_ofc_first (item 198): the plan of the OFC preliminary round and its days (season_ofc_first_plan.c).
 * The plan: pools of 9 to 25 for 9 and 10 places. The days: a group draw on 15 January of every year from 2005 to
 * 2302, from a day 200 days before it (both legs on FIFA days of one window, 3 to 6 days apart, after the lead day
 * and OFC_FIRST_MARGIN days or more before the draw), and none from 5 days before the draw. */
#include "season_ofc_first.h"
#include "season_uefa_days.h"

#include <stdio.h>

static int fails;

#define CHECK(c, ...)                                                                                                \
    do {                                                                                                             \
        if (!(c)) {                                                                                                  \
            fails++;                                                                                                 \
            printf("FAIL " __VA_ARGS__);                                                                             \
            printf("\n");                                                                                            \
        }                                                                                                            \
    } while (0)

static int fifa_day(long day)
{
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(day, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX);
    long base = season_abs_day(y, 1, 1);
    for (int i = 0; i < n; i++)
        if (base + doys[i] == day) return 1;
    return 0;
}

int main(void)
{
    SeasonOfcFirstPlan plan;
    CHECK(season_ofc_first_plan(10, 10, &plan) == 0 && plan.byes == 10, "pool 10: no round");
    CHECK(season_ofc_first_plan(9, 10, &plan) == 0, "pool 9: no round");
    CHECK(season_ofc_first_plan(11, 10, &plan) == 1 && plan.byes == 9 && plan.pair[0][0] == 9 && plan.pair[0][1] == 10,
          "pool 11: one tie 9 v 10, 9 byes (%d %d %d)", plan.byes, plan.pair[0][0], plan.pair[0][1]);
    CHECK(season_ofc_first_plan(12, 10, &plan) == 2 && plan.byes == 8 && plan.pair[0][0] == 8 &&
              plan.pair[0][1] == 11 && plan.pair[1][0] == 9 && plan.pair[1][1] == 10,
          "pool 12: ties 8 v 11, 9 v 10");
    CHECK(season_ofc_first_plan(11, 9, &plan) == 2 && plan.byes == 7, "pool 11, 9 places: two ties");
    CHECK(season_ofc_first_plan(25, 10, &plan) == 10 && plan.byes == 0, "pool 25: capped at 20, ten ties");
    int years = 0;
    for (int y = 2005; y <= 2302; y++) {
        long gd = season_abs_day(y, 1, 15), draw, l1, l2;
        int ok = season_ofc_first_days(gd, gd - 200, &draw, &l1, &l2);
        CHECK(ok, "%d: no window", y);
        if (!ok) continue;
        CHECK(fifa_day(l1) && fifa_day(l2), "%d: legs not on FIFA days", y);
        CHECK(l2 - l1 >= 3 && l2 - l1 <= 6, "%d: legs %ld days apart", y, l2 - l1);
        CHECK(l1 >= gd - OFC_FIRST_LEAD && l2 <= gd - OFC_FIRST_MARGIN, "%d: legs outside the window", y);
        CHECK(draw > gd - 200 && draw < l1, "%d: draw day", y);
        years++;
        long d2, a, b;
        CHECK(!season_ofc_first_days(gd, gd - 5, &d2, &a, &b), "%d: a window 5 days before the draw", y);
    }
    int y0, m0, d0;
    long draw, l1, l2;
    season_ofc_first_days(season_abs_day(2024, 1, 17), season_abs_day(2024, 1, 17) - 200, &draw, &l1, &l2);
    season_from_abs_day(l1, &y0, &m0, &d0);
    printf("2026 cycle: first leg %04d-%02d-%02d\n", y0, m0, d0);
    printf("%s: %d years\n", fails ? "FAILED" : "PASS", years);
    return fails ? 1 : 0;
}
