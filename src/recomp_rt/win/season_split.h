/* The split engine (0.7.2 league sizes, slice 7; spec docs/specs/2026-11-13-league-sizes-0.7.2-spec.md, D7).
 *
 * A league of the size engine (season_sizes.h, GROWN) whose rules row has a split ("after N: A+B[+C], points P",
 * season_league.h split_after, split_size, split_meetings, split_points) plays its table to N matches a club, then
 * splits into sections of the row's sizes by the positions after match N. Each section plays the row's meetings of
 * its own clubs. The clubs stay in the one table, so the points carry over (halved where the row says so), and the
 * table keeps each section above the next. This is the model of the Scottish split of the original
 * (ScoPrm::update_for_date 0x7f2d20, ScoPrm::create_split_stages 0x7f2d90, comparator 0x7f3220), for any count and
 * any sections. The Scottish Premier keeps its own code (D7). Only a season game gets any of it (rule Q152). */
#ifndef RECOMP_SEASON_SPLIT_H
#define RECOMP_SEASON_SPLIT_H

#include <stdint.h>
#include "season_league.h"

typedef struct {
    int meetings;                           /* the table's meetings before the split (+0x3c) */
    int after;                              /* matches of each club before the split: meetings x (n - 1) */
    int parts;                              /* sections */
    int size[SEASON_LEAGUE_PARTS];          /* the clubs of each section, top first */
    int meet[SEASON_LEAGUE_PARTS];          /* the meetings in each section (gap 34: 0, a section that plays no more) */
    int rounds[SEASON_LEAGUE_PARTS];        /* the rounds of each section: meet x (size' - 1) */
    int matches[SEASON_LEAGUE_PARTS];       /* the matches of a club of each section: meet x (size - 1) */
    int rounds_max;                         /* the dates after the split */
    int matches_max;                        /* the most matches of a club after the split */
    int marker;                             /* +0x3c once the split is made: meetings + the most section meetings */
    int halved;                             /* 1: some section halves its points at the split */
    signed char section_of_place[64];       /* the section of each place after match N (0-based), -1 past n */
    char points[SEASON_LEAGUE_PARTS];       /* gap 34: each section's points at the split: 'h' halved, 'k' kept, 'z'
                                             * zero, 'b' zero and 3 for its top club; 0 for all as +halved */
    int halve[SEASON_LEAGUE_PARTS];         /* 1: the section halves its points (rounded up) */
} SeasonSplit;

/* 1 when a league object of a grown file with n clubs and the row's meetings plays a split this edition (the row
 * of season_sizes_row_season), with its sections in *sp. 0 outside a season game, for the Scottish Premier, for a
 * row with no split, with unknown section meetings, with sections that do not add up to n, or with a split that
 * does not come after the whole table (N = meetings x (n - 1)). */
int season_split_of(uint32_t obj, const char *file, int n, int meetings, SeasonSplit *sp);

/* The matches after the split of the largest section (0 for none): season_sizes.c adds them to +0xa7. */
int season_split_matches(uint32_t obj, const char *file, int n, int meetings);

/* After League::update_for_date 0x6847c0 of any league: makes the split on the first day that every club has
 * played N matches. */
void season_split_update(uint32_t obj);

/* 1 while the split deletes the section stage obj (made at the split; its fixtures stay with the parent). */
int season_split_deleting(uint32_t obj);

/* After League::load_from_temp_file 0x687b10 (season_load.c): a league loaded after its split takes the table
 * order of its sections again (+0xa3 is not in the save, as ScoPrm's load sets it at 0x7f0bf1..0x7f0c08). */
void season_split_loaded(uint32_t obj);

/* For the test log (RECOMP_SIZE_LOG) when a table ends (League::free_table, season_sizes.c): after a split, each row
 * in table order as its place after match N and its matches played. */
void season_split_log_end(uint32_t obj);

/* The meetings of a league's table for the rules report (season_report.c): +0x3c, or after a split the meetings
 * before it (+0x3c then holds the marker of the split). */
int season_split_meetings(uint32_t obj);

/* At each create_schedule of a grown league for stage -1 (a new edition): the table order of the original again. */
void season_split_new_edition(uint32_t obj);

#endif
