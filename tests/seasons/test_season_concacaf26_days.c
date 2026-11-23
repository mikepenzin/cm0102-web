/* The match days of the CONCACAF World Cup qualifying in the 2026 shape (item 214, season_concacaf26_days.c).
 *   2026: the real days (22 and 26 March 2024; 5, 8 and 11 June 2024, 6 and 10 June 2025; 4 and 8 September,
 *   9 and 13 October, 13 and 18 November 2025), each a day of the FIFA days list.
 *   2030 to 2302: the window rule on the FIFA days list; every day is a FIFA day, the rounds come in order, two
 *   rounds of a stage at least 3 days apart, the legs of a tie 3 to 6 days apart, the draw before the first leg,
 *   and nothing of the autumn of W - 2 (the Nations League's league phase of item 209 takes W - 4 and W - 3). */
#include "season_concacaf26_days.h"
#include "season_uefa_days.h"

#include <stdio.h>

static int fails;

#define CHECK(c, ...)                                                                                                 \
    do {                                                                                                              \
        if (!(c)) {                                                                                                   \
            fails++;                                                                                                  \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                                                               \
            printf(__VA_ARGS__);                                                                                      \
            printf("\n");                                                                                             \
        }                                                                                                             \
    } while (0)

static int table(long z, void *ctx)
{
    (void)ctx;
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(z, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX), doy = season_day_of_year(y, m, d);
    for (int i = 0; i < n; i++)
        if (doys[i] == doy) return 1;
    return 0;
}

static long day(int y, int m, int d) { return season_abs_day(y, m, d); }

static void check_order(int wc, const SeasonCc26Days *p)
{
    long all[SEASON_CC26_FIRST + SEASON_CC26_SECOND + SEASON_CC26_THIRD];
    int n = 0;
    for (int i = 0; i < SEASON_CC26_FIRST; i++) all[n++] = p->first[i];
    for (int i = 0; i < SEASON_CC26_SECOND; i++) all[n++] = p->second[i];
    for (int i = 0; i < SEASON_CC26_THIRD; i++) all[n++] = p->third[i];
    CHECK(p->draw < p->first[0], "%d: draw after the first leg", wc);
    for (int i = 0; i < n; i++) {
        CHECK(table(all[i], NULL), "%d: day %d is not a FIFA day", wc, i);
        if (i) CHECK(all[i] - all[i - 1] >= 3, "%d: days %d and %d %ld apart", wc, i - 1, i, all[i] - all[i - 1]);
        int y, m, d;
        season_from_abs_day(all[i], &y, &m, &d);
        CHECK(y == wc - 2 || y == wc - 1, "%d: day %d in %d", wc, i, y);
        CHECK(!(y == wc - 2 && m >= 9), "%d: day %d in the autumn of %d", wc, i, y);
    }
    long gap = p->first[1] - p->first[0];
    CHECK(gap >= 3 && gap <= 6, "%d: first round legs %ld apart", wc, gap);
}

int main(void)
{
    SeasonCc26Days p;
    CHECK(season_cc26_days(2026, table, NULL, &p) == 1 && p.real, "2026: no plan");
    CHECK(p.draw == day(2024, 1, 25), "2026 draw");
    CHECK(p.first[0] == day(2024, 3, 22) && p.first[1] == day(2024, 3, 26), "2026 first round");
    static const short second[5][3] = {{2024, 6, 5}, {2024, 6, 8}, {2024, 6, 11}, {2025, 6, 6}, {2025, 6, 10}};
    static const short third[6][3] = {{2025, 9, 4}, {2025, 9, 8}, {2025, 10, 9}, {2025, 10, 13}, {2025, 11, 13},
                                      {2025, 11, 18}};
    for (int i = 0; i < 5; i++) CHECK(p.second[i] == day(second[i][0], second[i][1], second[i][2]), "2026 second %d", i);
    for (int i = 0; i < 6; i++) CHECK(p.third[i] == day(third[i][0], third[i][1], third[i][2]), "2026 third %d", i);
    check_order(2026, &p);

    /* 2030: the windows March 2028 23 to 28, June 2028 1 to 6, March 2029 22 to 27, June 2029 7 to 12, then 27
     * September to 2 October, 4 to 9 October and 15 to 20 November 2029; each pair its first day and 4 days later */
    CHECK(season_cc26_days(2030, table, NULL, &p) == 1 && !p.real, "2030: no plan");
    CHECK(p.first[0] == day(2028, 3, 23) && p.first[1] == day(2028, 3, 27), "2030 first round");
    CHECK(p.second[0] == day(2028, 6, 1) && p.second[1] == day(2028, 6, 5) && p.second[2] == day(2029, 3, 22) &&
              p.second[3] == day(2029, 6, 7) && p.second[4] == day(2029, 6, 11),
          "2030 second round");
    CHECK(p.third[0] == day(2029, 9, 27) && p.third[1] == day(2029, 10, 1) && p.third[2] == day(2029, 10, 4) &&
              p.third[5] == day(2029, 11, 19),
          "2030 third round");

    int ok = 0;
    for (int wc = 2030; wc <= 2302; wc += 4) {
        if (!season_cc26_days(wc, table, NULL, &p)) {
            CHECK(0, "%d: no plan", wc);
            continue;
        }
        ok++;
        check_order(wc, &p);
    }
    CHECK(season_cc26_days(2022, table, NULL, &p) == 0, "2022 has no plan");
    printf("concacaf26 days: %d cycles from 2030 planned\n", ok);
    if (fails) printf("%d failures\n", fails);
    else printf("season_concacaf26_days: all checks pass\n");
    return fails != 0;
}
