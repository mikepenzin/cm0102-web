/* The date template of a league of any size (season_template.h). */
#include "season_template.h"

#include <stdio.h>
#include <string.h>

static int leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int season_template_year_days(int year) { return leap(year) ? 366 : 365; }

/* 1 January of year 1 (proleptic Gregorian) is a Monday, weekday 0 as Date::get_weekday 0x54a3f0. */
int season_template_weekday(int year, int day)
{
    long y = year - 1;
    long days = 365 * y + y / 4 - y / 100 + y / 400 + day;
    return (int)(((days % 7) + 7) % 7);
}

int season_template_day(int base_year, int year_offset, int day_of_year)
{
    int d = day_of_year;
    for (int y = 0; y < year_offset; y++) d += season_template_year_days(base_year + y);
    for (int y = 0; y > year_offset; y--) d -= season_template_year_days(base_year + y - 1);
    return d;
}

void season_template_split(int base_year, int day, int *year_offset, int *day_of_year)
{
    int y = 0;
    while (day >= season_template_year_days(base_year + y)) day -= season_template_year_days(base_year + y++);
    while (day < 0) day += season_template_year_days(base_year + --y);
    *year_offset = y;
    *day_of_year = day;
}

/* The Wednesday of the gap after days[i], at least 2 days from both ends, or -1. */
static int gap_wednesday(int base_year, const int *days, int i)
{
    for (int w = days[i] + 2; w <= days[i + 1] - 2; w++)
        if (season_template_weekday(base_year, w) == SEASON_TEMPLATE_WEDNESDAY) return w;
    return -1;
}

/* The record a new Wednesday copies: the nearest class Wednesday of the part (its slots and its time byte), else
 * the date before the gap. */
static int prototype(int base_year, const int *days, int lo, int hi, int i)
{
    for (int k = 0; k < hi - lo; k++) {
        if (i - k >= lo && season_template_weekday(base_year, days[i - k]) == SEASON_TEMPLATE_WEDNESDAY) return i - k;
        if (i + 1 + k < hi && season_template_weekday(base_year, days[i + 1 + k]) == SEASON_TEMPLATE_WEDNESDAY)
            return i + 1 + k;
    }
    return i;
}

/* The plan of the part [lo, hi) of the class's days for `want` dates; returns the dates written, or -1. */
static int plan_part(int base_year, const int *days, int lo, int hi, int want, int largest, int earlier,
                     int earlier_max, SeasonTemplateDate *out, char *why, int why_size)
{
    int count = hi - lo, n = 0;
    if (want <= count) {
        for (int k = 0; k < want; k++) {
            /* D5: index round(k (count - 1) / (want - 1)); one date keeps the last. */
            int at = want == 1 ? count - 1 : (2 * k * (count - 1) + (want - 1)) / (2 * (want - 1));
            out[n++] = (SeasonTemplateDate){days[lo + at], lo + at, 0};
        }
        return n;
    }
    int extra = want - count, gap_at[SEASON_TEMPLATE_MAX], wed[SEASON_TEMPLATE_MAX], gaps = 0;
    for (int i = lo; i + 1 < hi; i++) {
        int len = days[i + 1] - days[i], w;
        if (len < 7 || len > 14 || len == largest || (w = gap_wednesday(base_year, days, i)) < 0) continue;
        gap_at[gaps] = i;
        wed[gaps++] = w;
    }
    int use = extra < gaps ? extra : gaps, rest = extra - use;
    if (rest > 0 && (!earlier || rest > earlier_max || days[lo] - 7 * rest < 0)) {
        char more[48];
        snprintf(more, sizeof more, "more than %d earlier starts", earlier_max);
        snprintf(why, (size_t)why_size, "%d gaps of 7 to 14 days for %d new dates, %s", gaps, extra,
                 !earlier ? "no earlier start in the second half" : rest > earlier_max ? more :
                 "an earlier start before 1 January");
        return -1;
    }
    char take[SEASON_TEMPLATE_MAX] = {0};
    for (int k = 0; k < use; k++) take[(2 * k + 1) * gaps / (2 * use)] = 1;   /* spread evenly over the gaps */
    for (int s = rest; s >= 1; s--) out[n++] = (SeasonTemplateDate){days[lo] - 7 * s, lo, 1};
    for (int i = lo, g = 0; i < hi; i++) {
        out[n++] = (SeasonTemplateDate){days[i], i, 0};
        if (g < gaps && gap_at[g] == i) {
            if (take[g]) out[n++] = (SeasonTemplateDate){wed[g], prototype(base_year, days, lo, hi, i), 1};
            g++;
        }
    }
    return n;
}

int season_template_plan(int base_year, const int *days, int count, int rounds, int mode, SeasonTemplateDate *out,
                      char *why, int why_size)
{
    return season_template_plan_from(base_year, days, count, rounds, mode, SEASON_TEMPLATE_EARLIER, out, why, why_size);
}

int season_template_plan_from(int base_year, const int *days, int count, int rounds, int mode, int earlier_max,
                              SeasonTemplateDate *out, char *why, int why_size)
{
    if (why && why_size > 0) why[0] = 0;
    char none[1];
    if (!why || why_size <= 0) { why = none; why_size = 1; }
    if (count < 1 || count > SEASON_TEMPLATE_MAX || rounds < 1 || rounds > SEASON_TEMPLATE_MAX) {
        snprintf(why, (size_t)why_size, "%d class dates for %d rounds", count, rounds);
        return 0;
    }
    /* The class's dates in the order of the calendar. A class date snaps to its weekday (Date 0x54a2d0), so it can
     * fall before the date ahead of it: PorSecond 2013/14 makes Sunday 2 March 2014 and then Saturday 1 March. The
     * plan works on the sorted dates; "from" names the class's own index again at the end. */
    int order[SEASON_TEMPLATE_MAX], sorted[SEASON_TEMPLATE_MAX];
    for (int i = 0; i < count; i++) {
        int j = i;
        for (; j > 0 && days[order[j - 1]] > days[i]; j--) order[j] = order[j - 1];
        order[j] = i;
    }
    for (int i = 0; i < count; i++) sorted[i] = days[order[i]];
    /* Two class dates on one day (ItaSerB 2003/04 makes 18 January 2004 twice, ItaSerC1a 2013/14 2 March 2014):
     * a plan that kept both would play two rounds on one day. A shuffled or sequential plan uses the first record of
     * the day and drops the other. The halves of a mirrored template cannot lose a date, so it keeps the class's. */
    int distinct = 0;
    for (int i = 0; i < count; i++) {
        if (distinct && sorted[i] == sorted[distinct - 1]) {
            if (mode == SEASON_TEMPLATE_MIRRORED) {
                snprintf(why, (size_t)why_size, "class dates %d and %d are on one day", order[distinct - 1], order[i]);
                return 0;
            }
            continue;
        }
        order[distinct] = order[i];
        sorted[distinct++] = sorted[i];
    }
    count = distinct;
    days = sorted;
    int largest = 0;
    for (int i = 0; i + 1 < count; i++)
        if (days[i + 1] - days[i] > largest) largest = days[i + 1] - days[i];
    /* A template with no gap over 7 days has no winter break: its 7-day gaps are its weeks (the Brazilian classes,
     * Sundays and Wednesdays from August to December, 0x429b50 and 0x42c140; 0.7.2 slice 17, lead decision). */
    if (largest <= 7) largest = 0;
    int n;
    if (mode == SEASON_TEMPLATE_MIRRORED) {
        if (count % 2 || rounds % 2) {
            snprintf(why, (size_t)why_size, "a mirrored template of %d dates for %d rounds has no halves", count,
                     rounds);
            return 0;
        }
        int a = plan_part(base_year, days, 0, count / 2, rounds / 2, largest, 1, earlier_max, out, why, why_size);
        if (a < 0) return 0;
        int b = plan_part(base_year, days, count / 2, count, rounds / 2, largest, 0, earlier_max, out + a, why, why_size);
        if (b < 0) return 0;
        n = a + b;
    } else {
        n = plan_part(base_year, days, 0, count, rounds, largest, 1, earlier_max, out, why, why_size);
        if (n < 0) return 0;
    }
    if (n != rounds) return 0;
    for (int k = 0; k < n; k++) out[k].from = order[out[k].from];
    return n;
}
