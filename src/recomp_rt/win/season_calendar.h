/* The League of Ireland moves to calendar years in 2003 (gap row 2 of the 0.7.2 league sizes; research
 * run-logs ire2003-research.md, sources ire2003-sources.md, kept outside the repo; the rows wal-nir-irl-41, 41b, 42 and
 * 42b of docs/research/rules-0.6.0/wal-nir-irl.tsv).
 *
 * Real life: the 2001-02 season (August to April), the short 2002-03 season (5 July 2002 to 26 January 2003, play-off
 * to 8 February 2003), then calendar seasons from 2003 (10 April to 30 November 2003, play-off 3 to 13 December). The
 * original's Ireland (league table entry 20, key_nation_build_table 0x667150 at 0x66834b..0x668443, creator 0x834e60)
 * plays every season from August to April with its update day on 20 June.
 *
 * The entry has three states, from the start year, changing rules and the edition +0x40 of IrePrm (decision 9 of the
 * brief: no save field; the edition is in the save):
 *   SEASON_CAL_WINTER  the original: a game with changing rules off that starts in 2001 or 2002, and the 2001/02
 *                      edition of a game with changing rules on;
 *   SEASON_CAL_MOVE    edition 2002 with changing rules on: the short season, from the change of 20 June 2002 to the
 *                      change of 20 February 2003;
 *   SEASON_CAL_YEAR    edition 2003 on, and every edition of a game that starts in 2003 or later.
 * The calendar move needs the league sizes on (season_sizes_on): its templates come from the size engine.
 *
 * This file is pure: the rules and the dates. season_calendar_game.c applies them to the game. */
#ifndef RECOMP_SEASON_CALENDAR_H
#define RECOMP_SEASON_CALENDAR_H

#include <stdint.h>

enum { SEASON_CAL_WINTER, SEASON_CAL_MOVE, SEASON_CAL_YEAR };

#define SEASON_CAL_ENTRY 20            /* Ireland's league table entry (0xb64300) */
#define SEASON_CAL_FIRST 2003          /* the first calendar season */

int season_calendar_state(int start_year, int changing_rules, int edition);

/* The day of the year (0 = 1 January) of a date, month 1 to 12. */
int season_calendar_doy(int year, int month, int day);

/* The update day of the entry in the short and calendar seasons: 20 February (day 50). */
int season_calendar_update_doy(void);

/* The entry's dates of a state for the season of year y (month 1 to 12): the start and end the entry holds
 * (+0x2a, +0x39), and the parts it keeps for the step of the next change (+0x32..+0x37, +0x41..+0x46). */
typedef struct { int day, month, year; } SeasonCalDate;
void season_calendar_dates(int state, int y, SeasonCalDate *start, SeasonCalDate *end, SeasonCalDate *part_start,
                           SeasonCalDate *part_end);

/* The days to add to each date of the league template of a calendar season (days from 1 January of base, the
 * edition): whole weeks, so that the last date falls nearest 30 November of base. */
int season_calendar_league_shift(int base, const int *days, int n);

/* The short season plans its league rounds from the class's dates before this day (from 1 January of base): 1
 * February of base + 1, so the play-off (about 10 days) ends before the update day. */
int season_calendar_move_cut(int base);

/* The day (from 1 January of base) before which a cup of the entry ends: 14 days before the update day of base + 1. */
int season_calendar_cup_cut(int base, int state);

/* The day of a cup's real final that its last date moves to (the FAI Cup: 27 October 2002, 26 October 2003), or
 * INT_MIN for a cup that only moves as far as it must. */
int season_calendar_cup_target(const char *file, int state, int base);

/* The dates of a cup (n days from 1 January of the edition, in order) moved into the room of a season: every date by
 * the same whole weeks, the last nearest target (INT_MIN: the fewest weeks), the first on or after earliest and the
 * last before cut. Returns 1. When the cup is longer than the room, each date moves by its own whole weeks, in order,
 * from the earliest to the cut (2). 0 when even that fails (out then holds days). Weekdays stay. */
int season_calendar_cup_plan(const int *days, int n, int earliest, int cut, int target, int *out);

/* The day (1 to 31) and month (0 to 11) of the Irish awards: 1 May in winter (IrelandAwards::daily_update 0x63bf10
 * and OldIrelandAwards::daily_update 0x79f250 push 1 and 4 at 0x63bf27..0x63bf29 and 0x79f267..0x79f269), 16 February
 * in the short and calendar seasons (after the season and its play-off, before the update day). */
void season_calendar_award(int state, int *day, int *month0);

/* 1 when the label of a season year (the start year of the season; 0: the year of the game date) keeps two years
 * though the entry has moved: a calendar state and a year up to 2002 (decision 6). */
int season_calendar_two_years(int state, int year);

/* The history label of an edition of a league or cup that moves to calendar years (ire_prm, ire_first, ire_chal_cup,
 * ire_lge_cup) in the short or a calendar season: the edition, 2002 for the short season and 2003 for the first
 * calendar season. The history screens pass the label to key_nation_format_comp_season 0x6697c0 (0x499c5b..0x499c77),
 * which shows 2002 "2002/03" by season_calendar_two_years and 2003 "2003". 0 elsewhere: the game's own label (the
 * year the edition ends, so 2001/02 keeps 2002 as in the original). */
int season_calendar_label(const char *file, int state, int edition);

/* 1 for the files of the entry's leagues (ire_prm, ire_first) and of its cups with a date plan (season_calendar_game.c:
 * ire_chal_cup, ire_munster_cup, ire_pres_cup, ire_leinster_cup). */
int season_calendar_league(const char *file);
int season_calendar_cup(const char *file);

/* The game (season_calendar_game.c). season_sizes_schedule asks for the template of IrePrm and IreFirst: in a calendar
 * season the class's days (from 1 January of base) move by whole weeks, in the short season *cut gets the day before
 * which the plan takes them. Returns 1 when either applies (the plan must run even for the class's count). */
int season_calendar_template(uint32_t obj, const char *file, int base, int *days, int n, int *cut);
/* The dates of IreFirst's play-off (create_schedule of stage 0, the table in cpu.eax). */
void season_calendar_stage(uint32_t obj, const char *file, int stage, uint32_t count_at);
/* The history label of a competition object's edition (season_calendar_label), 0 for the game's own. */
int season_calendar_history_label(uint32_t obj);
/* RECOMP_CALENDAR_LOG=1: one line with the entry's fields (season.c calls it at a save and after a load). */
void season_calendar_log_entry(const char *when);
/* IrePrm's edition +0x40, 0 when Ireland is not selected. */
int season_calendar_edition(void);
/* RECOMP_CALENDAR_LOG=1: the label of a history row of an Irish league or cup (season_history.c). */
void season_calendar_log_history(uint32_t obj, int label);

#endif
