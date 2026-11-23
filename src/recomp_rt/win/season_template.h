/* The date template of a league of any size (0.7.2 league sizes, slice 2; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D4 and D5).
 *
 * A league class makes its date template in create_schedule (thiscall: stage, &count, &code, &flag; ret 0x10):
 * one record of 0x41 bytes for each round, written by league_round_set_date 0x68a160 (+0 s16 day of the year from
 * 0, +2 s16 year offset from the year argument, +4 the time byte, +0x3d a dword) and league_round_set_match_slot
 * 0x68a1c0 (the 8 slots of 7 bytes at +5). league_next_schedule_round 0x68a030 takes the rounds from it: in order
 * (flag 0x40, or fewer than 2 meetings, or fewer than 8 clubs), shuffled modulo the count, or mirrored (flag 0x80:
 * the second half plays from the middle of the template). So a table of n clubs that meet m times needs exactly
 * R = m x (n' - 1) dates, n' the count rounded up to even.
 *
 * season_template_plan is pure: it takes the days of the class's template and gives the R days of the new one, each
 * with the class record it copies. The runtime (season_sizes.c) builds the records. */
#ifndef RECOMP_SEASON_TEMPLATE_H
#define RECOMP_SEASON_TEMPLATE_H

#define SEASON_TEMPLATE_MAX 128        /* the most dates of a template (a league of 60 clubs that meet twice: 118) */
#define SEASON_TEMPLATE_EARLIER 4      /* at most this many earlier starts of 7 days (lead decision, 13 November 2026) */
#define SEASON_TEMPLATE_WEDNESDAY 2    /* weekday 0 Monday .. 6 Sunday (Date::get_weekday 0x54a3f0) */

/* SEASON_TEMPLATE_HALVES: a class of halves (ArgPrm), whose runtime makes its own templates (season_sizes_arg.h);
 * season_template_plan never takes it. */
enum { SEASON_TEMPLATE_SHUFFLED, SEASON_TEMPLATE_SEQUENTIAL, SEASON_TEMPLATE_MIRRORED, SEASON_TEMPLATE_HALVES };

typedef struct {
    int day;        /* days from 1 January of the base year (the year argument of 0x68a160) */
    int from;       /* the class record that the date copies (its slots, its time byte) */
    int added;      /* 1: a new date (a Wednesday in a gap, or an earlier start); 0: a date of the class */
} SeasonTemplateDate;

/* Weekday of a day of a year (day 0 = 1 January): 0 Monday .. 6 Sunday. */
int season_template_weekday(int year, int day);

/* Days of a year: 365 or 366. */
int season_template_year_days(int year);

/* The day from 1 January of base_year of a record (year offset, day of that year), and back. */
int season_template_day(int base_year, int year_offset, int day_of_year);
void season_template_split(int base_year, int day, int *year_offset, int *day_of_year);

/* The plan of a template of `rounds` dates from the class's `count` days (days from 1 January of
 * base_year, in any order: a class date that snaps to its weekday can come before the one ahead of it). Mode
 * SEASON_TEMPLATE_MIRRORED plans each half of the template on its own.
 * - More rounds (D4): the class's dates stay; each extra round goes on the Wednesday of a gap of 7 to 14 days that
 *   is not the largest gap of the template (the winter break; a template with no gap over 7 days has none), at most
 *   one in a gap, spread evenly over the gaps.
 *   When the gaps run out, the season starts earlier in steps of 7 days (at most SEASON_TEMPLATE_EARLIER, and not
 *   before 1 January of base_year). Mirrored: the same number of new dates in each half; the second half has no
 *   earlier start. The last date stays the class's last date.
 * - Fewer rounds (D5): the dates at index round(k (count - 1) / (rounds - 1)), so the first and the last stay.
 * Returns rounds, or 0 when no room is left (why then holds the reason). */
int season_template_plan(int base_year, const int *days, int count, int rounds, int mode, SeasonTemplateDate *out,
                      char *why, int why_size);

/* The same with at most earlier_max earlier starts in place of SEASON_TEMPLATE_EARLIER (a class whose real season
 * starts months before the class's first date: the Brazilian classes, season_sizes_bra_earlier). */
int season_template_plan_from(int base_year, const int *days, int count, int rounds, int mode, int earlier_max,
                              SeasonTemplateDate *out, char *why, int why_size);

#endif
