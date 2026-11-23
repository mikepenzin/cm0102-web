/* Competition names of each season (0.6.0 slice 15, Q226, Q238; spec docs/specs/2026-10-16-seasons-0.6.0-spec.md).
 *
 * The names table: Seasons/names.tsv of the download (tools/seasons/make_names.py makes it from the name column
 * of the rules table). One row per season, source file of a class and object number: the long name and the
 * short name that the game shows, and for a league the abbreviation (format 2, 0.6.1 item 2). The database files
 * stay unchanged: the names go into the competition records in memory (club_comp.dat and nation_comp.dat
 * records, 0x6b bytes: long name at +4, 52 bytes; short name at +56, 27 bytes; abbreviation at +0x53, 4 bytes,
 * which Select Team shows for the division of a club; docs/research/2026-10-13-database-inventory.md). A row
 * whose long name is the data's leaves the data's abbreviation. A load of a save of 0.6.0 (season names with
 * the data's abbreviations) fits the abbreviations of its records to their names.
 *
 * When the names go in:
 * - a new game: at the end of build_comp_object_table 0x8386f0 (called at 0x829640 in create_game 0x828110,
 *   after the competitions are made and before the run to the start date 0x5cc850 at 0x829fd9). The names
 *   are those of the start season;
 * - a loaded game: at the end of the same function at 0x82c5f6 in load_game_data 0x82a9c0. The names are those
 *   of the start season with changing rules off, else those of the season of the game date (a season starts
 *   on 1 July). Slice 16 gives the names of each later season through season_names_apply.
 * The original finds each competition by its long name at every load (match_club_comp_key_id 0x60efd0, called
 * from set_key_ids 0x601a40 at 0x52d8fa in load_database 0x528540). So every long name goes in through
 * season_change_rename (season_change.h, slice 16): it keeps the name in the data for that match and puts the
 * names in the season part of the save. The short name is not matched and goes in directly.
 *
 * Only a season game gets names: a game whose start year is not 2001 or whose changing rules are on. The base
 * game (2001/02 with changing rules off) and the test switch RECOMP_SEASON_CODE=off keep the database names. */
#ifndef RECOMP_SEASON_NAMES_H
#define RECOMP_SEASON_NAMES_H

#include <stddef.h>
#include <stdint.h>

/* Reads the names table once at the start: names.tsv in the folder of RECOMP_RULES_TABLE (test runs) or
 * Seasons/names.tsv of the game folder. Host memory only. Returns the row count, 0 when the file is missing
 * or damaged (the game then keeps the database names). */
int season_names_init(const char *game_dir);
int season_names_load(const char *path);          /* also for tests; a later call replaces the table */

/* The long name of the n-th competition object (from 0) of a file (eng_first) in a season (start year). NULL
 * when the name does not change: outside a season game, in the base season 2001, when the table holds no name,
 * or when the object has that name now. A record that returns to its database name gets it. The objects of a
 * file count as in season_names_apply (ger_regional: North, then South). */
const char *season_names_get_object(const char *comp_file, int season, int n);

/* season_names_get_object for object 0: the agreed call of slice 16. */
const char *season_names_get(const char *comp_file, int season);

/* The long and short name of the n-th object (from 0) of a file in a season, from the table only. A gap in the
 * numbers gives NULL; an object after the last row takes the name of the last row. NULL when the table holds
 * none. */
const char *season_names_get_nth(const char *comp_file, int season, int n, const char **short_name);

/* Gives every competition object of the game the names of that season. Returns the number of records that
 * changed. Does nothing (returns 0) outside a season game. */
int season_names_apply(int season);

/* The names kind of slice 16 (season_kind_names.c): gives the competition object obj the long and short name
 * of the season, as season_names_apply does for every object. old gets the long name before (room bytes).
 * Returns 1 when the name changed, 0 when it did not or outside a season game. */
int season_names_rename_object(uint32_t obj, int season, char *old, size_t room);

/* The competition objects of the game in the order of the rules report: the slots of g_comp_object_table
 * [0xadadfc], then the arrays of the 35 league table entries at 0xb63d60, each object once. Returns the count
 * (at most max). */
int season_comp_objects(uint32_t *out, int max);

/* The path of a file of the season data: next to RECOMP_RULES_TABLE in test runs, else Seasons/<name> of the
 * game folder. */
void season_data_path(const char *game_dir, const char *name, char *out, size_t size);

/* 1 in a season game (see the top). */
int season_game_changes(void);

#endif
