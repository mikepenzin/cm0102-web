/* The UEFA knockout dates (O259, O378 review): the rows of a season, and the dates of a season after the table (its
 * last row moved 364 days a year, 7 days more when it drifts over 3 days, a final after 31 May on 31 May).
 * ctest season_uefa_dates. */
#include "season_uefa.h"

#include <stdio.h>

static int failures;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        if (!(cond)) {                                                              \
            printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);                  \
            failures++;                                                             \
        }                                                                           \
    } while (0)

/* The first leg of comp, season and round as day, month 1 to 12, year (the calendar year). */
static int leg(char comp, int season, int round, int *d, int *m, int *y)
{
    const SeasonUefaKoDate *r = season_uefa_ko_date(comp, season, round);
    if (!r) return 0;
    *d = r->day, *m = r->month + 1, *y = season + r->year_off;
    return 1;
}

/* 0 Sunday to 6 Saturday (Sakamoto). */
static int weekday(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    y -= m < 3;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

int main(void)
{
    int d, m, y;
    /* A season of the table: the row itself (the 2009/10 Europa League final, 12 May 2010, Hamburg). */
    CHECK(leg('u', 2009, SEASON_UEFA_KO_FINAL, &d, &m, &y) && d == 12 && m == 5 && y == 2010);
    /* 2019/20: the calendar before the break (Champions League final 30 May 2020). */
    CHECK(leg('c', 2019, SEASON_UEFA_KO_FINAL, &d, &m, &y) && d == 30 && m == 5 && y == 2020);
    /* A season before the table (the Conference League before 2021/22): none, the format's dates stay. */
    CHECK(!season_uefa_ko_date('k', 2015, SEASON_UEFA_KO_PO));
    CHECK(!season_uefa_ko_date('u', 2002, SEASON_UEFA_KO_R16));
    /* The last season of the table, and the seasons after it: each round in every competition has a date, on the
     * last row's weekday, within 3 days of its calendar day, a final by 31 May, and the rounds in order. */
    for (int c = 0; c < 3; c++) {
        char comp = "cuk"[c];
        for (int s = 2027; s <= 2060; s++) {
            int prev = -1;
            for (int r = 0; r <= SEASON_UEFA_KO_FINAL; r++) {
                const SeasonUefaKoDate *row = season_uefa_ko_date(comp, s, r);
                if (!row) continue;
                int ld, lm, ly, bd, bm, by;
                CHECK(leg(comp, s, r, &ld, &lm, &ly));
                CHECK(leg(comp, 2027, r, &bd, &bm, &by));
                CHECK(weekday(ly, lm, ld) == weekday(by, bm, bd) || (r == SEASON_UEFA_KO_FINAL && lm == 5 && ld == 31));
                int doy = lm * 31 + ld, bdoy = bm * 31 + bd;           /* an order, not a day count */
                CHECK(r != SEASON_UEFA_KO_FINAL || lm < 6);
                CHECK(doy > prev);
                CHECK(ly - s == by - 2027);
                if (!(r == SEASON_UEFA_KO_FINAL && lm == 5 && ld == 31)) CHECK(doy - bdoy >= -4 && doy - bdoy <= 4);
                prev = doy;
            }
        }
    }
    /* 2028/29 Europa League final: 17 May 2028 + 364 days = Wednesday 16 May 2029. */
    CHECK(leg('u', 2028, SEASON_UEFA_KO_FINAL, &d, &m, &y) && d == 16 && m == 5 && y == 2029);
    /* The log line: once per competition and season. */
    CHECK(season_uefa_ko_dates_log('u', 2030) == 1);
    CHECK(season_uefa_ko_dates_log('u', 2030) == 0);
    CHECK(season_uefa_ko_dates_log('c', 2030) == 1);
    CHECK(season_uefa_ko_dates_log('u', 2031) == 1);
    if (failures) return 1;
    printf("season_uefa_dates: all checks pass\n");
    return 0;
}
