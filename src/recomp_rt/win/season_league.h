/* The structured league rules of the rules table: play-off places, splits and calendars (0.6.0 slice 19), and
 * the structure of each league row: meetings, halves, zones, periods, split sections (0.7.2 league sizes, slice 1).
 *
 * tools/seasons/encode_leagues.py reads the playoffs, split and real_season texts of the league rows of the
 * rules table and writes season_league_table.c. Its docstring gives the words; the research of the original's
 * play-off and split code is in docs/research/2026-10-18-playoffs-splits-calendar.md. */
#ifndef RECOMP_SEASON_LEAGUE_H
#define RECOMP_SEASON_LEAGUE_H

#include "season_rules.h"

#define SEASON_LEAGUE_PARTS 4   /* zones of a division, sections of a split */

typedef struct {
    const char *file;       /* comp_file of the row: eng_first */
    int season;             /* start year of the row: 2001 for 2001/02 */
    int po_up;              /* promotion play-off places of the division (0 for none) */
    int po_up_first;        /* the first of them (3 for 3rd to 6th), 0 for none */
    int po_down;            /* relegation play-off places */
    int exact;              /* 1: the tool read the whole text; 0: only a part (the gaps file says why) */
    int split_after;        /* a split after this many matches, 0 for none */
    int split_top;          /* the clubs of the top group of the split */
    int calendar;           /* 0 a season over two years, 1 a calendar year, 2 a transition row */
    int split_points;       /* 0 points kept, 1 halved, 2 unknown, 3 by section (split_halved) */
    /* The structure of the row (0.7.2 league sizes, slice 1). 0 for a row of a file that is not a league. */
    int meetings;           /* times each pair meets in the table (in each half, zone, group, before a split);
                             * 0: the class's own meetings stay */
    int halves;             /* 1: two halves (Apertura and Clausura, two stages), each a table of the meetings;
                             * -1 (with zones, extra_rounds and periods -1): a future row with unknown clubs, whose
                             * structure is the last known row's (season_league_row) */
    int zones;              /* zones of one division, 0 for none */
    int zone_size[SEASON_LEAGUE_PARTS]; /* the clubs of each zone */
    int extra_rounds;       /* rounds beyond meetings x (n' - 1) (derby, inter-zone rounds); -1 unknown */
    int periods;            /* period titles of the season; 0 none, -1 unknown */
    int split_size[SEASON_LEAGUE_PARTS];     /* the clubs of each section of the split (split_size[0] = split_top) */
    int split_meetings[SEASON_LEAGUE_PARTS]; /* the meetings in each section; 0 unknown */
    int rounds;             /* rounds of the table (of a half, of a zone): meetings x (n' - 1) + extra_rounds, n'
                             * the count (the largest zone) rounded up to even; 0 unknown */
    int split_halved[SEASON_LEAGUE_PARTS];   /* 1: the section halves its points at the split (rounded up) */
} SeasonLeagueRule;

extern const SeasonLeagueRule season_league_rules[];
extern const int season_league_rule_count;

/* The encoded row of a file and season, or NULL (season_playoffs.c). */
const SeasonLeagueRule *season_league_rule(const char *file, int season);

/* The row whose structure a league plays in a season: the row of that season, or by the after-table rule the last
 * row before it (Q248); a future row with unknown clubs (halves -1) gives the last row before it with known
 * clubs. NULL when the file has no row. */
const SeasonLeagueRule *season_league_row(const char *file, int season);

/* The meetings of a league in a season: the row of that season, or by the after-table rule the last row before
 * it (Q248). A row with unknown clubs (a future season with no announcement) takes the meetings of the last row
 * before it that has them. 0 when no row gives meetings: the class's own meetings stay. Reads only the table:
 * no game state. */
int season_league_meetings(const char *file, int season);

/* The rules table row (season_rules.h) of a league's edition (+0x40, the year that start_new_season sets):
 * - row Y when its real season begins with Y ("2008/09", "2026/27"; jap_j1 row 2026 "2026/27", so a league that
 *   changes from calendar years to two-year seasons plays its two-year season in the year of the change);
 * - else a row of season Y - 1 whose real season begins with Y: a calendar-year league holds the real season Y + 1 in
 *   row Y ("Calendar-year league: real season 2009" in row 2008 of nor_prm), so edition Y takes row Y - 1. A season
 *   may hold a second row (gap row 2 of 0.7.2: 2002/03 holds the short Irish 2002-03 season and, in a second row,
 *   the first calendar season 2003), so every row of season Y - 1 is looked at, in order;
 * - else the row of season Y, or by the after-table rule the last row before it (Q248).
 * NULL when the file has no row then. Reads only the tables. */
const SeasonRule *season_league_edition_row(const char *file, int year);

/* 1 when the real season of a rules table row begins with the year ("2011", "2010/11", "2002–03 (short season)"). */
int season_league_real_starts(const SeasonRule *r, int year);

/* The season of that row when its real season begins with the year, else the year (season_sizes_edition). */
int season_league_edition(const char *file, int year);

/* The encoded row of a rules table row r of the file: the k-th encoded row of r's season and file, r being the k-th
 * rules table row of them (the groups of a division, the two Irish rows of 2002/03). NULL when there is none. */
const SeasonLeagueRule *season_league_rule_of(const char *file, const SeasonRule *r);

/* The meetings of a league in the season of rules table row r: those of its own encoded row when it is a second row
 * of its season with known meetings, else season_league_meetings(file, r->season). */
int season_league_meetings_of(const char *file, const SeasonRule *r);

#endif
