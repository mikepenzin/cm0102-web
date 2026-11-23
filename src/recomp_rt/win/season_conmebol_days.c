/* The match days of the South American section of World Cup qualifying in a season game (open item 163 of 0.6.2).
 *
 * The cause. WcSouthAmericanLeague::create_schedule 0x920590 gives the original's template of the 2002 cycle (18
 * rounds from March 2000 to November 2001, moved to each cycle), and end_by_october (season_intl_qual.c) only moves
 * its end. Each fixture then moves on its own to the nearest free FIFA day of its year (season_friendly_dates.c
 * fifa_target_by). The template puts 10 rounds in the year two years before the finals, from February, and that
 * year's FIFA days within the stage hold 9: in run set game a2002s3 the whole matchday of 1 August 2012 kept its
 * day (Argentina v Paraguay among them), and the Copa Sudamericana of 31 July (item 95 keeps it off the FIFA days
 * only) met it with three of Huracan's players on duty (get_international_duty_club 0x7757d0: the day before to the
 * day after). The 2098 cycle of a2006s3 did the same on 15 August 2096, and 2080 and 2084 kept November days.
 *
 * Real life. The cycles 2006 to 2026 played 18 matchdays in the FIFA windows from September or October three years
 * before the finals to September or October the year before, mostly two a window (en.wikipedia.org, "<year> FIFA
 * World Cup qualification (CONMEBOL)", the matchday sections). Not exactly two: 2006 had single matchdays in March
 * and November 2004, 2022 three in September and October 2021, and 2026 left out March and June 2024 for the Copa
 * America. So a section of 18 rounds of those cycles takes the real days (real[] below: the first day of each
 * matchday). The 2022 cycle is the exception: its real days ran to 29 March 2022, and the game ends the section by
 * 10 October 2021 for its tie of November 2021 (season_intl_qual.c real_end), so it takes the window rule below
 * from its real first day, 8 October 2020.
 *
 * The window rule (2022, every cycle after the table, and a section of another round count; an inference from the
 * real cycles): the FIFA days of the span, in each FIFA window (FIFA days at most WINDOW_GAP apart) its first day
 * and the next at least MIN_GAP days later, the windows with two such days first, then the other days (a third day
 * of a window, a window of one day), earliest first; with more two-day windows than the rounds need, windows spread
 * evenly from the first to the last of the span. A day in a South American final's days is left out. When the
 * span holds too few, its first day moves back a week at a time (the list has no FIFA days from March to September
 * 2020). The span after the table: 1 September three years before the finals to 15 October the year before (the
 * real first days were in September or October, the latest real last day 15 October 2013). */
#include "season_conmebol_days.h"
#include "season_uefa_days.h"

#include <stdlib.h>

#define WINDOW_GAP 4                    /* FIFA days of one window: Thursday to Tuesday, at most 4 days apart */
#define MIN_GAP 3                       /* two rounds of a window: Thursday and Sunday, Friday and Tuesday */
#define SLOTS_MAX 256
#define BACK_DAYS 400                   /* how far the first day may move back */

#define REAL_ROUNDS 18
static const struct {
    int wc;
    short day[REAL_ROUNDS][3];          /* year, month, day */
} real[] = {
    {2006, {{2003, 9, 6}, {2003, 9, 9}, {2003, 11, 15}, {2003, 11, 18}, {2004, 3, 30}, {2004, 6, 1}, {2004, 6, 5},
            {2004, 9, 4}, {2004, 10, 9}, {2004, 10, 12}, {2004, 11, 17}, {2005, 3, 26}, {2005, 3, 29}, {2005, 6, 4},
            {2005, 6, 7}, {2005, 9, 3}, {2005, 10, 8}, {2005, 10, 12}}},
    {2010, {{2007, 10, 13}, {2007, 10, 16}, {2007, 11, 17}, {2007, 11, 20}, {2008, 6, 14}, {2008, 6, 17},
            {2008, 9, 6}, {2008, 9, 9}, {2008, 10, 11}, {2008, 10, 14}, {2009, 3, 28}, {2009, 3, 31}, {2009, 6, 6},
            {2009, 6, 10}, {2009, 9, 5}, {2009, 9, 9}, {2009, 10, 10}, {2009, 10, 14}}},
    {2014, {{2011, 10, 7}, {2011, 10, 11}, {2011, 11, 11}, {2011, 11, 15}, {2012, 6, 2}, {2012, 6, 9}, {2012, 9, 7},
            {2012, 9, 11}, {2012, 10, 12}, {2012, 10, 16}, {2013, 3, 22}, {2013, 3, 26}, {2013, 6, 7}, {2013, 6, 11},
            {2013, 9, 6}, {2013, 9, 10}, {2013, 10, 11}, {2013, 10, 15}}},
    {2018, {{2015, 10, 8}, {2015, 10, 13}, {2015, 11, 12}, {2015, 11, 17}, {2016, 3, 24}, {2016, 3, 29}, {2016, 9, 1},
            {2016, 9, 6}, {2016, 10, 6}, {2016, 10, 11}, {2016, 11, 10}, {2016, 11, 15}, {2017, 3, 23}, {2017, 3, 28},
            {2017, 8, 31}, {2017, 9, 5}, {2017, 10, 5}, {2017, 10, 10}}},
    {2026, {{2023, 9, 7}, {2023, 9, 12}, {2023, 10, 12}, {2023, 10, 17}, {2023, 11, 16}, {2023, 11, 21}, {2024, 9, 5},
            {2024, 9, 10}, {2024, 10, 10}, {2024, 10, 15}, {2024, 11, 14}, {2024, 11, 19}, {2025, 3, 20},
            {2025, 3, 25}, {2025, 6, 5}, {2025, 6, 10}, {2025, 9, 4}, {2025, 9, 9}}},
};

int season_conmebol_real_day(long z)
{
    for (size_t i = 0; i < sizeof real / sizeof real[0]; i++)
        for (int r = 0; r < REAL_ROUNDS; r++)
            if (season_abs_day(real[i].day[r][0], real[i].day[r][1], real[i].day[r][2]) == z) return 1;
    return 0;
}

void season_conmebol_round_fields(long z, int base, int *doy, int *year_off, int *flag)
{
    int y, m, d, wd = (int)(((z % 7) + 7 + 3) % 7);          /* 1 January 1970 was a Thursday; 0 Monday */
    season_from_abs_day(z, &y, &m, &d);
    *doy = season_day_of_year(y, m, d);
    *year_off = y - base;
    *flag = wd >= 5 ? 1 : 2;
}

static const struct {
    int wc, y0, m0, d0, y1, m1, d1;
} spans[] = {
    /* matchday 1 and matchday 18 (en.wikipedia.org, "<year> FIFA World Cup qualification (CONMEBOL)") */
    {2006, 2003, 9, 6, 2005, 10, 12},
    {2010, 2007, 10, 13, 2009, 10, 14},
    {2014, 2011, 10, 7, 2013, 10, 15},
    {2018, 2015, 10, 8, 2017, 10, 10},
    /* 2022: matchday 1 on 8 October 2020 (moved from March 2020); the game ends the section by 10 October 2021 for
     * its tie in November 2021 (season_intl_qual.c real_end), the real end was 29 March 2022 */
    {2022, 2020, 10, 8, 2021, 10, 10},
    {2026, 2023, 9, 7, 2025, 9, 9},
};

void season_conmebol_span(int wc, long *first, long *last)
{
    for (size_t i = 0; i < sizeof spans / sizeof spans[0]; i++)
        if (spans[i].wc == wc) {
            *first = season_abs_day(spans[i].y0, spans[i].m0, spans[i].d0);
            *last = season_abs_day(spans[i].y1, spans[i].m1, spans[i].d1);
            return;
        }
    *first = season_abs_day(wc - 3, 9, 1);
    *last = season_abs_day(wc - 1, 10, 15);
}

static int by_day(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return x < y ? -1 : x > y;
}

/* The FIFA days from first to last, in order. */
static int fifa_days(long first, long last, long *out, int max)
{
    int n = 0, y0, y1, m, d;
    season_from_abs_day(first, &y0, &m, &d);
    season_from_abs_day(last, &y1, &m, &d);
    for (int y = y0; y <= y1; y++) {
        int doys[SEASON_FIFA_YEAR_MAX], k = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX);
        long jan1 = season_abs_day(y, 1, 1);
        for (int i = 0; i < k && n < max; i++)
            if (jan1 + doys[i] >= first && jan1 + doys[i] <= last) out[n++] = jan1 + doys[i];
    }
    qsort(out, (size_t)n, sizeof *out, by_day);
    return n;
}

static int pick(long first, long last, long earliest, SeasonConmebolBusy busy, int rounds, long out[])
{
    long days[SLOTS_MAX], best[SLOTS_MAX], rest[SLOTS_MAX];
    int n = fifa_days(first, last, days, SLOTS_MAX), nb = 0, nr = 0;
    for (int i = 0; i < n;) {
        int j = i + 1;
        while (j < n && days[j] - days[j - 1] <= WINDOW_GAP) j++;          /* days[i..j-1]: one window */
        long taken[8];
        int nt = 0;
        for (int k = i; k < j && nt < 8; k++)
            if (days[k] >= earliest && !(busy && busy(days[k])) && (!nt || days[k] - taken[nt - 1] >= MIN_GAP))
                taken[nt++] = days[k];
        for (int k = 0; k < nt; k++) {
            if (nt >= 2 && k < 2) best[nb++] = taken[k];
            else rest[nr++] = taken[k];
        }
        i = j;
    }
    if (nb + nr < rounds) return 0;
    int m = 0, windows = nb / 2, need = rounds / 2;
    if (windows > need && need > 1) {
        /* More two-day windows than the rounds need: windows spread from the first to the last, so the section
         * runs through its span as the real ones do. */
        for (int k = 0; k < need; k++) {
            int w = (int)((long)k * (windows - 1) * 2 / (need - 1) + 1) / 2;
            out[m++] = best[2 * w], out[m++] = best[2 * w + 1];
        }
        for (int k = 0; k < nb && m < rounds; k++) {          /* an odd round: a day of a window not taken */
            int seen = 0;
            for (int j = 0; j < m; j++) seen |= out[j] == best[k];
            if (!seen) out[m++] = best[k];
        }
    } else {
        for (int k = 0; k < nb && m < rounds; k++) out[m++] = best[k];
    }
    for (int k = 0; k < nr && m < rounds; k++) out[m++] = rest[k];
    qsort(out, (size_t)m, sizeof *out, by_day);
    return m;
}

/* The original makes the fixtures on the Saturday near 25 January (set_up_rules 0x921040: Date 0x549f70 with 25,
 * January, the campaign year and weekday 5, 0x9210a4..0x92110c), 25 to 28 days before its first round of mid to
 * late February. The same lead, four weeks, before the real first round (an inference from the original). */
#define MAKE_LEAD 28

long season_conmebol_make_day(long first_round, long original, long today)
{
    if (first_round > original) return original;
    long m = first_round - MAKE_LEAD;
    return m > today ? m : today + 1;
}

int season_conmebol_days(int wc, int rounds, long earliest, SeasonConmebolBusy busy, long out[])
{
    long first, last;
    if (rounds <= 0 || rounds > SLOTS_MAX) return 0;
    for (size_t i = 0; i < sizeof real / sizeof real[0] && rounds == REAL_ROUNDS; i++)
        if (real[i].wc == wc) {
            for (int r = 0; r < REAL_ROUNDS; r++)
                out[r] = season_abs_day(real[i].day[r][0], real[i].day[r][1], real[i].day[r][2]);
            if (out[0] >= earliest) return rounds;          /* made later than its first day: the window rule */
        }
    season_conmebol_span(wc, &first, &last);
    for (long back = 0; back <= BACK_DAYS; back += 7) {
        if (back && first - back + 7 < earliest) break;                   /* nothing new before the earliest day */
        if (pick(first - back, last, earliest, busy, rounds, out) == rounds) return rounds;
    }
    return 0;
}
