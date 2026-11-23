/* The days of the South American section of World Cup qualifying (open item 163 of 0.6.2, season_conmebol_days.c).
 * Every cycle from 2006 to 2302 with 14, 16 and 18 rounds: each round on a FIFA day (or a real matchday), at least
 * 3 days after the one before, in the span, after the day the section is made; with a finals stand-in that holds
 * every 10 June to 20 July, none of the window rule in it. 18 rounds of 2006, 2010, 2014, 2018 and 2026 take the
 * real first and last matchday; 2014 (run set a2002s3, whose 1 August 2012 met the Copa Sudamericana) has the real
 * days. The 2098 cycle has no round in July or August 2096 (a2006s3). The fields of a round record. */
#include "season_conmebol_days.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <stdlib.h>

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

static int is_fifa_day(long z)
{
    int y, m, d, doys[SEASON_FIFA_YEAR_MAX];
    season_from_abs_day(z, &y, &m, &d);
    int n = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX), doy = season_day_of_year(y, m, d);
    for (int i = 0; i < n; i++)
        if (doys[i] == doy) return 1;
    return 0;
}

static int summer(long z)                       /* a stand-in for the Copa America: 10 June to 20 July */
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    return (m == 6 && d >= 10) || (m == 7 && d <= 20);
}

static void cycle(int wc, int rounds, SeasonConmebolBusy busy)
{
    long out[64], first, last;
    long made = season_abs_day(wc - 4, 7, 25);          /* set up on 25 July after the finals before it */
    season_conmebol_span(wc, &first, &last);
    int n = season_conmebol_days(wc, rounds, made + 2, busy, out);
    CHECK(n == rounds, "%d, %d rounds%s: %d placed", wc, rounds, busy ? " (finals)" : "", n);
    for (int i = 0; i < n; i++) {
        int y, m, d;
        season_from_abs_day(out[i], &y, &m, &d);
        CHECK(is_fifa_day(out[i]) || season_conmebol_real_day(out[i]), "%d round %d on %04d-%02d-%02d: not a FIFA day",
              wc, i + 1, y, m, d);
        CHECK(out[i] <= last && out[i] >= made + 2, "%d round %d on %04d-%02d-%02d: out of the span", wc, i + 1, y, m,
              d);
        CHECK(!i || out[i] - out[i - 1] >= 3, "%d round %d on %04d-%02d-%02d: %ld days after the one before", wc,
              i + 1, y, m, d, i ? out[i] - out[i - 1] : 0);
        CHECK(!busy || season_conmebol_real_day(out[i]) || !busy(out[i]), "%d round %d on %04d-%02d-%02d: in a final's days",
              wc, i + 1, y, m, d);
    }
    /* The real cycles: the real first and last matchday. */
    int real = rounds == 18 && (wc == 2006 || wc == 2010 || wc == 2014 || wc == 2018 || wc == 2026);
    if (real && n == rounds) {
        CHECK(out[0] == first, "%d: first round not on the real first matchday", wc);
        CHECK(out[n - 1] == last, "%d: last round not on the real last matchday", wc);
    }
}

int main(void)
{
    for (int wc = 2006; wc <= 2302; wc += 4)
        for (int r = 14; r <= 18; r += 2) {
            cycle(wc, r, NULL);
            cycle(wc, r, summer);
        }

    /* 2014 (a2002s3): the real days (en.wikipedia.org, 2014 FIFA World Cup qualification (CONMEBOL)). */
    static const int want[18][3] = {{2011, 10, 7}, {2011, 10, 11}, {2011, 11, 11}, {2011, 11, 15}, {2012, 6, 2},
                                    {2012, 6, 9},  {2012, 9, 7},   {2012, 9, 11},  {2012, 10, 12}, {2012, 10, 16},
                                    {2013, 3, 22}, {2013, 3, 26},  {2013, 6, 7},   {2013, 6, 11},  {2013, 9, 6},
                                    {2013, 9, 10}, {2013, 10, 11}, {2013, 10, 15}};
    long out[64];
    int n = season_conmebol_days(2014, 18, season_abs_day(2010, 7, 27), NULL, out);
    CHECK(n == 18, "2014: %d placed", n);
    for (int i = 0; i < n; i++) {
        int y, m, d;
        season_from_abs_day(out[i], &y, &m, &d);
        CHECK(y == want[i][0] && m == want[i][1] && d == want[i][2], "2014 round %d on %04d-%02d-%02d, want %04d-%02d-%02d",
              i + 1, y, m, d, want[i][0], want[i][1], want[i][2]);
        CHECK(out[i] != season_abs_day(2012, 8, 1), "2014: a round on 1 August 2012");
    }

    /* 2098 (a2006s3): no round in July or August. */
    n = season_conmebol_days(2098, 18, season_abs_day(2094, 7, 20), NULL, out);
    for (int i = 0; i < n; i++) {
        int y, m, d;
        season_from_abs_day(out[i], &y, &m, &d);
        CHECK(m != 7 && m != 8, "2098 round %d on %04d-%02d-%02d", i + 1, y, m, d);
    }

    /* A section made after its span began places nothing before the day it is made (the window rule). */
    n = season_conmebol_days(2014, 18, season_abs_day(2012, 1, 1), NULL, out);
    CHECK(n == 18 && out[0] >= season_abs_day(2012, 1, 1), "2014 made in 2012: a round before it");

    /* A 2018 section made on 1 December 2015, after its real first day: the windows to 10 October 2017 hold too
     * few rounds, so 0 (the caller then keeps the template, end_by_october);
     * a window rule cycle ends in the last FIFA window of its span (2030: September 2029 or later). */
    n = season_conmebol_days(2018, 18, season_abs_day(2015, 12, 1), NULL, out);
    CHECK(n == 0, "2018 made late: %d placed, want 0 (the windows from December 2015 hold fewer than 18: the template)", n);
    n = season_conmebol_days(2030, 14, season_abs_day(2026, 7, 20), NULL, out);
    CHECK(n == 14 && out[13] >= season_abs_day(2029, 9, 1), "2030 (14 rounds): ends too early");
    n = season_conmebol_days(2030, 18, season_abs_day(2026, 7, 20), NULL, out);
    CHECK(n == 18 && out[17] >= season_abs_day(2029, 9, 1), "2030 (18 rounds): ends too early");

    /* 2022: the window rule from 8 October 2020 to the game's 10 October 2021. */
    long first, last;
    season_conmebol_span(2022, &first, &last);
    n = season_conmebol_days(2022, 18, season_abs_day(2018, 7, 20), NULL, out);
    CHECK(n == 18 && out[17] <= last, "2022: %d placed", n);

    /* Round record fields: Friday 7 October 2011, campaign year 2012 (base): day 279, year -1, midweek 2; Saturday
     * 2 June 2012: day 153, year 0, weekend 1; Sunday 13 October 2002 (base 2004): weekend 1. */
    int doy, off, flag;
    season_conmebol_round_fields(season_abs_day(2011, 10, 7), 2012, &doy, &off, &flag);
    CHECK(doy == 279 && off == -1 && flag == 2, "7 October 2011: %d %d %d", doy, off, flag);
    season_conmebol_round_fields(season_abs_day(2012, 6, 2), 2012, &doy, &off, &flag);
    CHECK(doy == 153 && off == 0 && flag == 1, "2 June 2012: %d %d %d", doy, off, flag);
    season_conmebol_round_fields(season_abs_day(2002, 10, 13), 2004, &doy, &off, &flag);
    CHECK(flag == 1 && off == -2, "13 October 2002: %d %d %d", doy, off, flag);
    season_conmebol_round_fields(season_abs_day(2025, 9, 9), 2024, &doy, &off, &flag);
    CHECK(flag == 2 && off == 1, "9 September 2025 (Tuesday): %d %d %d", doy, off, flag);

    /* The make day (WcSouthAmericanLeague::update_for_date 0x921160 makes the fixtures on it, 0x92123a). The
     * original's: the Saturday near 25 January of the campaign year (28 January 2012). With the real first round of
     * 7 October 2011: 9 September 2011, so no round lies before it (FIXTURE_MANAGER::add_fixture 0x5aa680 moves
     * such a round, 0x5aa729). A section made late: tomorrow. A first round after the original's: the original's. */
    long orig = season_abs_day(2012, 1, 28), today = season_abs_day(2010, 7, 25);
    CHECK(season_conmebol_make_day(season_abs_day(2011, 10, 7), orig, today) == season_abs_day(2011, 9, 9),
          "make day of the 2014 real cycle");
    CHECK(season_conmebol_make_day(season_abs_day(2010, 8, 10), orig, today) == today + 1, "make day: tomorrow");
    CHECK(season_conmebol_make_day(season_abs_day(2012, 2, 29), orig, today) == orig, "make day: the original's");
    for (int wc = 2006; wc <= 2302; wc += 4) {
        long made = season_abs_day(wc - 4, 7, 25);
        n = season_conmebol_days(wc, 18, made + 2, NULL, out);
        long o = season_abs_day(wc - 2, 1, 25), mk = season_conmebol_make_day(out[0], o, made);
        CHECK(n == 18 && mk > made && mk < out[0] && mk <= o, "%d: make day out of order", wc);
    }

    /* Every real day is a FIFA day or a real matchday; 9 June 2012 is not in the FIFA list. */
    CHECK(!is_fifa_day(season_abs_day(2012, 6, 9)) && season_conmebol_real_day(season_abs_day(2012, 6, 9)),
          "9 June 2012");
    CHECK(!season_conmebol_real_day(season_abs_day(2012, 8, 1)), "1 August 2012 a real matchday");

    if (fails) {
        printf("%d failures\n", fails);
        return 1;
    }
    printf("season_conmebol_days: all checks passed\n");
    return 0;
}
