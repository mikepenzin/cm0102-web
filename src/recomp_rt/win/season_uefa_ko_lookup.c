/* The lookup of the UEFA knockout dates (season_uefa.h): the row of a season, or for a season after the table its last
 * row moved to the season's weekdays. Pure C, no game memory, so tests/seasons/test_season_uefa_dates.c checks it. */
#include "season_uefa.h"

#include <stddef.h>

/* Days from 1 January 1970 of a date (month 1 to 12), and back (H. Hinnant's civil calendar algorithms). */
static long days_of(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    long doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

static void date_of(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

/* A date of the row of season `from` moved to season `to`: 364 days a year keeps the weekday, and 7 days more
 * each time the date has come more than 3 days before the same day of the month (O378 review). Month 0 to 11 and year
 * offset from the season's first year, as the table's fields. */
static void carry(int from, int to, int day, int month, int year_off, int *nd, int *nm, int *ny_off)
{
    int y = from + year_off, k = to - from;
    long z = days_of(y, month + 1, day) + 364L * k;
    while (z - days_of(y + k, month + 1, day) < -3) z += 7;
    int yy, mm, dd;
    date_of(z, &yy, &mm, &dd);
    *nd = dd, *nm = mm - 1, *ny_off = yy - to;
}

const SeasonUefaKoDate *season_uefa_ko_date(char comp, int season, int round)
{
    const SeasonUefaKoDate *last = NULL;
    for (int i = 0; i < season_uefa_ko_dates_count; i++) {
        const SeasonUefaKoDate *d = &season_uefa_ko_dates_table[i];
        if (d->comp != comp || d->round != round) continue;
        if (d->season == season) return d;
        if (!last || d->season > last->season) last = d;
    }
    /* A season after the table (it ends with 2027/28, the last one UEFA has dates for): the last row's dates moved
     * to the season's weekdays, as tools/seasons/make_uefa_dates.py carries 2026/27 into 2027/28 (Q547 update).
     * An earlier season with no row (a format before the table) keeps the format's dates. */
    if (!last || season <= last->season) return NULL;
    static SeasonUefaKoDate out;
    out = *last;
    out.season = season;
    carry(last->season, season, last->draw_day, last->draw_month, last->draw_year, &out.draw_day, &out.draw_month,
          &out.draw_year);
    carry(last->season, season, last->day, last->month, last->year_off, &out.day, &out.month, &out.year_off);
    /* The game's season ends on 1 June: a final after 31 May is played on 31 May (the tool's rule). */
    if (round == SEASON_UEFA_KO_FINAL && out.year_off == 1 && (out.month > 4))
        out.day = 31, out.month = 4;
    return &out;
}

int season_uefa_ko_dates_log(char comp, int season)
{
    static int logged[3] = {0, 0, 0};
    int *l = &logged[comp == 'c' ? 0 : comp == 'u' ? 1 : 2];
    if (*l == season) return 0;
    *l = season;
    return 1;
}

