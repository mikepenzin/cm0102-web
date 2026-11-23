/* The Irish calendar move of 2003: the rules and the dates (season_calendar.h). Pure: no game memory. */
#include "season_calendar.h"
#include "season_template.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define UPDATE_MONTH 2
#define UPDATE_DAY 20               /* 20 February */
#define CUP_ROOM 14                 /* a cup ends 14 days before the update day (replays, second legs) */

int season_calendar_state(int start_year, int changing_rules, int edition)
{
    if (start_year >= SEASON_CAL_FIRST) return SEASON_CAL_YEAR;
    if (!changing_rules || edition < SEASON_CAL_FIRST - 1) return SEASON_CAL_WINTER;
    return edition == SEASON_CAL_FIRST - 1 ? SEASON_CAL_MOVE : SEASON_CAL_YEAR;
}

int season_calendar_doy(int year, int month, int day)
{
    static const int cum[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = season_template_year_days(year) == 366;
    return cum[month - 1] + (leap && month > 2) + day - 1;
}

int season_calendar_update_doy(void) { return season_calendar_doy(2001, UPDATE_MONTH, UPDATE_DAY); }

static int at(int base, int year, int month, int day)
{
    return season_template_day(base, year - base, season_calendar_doy(year, month, day));
}

void season_calendar_dates(int state, int y, SeasonCalDate *start, SeasonCalDate *end, SeasonCalDate *part_start,
                           SeasonCalDate *part_end)
{
    /* The calendar season: from 1 March (the first rounds come from mid-March, real 10 April 2003 and 19 March
     * 2004) to 30 November. The parts of the short season are those of a calendar season of the same year, so the
     * step of the February change (game_season_update_pass 0x5d5bfc..0x5d5c56: + the step +0x1d to +0x35 and +0x44)
     * gives the calendar season of the next year. */
    *part_start = (SeasonCalDate){1, 3, y};
    *part_end = (SeasonCalDate){30, 11, y};
    if (state == SEASON_CAL_MOVE) {
        *start = (SeasonCalDate){1, 8, y};              /* the class's first rounds of August */
        *end = (SeasonCalDate){31, 1, y + 1};           /* real last round 26 January 2003 */
    } else {
        *start = *part_start;
        *end = *part_end;
    }
}

/* The whole weeks k that bring day `last` nearest `target`; a tie takes the earlier. */
static int nearest_weeks(int last, int target)
{
    int k = (int)floor((target - last) / 7.0);
    return abs(last + 7 * (k + 1) - target) < abs(last + 7 * k - target) ? k + 1 : k;
}

int season_calendar_league_shift(int base, const int *days, int n)
{
    int last = INT_MIN;
    for (int i = 0; i < n; i++)
        if (days[i] > last) last = days[i];
    return n > 0 ? 7 * nearest_weeks(last, at(base, base, 11, 30)) : 0;
}

int season_calendar_move_cut(int base) { return at(base, base + 1, 2, 1); }

int season_calendar_cup_cut(int base, int state)
{
    (void)state;    /* both states end at the update day of base + 1 */
    return at(base, base + 1, UPDATE_MONTH, UPDATE_DAY) - CUP_ROOM;
}

int season_calendar_cup_target(const char *file, int state, int base)
{
    if (!file || strcmp(file, "ire_chal_cup")) return INT_MIN;
    if (state == SEASON_CAL_YEAR) return at(base, base, 10, 26);   /* 2003 final, 26 October (RSSSF ier03a) */
    if (state == SEASON_CAL_MOVE) return at(base, base, 10, 27);   /* 2002-03 final, 27 October 2002 (RSSSF ier03) */
    return INT_MIN;
}

static int floor_div7(int x) { return (int)floor(x / 7.0); }
static int ceil_div7(int x) { return (int)ceil(x / 7.0); }

int season_calendar_cup_plan(const int *days, int n, int earliest, int cut, int target, int *out)
{
    if (n <= 0) return 0;
    int first = days[0], last = days[0];
    for (int i = 0; i < n; i++) {
        out[i] = days[i];
        if (days[i] < first) first = days[i];
        if (days[i] > last) last = days[i];
    }
    int kmin = ceil_div7(earliest - first), kmax = floor_div7(cut - 1 - last);
    if (kmin <= kmax) {
        int k = target == INT_MIN ? 0 : nearest_weeks(last, target);
        if (k < kmin) k = kmin;
        if (k > kmax) k = kmax;
        for (int i = 0; i < n; i++) out[i] = days[i] + 7 * k;
        return 1;
    }
    /* Pressed: the first date moves to the earliest, the others keep their share of the span, each by whole weeks. */
    int f0 = first + 7 * kmin;
    if (f0 >= cut || last == first) return 0;
    double scale = (double)(cut - 1 - f0) / (double)(last - first);
    for (int i = 0; i < n; i++) {
        double t = f0 + (days[i] - first) * scale;
        int o = days[i] + 7 * floor_div7((int)floor(t) - days[i]);
        while (i > 0 && o <= out[i - 1]) o += 7;
        if (o >= cut || o < earliest) {
            for (int j = 0; j < n; j++) out[j] = days[j];
            return 0;
        }
        out[i] = o;
    }
    return 2;
}

void season_calendar_award(int state, int *day, int *month0)
{
    if (state == SEASON_CAL_WINTER) {
        *day = 1;
        *month0 = 4;
    } else {
        *day = 16;
        *month0 = 1;
    }
}

int season_calendar_two_years(int state, int year)
{
    return state == SEASON_CAL_YEAR && year > 0 && year < SEASON_CAL_FIRST;
}

int season_calendar_league(const char *file)
{
    return file && (!strcmp(file, "ire_prm") || !strcmp(file, "ire_first"));
}

int season_calendar_cup(const char *file)
{
    return file && (!strcmp(file, "ire_chal_cup") || !strcmp(file, "ire_munster_cup") ||
                    !strcmp(file, "ire_pres_cup") || !strcmp(file, "ire_leinster_cup"));
}

int season_calendar_label(const char *file, int state, int edition)
{
    if (state == SEASON_CAL_WINTER || !file) return 0;
    return season_calendar_league(file) || !strcmp(file, "ire_chal_cup") || !strcmp(file, "ire_lge_cup") ? edition : 0;
}
