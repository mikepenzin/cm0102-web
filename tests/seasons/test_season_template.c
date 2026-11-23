/* The date template of a league of any size (0.7.2 league sizes, slice 2, D4 and D5): season_template.c.
 *
 * The class templates here are the original's: NorPrm::create_schedule 0x7917c0 (26 dates from 5 April, calls of
 * 0x68a160 at 0x7918ce..0x791f1a, the summer break from 31 May to 26 July) and PorSecond::create_schedule 0x7d0c40
 * (34 Sundays from 22 August, 0x7d0d0f..0x7d14f5), both with the dates that Date 0x549f70 gives in the year. */
#include "season_template.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

static int day(int year, int month, int d)   /* month 1..12 */
{
    static const int cum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return cum[month - 1] + d - 1 + (leap && month > 2);
}

/* The common checks of a plan: rounds dates, ascending and distinct, the class's dates in it (growth) or a part
 * of them (shrink), each new date a Wednesday (or an earlier start), none in the largest gap or a gap over 14
 * days, the last date the class's last. */
static void check_plan(const char *name, int base, const int *days, int count, int rounds, const SeasonTemplateDate *p)
{
    int largest = 0;
    for (int i = 0; i + 1 < count; i++)
        if (days[i + 1] - days[i] > largest) largest = days[i + 1] - days[i];
    for (int k = 0; k < rounds; k++) {
        CHECK(k == 0 || p[k].day > p[k - 1].day, "%s: date %d (%d) not after date %d (%d)", name, k, p[k].day, k - 1,
              p[k - 1].day);
        CHECK(p[k].from >= 0 && p[k].from < count, "%s: date %d copies record %d", name, k, p[k].from);
        if (!p[k].added) {
            CHECK(days[p[k].from] == p[k].day, "%s: class date %d is %d, record %d is %d", name, k, p[k].day,
                  p[k].from, days[p[k].from]);
            continue;
        }
        if (p[k].day < days[0]) {
            CHECK((days[0] - p[k].day) % 7 == 0 && days[0] - p[k].day <= 7 * SEASON_TEMPLATE_EARLIER,
                  "%s: earlier start %d, first class date %d", name, p[k].day, days[0]);
            continue;
        }
        CHECK(season_template_weekday(base, p[k].day) == SEASON_TEMPLATE_WEDNESDAY, "%s: new date %d is weekday %d", name,
              p[k].day, season_template_weekday(base, p[k].day));
        for (int i = 0; i + 1 < count; i++)
            if (days[i] < p[k].day && p[k].day < days[i + 1]) {
                int gap = days[i + 1] - days[i];
                CHECK(gap >= 7 && gap <= 14 && (largest <= 7 || gap != largest), "%s: new date %d in a gap of %d days (largest %d)",
                      name, p[k].day, gap, largest);
                CHECK(p[k].day - days[i] >= 2 && days[i + 1] - p[k].day >= 2, "%s: new date %d too near %d or %d",
                      name, p[k].day, days[i], days[i + 1]);
            }
    }
    CHECK(p[rounds - 1].day == days[count - 1], "%s: last date %d, class %d", name, p[rounds - 1].day,
          days[count - 1]);
}

static int count_added(const SeasonTemplateDate *p, int n)
{
    int a = 0;
    for (int k = 0; k < n; k++) a += p[k].added;
    return a;
}

static void test_weekday(void)
{
    CHECK(season_template_weekday(2009, 94) == 6, "5 April 2009 is weekday %d, want 6 (Sunday)",
          season_template_weekday(2009, 94));
    CHECK(season_template_weekday(2000, 0) == 5, "1 January 2000 is weekday %d, want 5 (Saturday)",
          season_template_weekday(2000, 0));
    CHECK(season_template_weekday(2008, 59) == 4, "29 February 2008 is weekday %d, want 4 (Friday)",
          season_template_weekday(2008, 59));
    CHECK(season_template_weekday(2015, 3) == 6, "4 January 2015 is weekday %d, want 6 (Sunday)",
          season_template_weekday(2015, 3));
    CHECK(season_template_year_days(2000) == 366 && season_template_year_days(1900) == 365 &&
          season_template_year_days(2014) == 365, "year days");
    CHECK(season_template_day(2014, 1, 3) == 368, "day of 4 January 2015 from 2014: %d", season_template_day(2014, 1, 3));
    CHECK(season_template_day(2015, 1, 0) == 365 && season_template_day(2016, 1, 0) == 366, "a year from 2015 and 2016");
    int yoff = -1, doy = -1;
    season_template_split(2014, 368, &yoff, &doy);
    CHECK(yoff == 1 && doy == 3, "split of 368 from 2014: %d %d", yoff, doy);
    season_template_split(2016, 365, &yoff, &doy);
    CHECK(yoff == 0 && doy == 365, "split of 365 from 2016 (leap): %d %d", yoff, doy);
}

/* NorPrm 2009: 26 dates, 16 clubs that meet twice play 30 rounds. */
static int nor_prm(int *d)
{
    static const int md[26][2] = {{4, 5}, {4, 8}, {4, 12}, {4, 15}, {4, 19}, {4, 26}, {5, 3}, {5, 6}, {5, 10},
                                  {5, 13}, {5, 16}, {5, 24}, {5, 31}, {7, 26}, {7, 29}, {8, 2}, {8, 5}, {8, 9},
                                  {8, 23}, {8, 30}, {9, 13}, {9, 20}, {9, 27}, {9, 30}, {10, 4}, {10, 18}};
    for (int i = 0; i < 26; i++) d[i] = day(2009, md[i][0], md[i][1]);
    return 26;
}

static void test_calendar_year(void)
{
    int d[SEASON_TEMPLATE_MAX];
    int n = nor_prm(d);
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    int r = season_template_plan(2009, d, n, 30, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 30, "nor_prm 2009: %d dates, want 30 (%s)", r, why);
    if (r != 30) return;
    check_plan("nor_prm 2009", 2009, d, n, r, p);
    CHECK(count_added(p, r) == 4, "nor_prm 2009: %d new dates, want 4", count_added(p, r));
    for (int k = 0; k < r; k++)
        CHECK(!(p[k].day > day(2009, 5, 31) && p[k].day < day(2009, 7, 26)), "nor_prm 2009: a date (%d) in the "
              "summer break", p[k].day);
    CHECK(p[0].day == d[0], "nor_prm 2009: first date %d, class %d", p[0].day, d[0]);
    /* Spread: the new dates are not all in one part of the season. */
    int first = -1, last = -1;
    for (int k = 0; k < r; k++)
        if (p[k].added) { if (first < 0) first = p[k].day; last = p[k].day; }
    CHECK(last - first >= 60, "nor_prm 2009: new dates from %d to %d only", first, last);
}

/* PorSecond 2014: 34 Sundays (two Saturdays) from 24 August 2014 to 17 May 2015. */
static int por_second(int *d)
{
    static const int gaps[33] = {7, 14, 7, 7, 6, 15, 7, 7, 7, 14, 7, 7, 7, 14, 14, 7, 7, 7, 7, 7, 7, 6, 8, 7, 7, 7,
                                 7, 7, 6, 8, 7, 7, 7};
    d[0] = day(2014, 8, 24);
    for (int i = 0; i < 33; i++) d[i + 1] = d[i] + gaps[i];
    return 34;
}

static void test_two_years(void)
{
    int d[SEASON_TEMPLATE_MAX];
    int n = por_second(d);
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    /* 24 clubs: 46 rounds. */
    int r = season_template_plan(2014, d, n, 46, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 46, "por_second 2014: %d dates, want 46 (%s)", r, why);
    if (r == 46) {
        check_plan("por_second 2014", 2014, d, n, r, p);
        CHECK(count_added(p, r) == 12, "por_second 2014: %d new dates", count_added(p, r));
        CHECK(p[0].day == d[0], "por_second 2014: the season starts earlier with room in the gaps");
    }
    /* 16 clubs (2006 to 2011): 30 rounds, the first and the last stay (D5). */
    r = season_template_plan(2014, d, n, 30, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 30, "por_second 16 clubs: %d dates (%s)", r, why);
    if (r == 30) {
        check_plan("por_second 16 clubs", 2014, d, n, r, p);
        CHECK(count_added(p, r) == 0, "por_second 16 clubs: new dates in a shrink");
        CHECK(p[0].day == d[0] && p[29].day == d[33], "por_second 16 clubs: first %d last %d", p[0].day, p[29].day);
        /* Evenly: no two dropped dates next to each other. */
        int prev = -1;
        for (int k = 0; k < r; k++) {
            CHECK(p[k].from - prev <= 2, "por_second 16 clubs: dates %d to %d dropped", prev + 1, p[k].from - 1);
            prev = p[k].from;
        }
    }
    /* The same count: the class's template. */
    r = season_template_plan(2014, d, n, 34, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 34 && count_added(p, r) == 0 && p[33].from == 33, "por_second 18 clubs: not the class's template");
    /* An odd count: 23 clubs play as 24. */
    r = season_template_plan(2014, d, n, 46, SEASON_TEMPLATE_SEQUENTIAL, p, why, sizeof why);
    CHECK(r == 46, "por_second sequential: %d dates (%s)", r, why);
    if (r == 46) check_plan("por_second sequential", 2014, d, n, r, p);
}

/* Mirrored (the French classes, flag 0x80): 17 dates in each half; 20 clubs give 38 rounds, 2 new in each half. */
static void test_mirrored(void)
{
    int d[SEASON_TEMPLATE_MAX];
    int n = por_second(d);
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    int r = season_template_plan(2014, d, n, 38, SEASON_TEMPLATE_MIRRORED, p, why, sizeof why);
    CHECK(r == 38, "mirrored 38: %d dates (%s)", r, why);
    if (r == 38) {
        check_plan("mirrored 38", 2014, d, n, r, p);
        CHECK(count_added(p, 19) == 2 && count_added(p + 19, 19) == 2, "mirrored 38: %d and %d new dates",
              count_added(p, 19), count_added(p + 19, 19));
        CHECK(p[18].day <= d[16] && p[19].day == d[17], "mirrored 38: the halves meet at %d and %d", p[18].day,
              p[19].day);
    }
    r = season_template_plan(2014, d, n, 30, SEASON_TEMPLATE_MIRRORED, p, why, sizeof why);
    CHECK(r == 30, "mirrored 30: %d dates (%s)", r, why);
    if (r == 30)
        CHECK(p[0].day == d[0] && p[14].day == d[16] && p[15].day == d[17] && p[29].day == d[33],
              "mirrored 30: each half keeps its first and last date");
    r = season_template_plan(2014, d, n, 33, SEASON_TEMPLATE_MIRRORED, p, why, sizeof why);
    CHECK(r == 0, "mirrored 33: an odd count of rounds has no halves");
}

/* A template with no gap of 7 days: the season starts earlier, at most 4 weeks; then no room. */
static void test_no_room(void)
{
    int d[SEASON_TEMPLATE_MAX], n = 0;
    for (int w = 0; w < 10; w++) {
        d[n++] = day(2010, 4, 4) + 7 * w;       /* Sunday */
        d[n++] = day(2010, 4, 4) + 7 * w + 3;   /* Wednesday */
    }
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    int r = season_template_plan(2010, d, n, 24, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 24, "earlier start: %d dates (%s)", r, why);
    if (r == 24) {
        check_plan("earlier start", 2010, d, n, r, p);
        CHECK(p[0].day == d[0] - 28 && p[4].day == d[0], "earlier start: first %d, class first %d", p[0].day, d[0]);
        CHECK(season_template_weekday(2010, p[0].day) == 6, "earlier start: not on the class's first weekday");
    }
    why[0] = 0;
    r = season_template_plan(2010, d, n, 25, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 0 && why[0], "no room: %d dates, reason '%s'", r, why);
    /* Not before 1 January. */
    for (int i = 0; i < n; i++) d[i] -= day(2010, 4, 4) - 2;   /* first date Sunday 3 January 2010, day 2 */
    r = season_template_plan(2010, d, n, 22, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 0, "no room before 1 January: %d dates", r);
}

/* BraNatFirst::create_schedule 0x429b50 in 2003: Sundays and Wednesdays from 3 August to 30 November, no gap over
 * 7 days, so no winter break; its 8 week gaps take a Wednesday each. Série A of 24 clubs in two meetings (46 rounds)
 * then starts 11 weeks earlier, within the 13 of season_sizes_bra_earlier; 20 clubs (38 rounds) 3 weeks. */
static void test_brazil(void)
{
    static const int md[][2] = {{8, 3}, {8, 6}, {8, 10}, {8, 13}, {8, 17}, {8, 24}, {8, 27}, {8, 31}, {9, 3},
                                {9, 7}, {9, 14}, {9, 17}, {9, 21}, {9, 28}, {10, 1}, {10, 5}, {10, 8}, {10, 12},
                                {10, 19}, {10, 26}, {11, 2}, {11, 5}, {11, 9}, {11, 12}, {11, 16}, {11, 23}, {11, 30}};
    int d[27];
    for (int i = 0; i < 27; i++) d[i] = day(2003, md[i][0], md[i][1]);
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    int r = season_template_plan(2003, d, 27, 38, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 38, "Brazil 20 clubs: %d dates (%s)", r, why);
    if (r == 38) {
        check_plan("Brazil 20 clubs", 2003, d, 27, r, p);
        CHECK(count_added(p, r) == 11 && p[0].day == d[0] - 21, "Brazil 20 clubs: %d new, first %d (class %d)",
              count_added(p, r), p[0].day, d[0]);
    }
    why[0] = 0;
    r = season_template_plan(2003, d, 27, 46, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 0 && why[0], "Brazil 24 clubs with 4 earlier starts: %d dates", r);
    why[0] = 0;
    r = season_template_plan_from(2003, d, 27, 46, SEASON_TEMPLATE_SHUFFLED, 13, p, why, sizeof why);
    CHECK(r == 46, "Brazil 24 clubs: %d dates (%s)", r, why);
    if (r == 46) {
        CHECK(p[0].day == d[0] - 77 && p[11].day == d[0] && p[45].day == d[26], "Brazil 24 clubs: first %d, class %d",
              p[0].day, d[0]);
        int wed = 0;
        for (int k = 11; k < 46; k++) wed += p[k].added;
        CHECK(wed == 8, "Brazil 24 clubs: %d gap Wednesdays, want 8", wed);
    }
    r = season_template_plan_from(2003, d, 27, 54, SEASON_TEMPLATE_SHUFFLED, 13, p, why, sizeof why);
    CHECK(r == 0, "Brazil 28 clubs in two meetings: %d dates, want no room", r);
}

/* Open item 236: a class date that snaps to its weekday can fall before the date ahead of it. PorSecond 2013/14
 * (the game's log of the 2013 start, run-logs/0.7.2-sz03/run-portugal) makes Sunday 2 March 2014 and then
 * Saturday 1 March 2014; the plan of 42 rounds for 22 clubs failed with "class date 23 is not after date 22". */
static void test_out_of_order(void)
{
    static const int ymd[34][3] = {{2013, 8, 25}, {2013, 9, 1}, {2013, 9, 15}, {2013, 9, 22}, {2013, 9, 29}, {2013, 10, 5}, {2013, 10, 20}, {2013, 10, 27}, {2013, 11, 3}, {2013, 11, 10}, {2013, 11, 24}, {2013, 12, 1}, {2013, 12, 8}, {2013, 12, 15}, {2013, 12, 22}, {2014, 1, 5}, {2014, 1, 19}, {2014, 1, 26}, {2014, 2, 2}, {2014, 2, 9}, {2014, 2, 16}, {2014, 2, 23}, {2014, 3, 2}, {2014, 3, 1}, {2014, 3, 9}, {2014, 3, 16}, {2014, 3, 23}, {2014, 3, 30}, {2014, 4, 6}, {2014, 4, 13}, {2014, 4, 19}, {2014, 4, 27}, {2014, 5, 4}, {2014, 5, 11}};
    int d[34];
    for (int i = 0; i < 34; i++) d[i] = day(ymd[i][0], ymd[i][1], ymd[i][2]) + (ymd[i][0] - 2013) * 365;
    CHECK(d[23] < d[22], "out of order: the class dates 22 and 23 are in order (%d, %d)", d[22], d[23]);
    SeasonTemplateDate p[SEASON_TEMPLATE_MAX];
    char why[128] = "";
    int r = season_template_plan(2013, d, 34, 42, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
    CHECK(r == 42, "out of order: %d dates, want 42 (%s)", r, why);
    if (r != 42) return;
    int seen[34] = {0};
    for (int k = 0; k < r; k++) {
        CHECK(k == 0 || p[k].day > p[k - 1].day, "out of order: date %d (%d) not after date %d (%d)", k, p[k].day,
              k - 1, p[k - 1].day);
        CHECK(p[k].from >= 0 && p[k].from < 34, "out of order: date %d copies record %d", k, p[k].from);
        if (p[k].added) {
            CHECK(p[k].day < d[0] || season_template_weekday(2013, p[k].day) == SEASON_TEMPLATE_WEDNESDAY,
                  "out of order: new date %d is weekday %d", p[k].day, season_template_weekday(2013, p[k].day));
            continue;
        }
        CHECK(d[p[k].from] == p[k].day, "out of order: date %d is %d, record %d is %d", k, p[k].day, p[k].from,
              d[p[k].from]);
        seen[p[k].from]++;
    }
    for (int i = 0; i < 34; i++) CHECK(seen[i] == 1, "out of order: class date %d taken %d times", i, seen[i]);
    CHECK(p[r - 1].day == d[33], "out of order: last date %d, class %d", p[r - 1].day, d[33]);
    /* 2 March (record 22) comes after 1 March (record 23). */
    for (int k = 0; k + 1 < r; k++)
        if (p[k].from == 23 && !p[k].added) CHECK(p[k + 1].from == 22 && !p[k + 1].added, "out of order: 1 March "
                                                  "is not followed by 2 March");
    /* Two class dates on one day: the plan uses one of them and never plays two rounds on one day. */
    d[23] = d[22];
    for (int want = 30; want <= 42; want += 12) {
        why[0] = 0;
        r = season_template_plan(2013, d, 34, want, SEASON_TEMPLATE_SHUFFLED, p, why, sizeof why);
        CHECK(r == want, "one day: %d dates, want %d (%s)", r, want, why);
        for (int k = 1; k < r; k++) CHECK(p[k].day > p[k - 1].day, "one day: dates %d and %d on day %d", k - 1, k,
                                          p[k].day);
        for (int k = 0; k < r; k++) CHECK(p[k].from != 23 || p[k].added, "one day: the dropped record 23 is taken");
    }
    /* A mirrored template keeps its halves: no plan. */
    why[0] = 0;
    r = season_template_plan(2013, d, 34, 42, SEASON_TEMPLATE_MIRRORED, p, why, sizeof why);
    CHECK(r == 0 && why[0], "one day mirrored: %d dates, reason '%s'", r, why);
}

int main(void)
{
    test_brazil();
    test_weekday();
    test_calendar_year();
    test_two_years();
    test_mirrored();
    test_no_room();
    test_out_of_order();
    printf("%s: season_template\n", fails ? "FAIL" : "PASS");
    return fails ? 1 : 0;
}
