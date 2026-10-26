/* The match days of the UEFA group stages and league phases (row A18, Q547 update of 26 October 2026, O378).
 * Data and pure functions (season_uefa_days.h); ctest season_uefa_days checks every running season from 2002/03 to
 * 2030/31 against the FIFA days (tests/seasons/test_season_uefa_days.c). */
#include "season_uefa_days.h"

#include <stddef.h>

/* ---- Dates ---- */

static int is_leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int days_in(int y) { return is_leap(y) ? 366 : 365; }

/* 0 Monday to 6 Sunday (Sakamoto, 0 Sunday, moved). */
static int weekday_of(int y, int m, int d)
{
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y--;
    return ((y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7 + 6) % 7;
}

static void add_days(int *y, int *m, int *d, int n)        /* m 1 to 12 */
{
    int doy = season_day_of_year(*y, *m, *d) + n;
    while (doy < 0) doy += days_in(--*y);
    while (doy >= days_in(*y)) doy -= days_in((*y)++);
    season_from_day_of_year(*y, doy, m, d);
}

/* Days from 1 January 1970 (month 1 to 12), and back (H. Hinnant's civil calendar algorithms, as
 * season_uefa_ko_lookup.c). */
static long abs_day(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

static void from_abs_day(long z, int *y, int *m, int *d)
{
    z += 719468;
    long era = (z >= 0 ? z : z - 146096) / 146097, doe = z - era * 146097;
    long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yoe + era * 400 + (*m <= 2));
}

void season_weekday_date(int y, int m, int d, int wd, int *oy, int *om, int *od)
{
    int mm = m + 1, k = ((weekday_of(y, mm, d) - wd) % 7 + 7) % 7;
    if (k > 3) k -= 7;                      /* 0x54a2d0: the difference to the weekday, -3 to 3, taken off */
    add_days(&y, &mm, &d, -k);
    *oy = y, *om = mm - 1, *od = d;
}

int season_fifa_meets(int y, int m, int d)
{
    int doy = season_day_of_year(y, m, d), prev = -100, next = 1000;
    for (int dy = -1; dy <= 1; dy++) {      /* the FIFA days of the year and those beside it, as days of year y */
        int yy = y + dy, base = dy < 0 ? -days_in(yy) : dy > 0 ? days_in(y) : 0;
        for (int mm = 1; mm <= 12; mm++)
            for (int want = 1; want <= 31; want++) {
                int f = season_fifa_day(yy, mm, want);
                if (!f) break;
                int fd = base + season_day_of_year(yy, mm, f);
                if (fd - doy >= -1 && fd - doy <= 1) return 1;
                if (fd < doy && fd > prev) prev = fd;
                if (fd > doy && fd < next) next = fd;
            }
    }
    return next - prev <= 6;
}

/* ---- The league phase (2024/25 on) ----
 *
 * The real match days of each published season, and for a later season those of the last published one on the
 * same weekday (O378): the Champions League on Wednesdays (its Tuesday and Wednesday matches in one day), the
 * Europa League and Conference League on Thursdays. Weekday 2 and 3 as the original's Champions League and UEFA
 * Cup round dates (0x584171, 0x9026a4). From 2026/27 FIFA's September window is a double window (21 September
 * to 6 October 2026, FIFA Men's International Match Calendar 2023-2030), so the 2024/25 days would put matchdays
 * 1 and 2 of a later season in it (a duty move of Lyon v Hajduk Split on 20 September 2028 in a test game). A day
 * of a later season that meets a FIFA day (season_fifa_meets) moves a week on, and the days after it keep a week
 * apart (matchday 5 of 2028/29: 22 November, the day after the November window, moves to 29 November).
 * Sources: 2024/25 docs/research/2026-10-18-uefa-access-and-round-names.md (Slice 25, Task C). 2025/26 and
 * 2026/27: uefa.com fixture pages of each competition, with laligaexpert.com (2025/26 Europa League and
 * Conference League), tntsports.co.uk (2025/26 Champions League), si.com (2026/27 Champions League) and
 * flashscore.com (2026/27 Europa League and Conference League), read 26 October 2026. A matchday over several days
 * takes its Wednesday (Champions League) or Thursday. */
static const SeasonLeagueDay CL_DAYS[3][8] = {
    {{18, 8, 0}, {2, 9, 0}, {23, 9, 0}, {6, 10, 0}, {27, 10, 0}, {11, 11, 0}, {22, 0, 1}, {29, 0, 1}},  /* 2024/25 */
    {{17, 8, 0}, {1, 9, 0}, {22, 9, 0}, {5, 10, 0}, {26, 10, 0}, {10, 11, 0}, {21, 0, 1}, {28, 0, 1}},  /* 2025/26 */
    {{9, 8, 0}, {14, 9, 0}, {21, 9, 0}, {4, 10, 0}, {25, 10, 0}, {9, 11, 0}, {20, 0, 1}, {27, 0, 1}},   /* 2026/27 */
};
static const SeasonLeagueDay EL_DAYS[3][8] = {
    {{26, 8, 0}, {3, 9, 0}, {24, 9, 0}, {7, 10, 0}, {28, 10, 0}, {12, 11, 0}, {23, 0, 1}, {30, 0, 1}},
    {{25, 8, 0}, {2, 9, 0}, {23, 9, 0}, {6, 10, 0}, {27, 10, 0}, {11, 11, 0}, {22, 0, 1}, {29, 0, 1}},
    {{17, 8, 0}, {15, 9, 0}, {22, 9, 0}, {5, 10, 0}, {26, 10, 0}, {10, 11, 0}, {21, 0, 1}, {28, 0, 1}},
};
static const SeasonLeagueDay ECL_DAYS[3][6] = {
    {{3, 9, 0}, {24, 9, 0}, {7, 10, 0}, {28, 10, 0}, {12, 11, 0}, {19, 11, 0}},
    {{2, 9, 0}, {23, 9, 0}, {6, 10, 0}, {27, 10, 0}, {11, 11, 0}, {18, 11, 0}},
    {{15, 9, 0}, {22, 9, 0}, {5, 10, 0}, {26, 10, 0}, {10, 11, 0}, {17, 11, 0}},
};
#define LAST_PUBLISHED 2026

/* The day of a date k years on (season_uefa_ko_lookup.c, carry). */
static long carry(int y, int m, int d, int k)
{
    long z = abs_day(y, m, d) + 364L * k;
    while (z - abs_day(y + k, m, d) < -3) z += 7;
    return z;
}

const SeasonLeagueDay *season_uefa_league_days(char comp, int season, int *weekday)
{
    static SeasonLeagueDay later[3][8];
    int k = season <= 2024 ? 0 : season == 2025 ? 1 : 2, c = comp == 'c' ? 0 : comp == 'k' ? 2 : 1;
    int n = c == 2 ? 6 : 8;
    const SeasonLeagueDay *base = c == 0 ? CL_DAYS[k] : c == 2 ? ECL_DAYS[k] : EL_DAYS[k];
    *weekday = c == 0 ? 2 : 3;
    if (season <= LAST_PUBLISHED) return base;
    long prev = 0;
    for (int i = 0; i < n; i++) {
        /* 364 days a year keeps the weekday, 7 days more when the date has come more than 3 days early: the rule of
         * the knockout dates after their table (season_uefa_ko_lookup.c, carry), in its two steps (the table carries
         * 2026/27 into 2027/28, the lookup carries 2027/28 on), so the draws stay after the days. */
        int y = LAST_PUBLISHED + base[i].year_off, m = base[i].month + 1, d = base[i].day;
        long z = carry(y, m, d, 1);
        if (season > LAST_PUBLISHED + 1) {
            from_abs_day(z, &y, &m, &d);
            z = carry(y, m, d, season - LAST_PUBLISHED - 1);
        }
        if (i && z < prev + 7) z = prev + 7;            /* the matchdays a week apart at least */
        from_abs_day(z, &y, &m, &d);
        while (season_fifa_meets(y, m, d)) z += 7, from_abs_day(z, &y, &m, &d);
        later[c][i].day = d, later[c][i].month = m - 1, later[c][i].year_off = y - season;
        prev = z;
    }
    return later[c];
}

/* ---- The Champions League group stages (to 2023/24) ----
 *
 * The original's days (create_round_table 0x583c50): phase 1, groups A to D (stages 0 to 3, 0x583d38..0x583dbe)
 * 12 September (Tuesday), 20 and 27 September (Wednesday), 17 October (Tuesday), 25 October (Wednesday),
 * 7 November (Tuesday); groups E to H (0x583dc7..0x583e45) 13 September (Wednesday), 19 and 26 September
 * (Tuesday), 18 October (Wednesday), 24 October (Tuesday), 8 November (Wednesday). Phase 2 (stages 8 to 11,
 * 0x583ea5..0x583fd6): 21 and 22 November, 6 and 5 December, 14 and 13 February, 20 and 21 February, 7 and 6
 * March, 13 and 14 March. Each moved to its weekday in the season's year, they met FIFA days in 14 of the 22
 * running seasons from 2002/03 to 2023/24 (ctest season_uefa_days): in 2008 Liverpool fielded 7 at Bordeaux on
 * 10 September, the day of a World Cup qualifier (match_eng.cpp 652). So every season takes its real days: the
 * Tuesday of each matchday's week, with the original's weekday per group (the groups of a matchday on Tuesday
 * and Wednesday, as UEFA plays them). Sources: see CL_REAL. */
static const SeasonLeagueDay CL_P1[2][6] = {
    {{12, 8, 0}, {20, 8, 0}, {27, 8, 0}, {17, 9, 0}, {25, 9, 0}, {7, 10, 0}},
    {{13, 8, 0}, {19, 8, 0}, {26, 8, 0}, {18, 9, 0}, {24, 9, 0}, {8, 10, 0}},
};
static const int CL_P1_WD[2][6] = {{1, 2, 2, 1, 2, 1}, {2, 1, 1, 2, 1, 2}};
static const SeasonLeagueDay CL_P2[2][6] = {
    {{21, 10, 0}, {6, 11, 0}, {14, 1, 1}, {20, 1, 1}, {7, 2, 1}, {13, 2, 1}},
    {{22, 10, 0}, {5, 11, 0}, {13, 1, 1}, {21, 1, 1}, {6, 2, 1}, {14, 2, 1}},
};
static const int CL_P2_WD[2][6] = {{1, 2, 2, 1, 2, 1}, {2, 1, 1, 2, 1, 2}};

typedef struct {
    short season;
    signed char phase;                  /* 1, or 2 for the second group stage (2002/03) */
    SeasonLeagueDay tue[6];             /* the Tuesday of each matchday's week */
} ClRealDays;

/* CL_REAL: the real matchdays of each season. */
#include "season_uefa_days_cl.inc"

int season_uefa_cl_group_days(int season, int phase, int set, SeasonLeagueDay out[6], int wd[6])
{
    for (int i = 0; i < 6; i++) {
        out[i] = phase == 2 ? CL_P2[set][i] : CL_P1[set][i];
        wd[i] = phase == 2 ? CL_P2_WD[set][i] : CL_P1_WD[set][i];
    }
    for (size_t r = 0; r < sizeof CL_REAL / sizeof CL_REAL[0]; r++)
        if (CL_REAL[r].season == season && CL_REAL[r].phase == phase) {
            for (int i = 0; i < 6; i++) {
                int y = season + CL_REAL[r].tue[i].year_off, m = CL_REAL[r].tue[i].month + 1,
                    d = CL_REAL[r].tue[i].day;
                if (wd[i] == 2) add_days(&y, &m, &d, 1);
                out[i].day = d, out[i].month = m - 1, out[i].year_off = y - season;
            }
            return 1;
        }
    return 0;
}

/* ---- The UEFA Cup, Europa League and Conference League group stages (2004/05 to 2023/24) ----
 *
 * Thursdays. 5 days: 21 October, 4 and 25 November, 2 and 16 December (UEFA Cup 2004/05). 6 days: 17 September,
 * 1 and 22 October, 5 November, 2 and 17 December (Europa League 2009/10, row uefa-69 of the rules table). In
 * 2007 the 5 days met the FIFA days of 17 October and 21 November (ctest season_uefa_days), so the UEFA Cup
 * 2007/08 takes its real days (the Thursday of each matchday): 25 October, 8 and 29 November, 6 and 20 December
 * (sources: see UC_2007). The 6 days of 2009/10 ended after the real first knockout draw from 2012/13 (the draw of
 * 2012/13 on 20 December, the last group day 20 December), and in 2022/23 ran into the World Cup: the Europa League
 * and Conference League groups of 2009/10 to 2023/24 take their real days (sources: see EL_REAL). */
static const SeasonLeagueDay GROUP_DATES_5[5] = {{21, 9, 0}, {4, 10, 0}, {25, 10, 0}, {2, 11, 0}, {16, 11, 0}};
static const SeasonLeagueDay GROUP_DATES_6[6] = {{17, 8, 0}, {1, 9, 0}, {22, 9, 0}, {5, 10, 0}, {2, 11, 0},
                                                 {17, 11, 0}};
#include "season_uefa_days_uc.inc"

const SeasonLeagueDay *season_uefa_group_days(int season, int days)
{
    if (days != 6) return season == 2007 ? UC_2007 : GROUP_DATES_5;
    for (size_t r = 0; r < sizeof EL_REAL / sizeof EL_REAL[0]; r++)
        if (EL_REAL[r].season == season) return EL_REAL[r].thu;
    return GROUP_DATES_6;
}
