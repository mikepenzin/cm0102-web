/* The match days of the UEFA group stages and league phases, and the real FIFA match days (row A18). Data and pure
 * functions only, so ctest links them (season_uefa_days.c, season_fifa_days_table.c). */
#ifndef RECOMP_SEASON_UEFA_DAYS_H
#define RECOMP_SEASON_UEFA_DAYS_H

#include "season_uefa_league.h"

typedef struct {
    short year;
    signed char month, day;             /* month 1 to 12 */
} FifaDay;

extern const FifaDay season_fifa_days[];
extern const int season_fifa_day_count;

/* Day of the year (0 is 1 January) of day d of month m (1 to 12), and back. */
int season_day_of_year(int y, int m, int d);
void season_from_day_of_year(int y, int doy, int *m, int *d);

/* The real FIFA day of month m (1 to 12) in year y nearest to day `want`, or 0 when the month has none. After the
 * last year of the list: that year's days, 364 days a year later (7 more when the drift passes 3 days). */
int season_fifa_day(int y, int m, int want);
/* The FIFA days of year y as days of the year (0 is 1 January), in table order, at most max of them. A year has
 * fewer than SEASON_FIFA_YEAR_MAX (ctest season_fifa_days). */
#define SEASON_FIFA_YEAR_MAX 64
int season_fifa_year_days(int y, int doys[], int max);

/* The Club World Cup after the June FIFA window (open item 201). The first day (day of the year) of an edition that
 * starts in June, at least SEASON_CWC_AFTER_WINDOW days after the last June FIFA day of year y; first itself when it
 * is late enough or not in June. */
#define SEASON_CWC_AFTER_WINDOW 4
int season_cwc_after_window(int y, int first);

/* Days from 1 January 1970 (month 1 to 12), and back (H. Hinnant's civil calendar algorithms). */
long season_abs_day(int y, int m, int d);
void season_from_abs_day(long z, int *y, int *m, int *d);

/* 1 when day d of month m (1 to 12) of year y meets a FIFA day: within a day of one (more than two players on
 * duty within a day stop a club match, NationalTeams::can_schedule_club_fixture 0x776fc0), or between two FIFA
 * days at most 6 days apart (the players stay with their nation for the window). */
int season_fifa_meets(int y, int m, int d);

/* The date that day d of month m (0 to 11) of year y takes on weekday wd (0 Monday to 6 Sunday): the nearest such
 * day, at most 3 days either way, as Date::Date 0x549f70 moves it (0x54a2d0). */
void season_weekday_date(int y, int m, int d, int wd, int *oy, int *om, int *od);

/* The league phase match days of comp ('c' Champions League, 'e' Europa League, 'k' Conference League) in the
 * season that starts in year season, and their weekday (league_round_set_date 0x68a160: 2 Wednesday, 3
 * Thursday). */
const SeasonLeagueDay *season_uefa_league_days(char comp, int season, int *weekday);

/* The Champions League group days (6) of phase 1 or 2 (the second group stage, to 2002/03) for the groups of
 * set 0 (A to D, stages 0 to 3; phase 2: stages 8 and 9) or set 1, and their weekdays (1 Tuesday, 2 Wednesday).
 * Returns 1 with the real days of the season, 0 with the original's (EuropeanCup::create_round_table 0x583c50,
 * 0x583d38..0x583e86) when the table has no row for it. */
int season_uefa_cl_group_days(int season, int phase, int set, SeasonLeagueDay out[6], int wd[6]);

/* The group days (Thursdays) of a UEFA Cup, Europa League or Conference League group stage of `days` match days
 * (5 or 6) in the season that starts in year season (season_uefa_cup.c, group_table). */
const SeasonLeagueDay *season_uefa_group_days(int season, int days);

/* The second qualifying round (Q2) and the first round (R1) of the UEFA Cup with groups (season_uefa_cup.c
 * qual_table) in the season that starts in year season (month m 1 to 12). Each first leg: the original's day on a
 * Thursday (Uefa_Cup::create_round_table: Q2 10 August, 0x902735..0x90273c; R1 14 September, 0x9027cd..0x9027de),
 * a week later while the first or the second leg (14 days on) meets a FIFA day, at most 4 weeks. The R1 draw: the
 * day after the Q2 second leg (the original's 25 August, 0x9027bf..0x9027c4, came before it). Item 125 (batch Q):
 * the original's Q2 second legs of 23 August 2007 and 21 August 2008 and R1 first legs of 13 September 2007 and
 * 11 September 2008 fell the day after a FIFA day. The real days (en.wikipedia.org, 2004-05 to 2008-09 UEFA Cup,
 * "Schedule"): Q2 12, 11, 10, 16 and 14 August, R1 drawn 27, 26, 25, 31 and 29 August, R1 16, 15, 14, 20 and
 * 18 September, of 2004 to 2008, each second leg 14 days later. */
void season_uefa_cup_q2_day(int season, int *y, int *m, int *d);
void season_uefa_cup_r1_day(int season, int *y, int *m, int *d);
void season_uefa_cup_r1_draw(int season, int *y, int *m, int *d);

#endif
