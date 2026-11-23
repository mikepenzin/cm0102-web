/* Real club records for missing top and second tier clubs (0.6.0 slice 15b, Q359, Q361, Q362; doc
 * docs/research/2026-10-19-club-records.md).
 *
 * At the data load of a season game (a start year other than 2001) with rows in Seasons/club_records.tsv for
 * its start year, the loader reads a merged copy of the season database: the files with the new clubs, their
 * stadiums and cities, and the players of their squads added after the rows of the files. The files on disk stay
 * unchanged. Slice 15's club moves (season_clubs.c) then put each new club in its division. */
#ifndef RECOMP_SEASON_RECORDS_H
#define RECOMP_SEASON_RECORDS_H

#include <stddef.h>
#include <stdint.h>

/* Reads Seasons/club_records.tsv of the game folder (main.c). Returns the row count, 0 for none. */
int season_records_init(const char *game_dir);
int season_records_load(const char *path);

/* Around the data load 0x524d50 (season_dates.c): before builds the merged files and gives them to the file layer,
 * after frees them. dir is the data set folder (win_data_set_dir, "" for the game's own Data). */
void season_records_before_load(const char *dir, int start_year);
void season_records_after_load(void);

/* The club id (its row before the club trim) of the new record that the club moves list of `season` names
 * `real` (the real column of Seasons/clubs.tsv), or -1. */
int season_records_club(int season, const char *real);

/* Test log of FIXTURE_MANAGER::add_fixture 0x5aa680 (season_kind_cup_breaks.c calls it). */
void season_records_fixture_log(uint32_t fixture, uint32_t ret);
/* Test log of League::create_fixtures 0x6835c0 (season_uefa_league.c calls it). */
void season_records_league_log(uint32_t league, uint32_t ret);

/* The squad fill (Q509) at the start of 0x524d70, after slice 15's club moves (season_clubs.c). */
void season_records_fill(int season);

/* The squad of a club that joins a closed league at a season change: the same fill, from the players with no club of
 * the running game. Returns the players of the club. */
int season_records_fill_club(uint32_t club, int season);

/* The players of the last season_records_fill_club who have a contract row. */
int season_records_joined_contracts(void);

/* The staff reserve rows that the new clubs of this load need (staff_reserve.c): 16 for each new club, for the
 * coaches, scouts, physios and manager that 0x527240 makes. */
uint32_t season_records_reserve_extra(void);

/* The merge on its own, for the ctest case and the tools: builds the merged files of the data folder `dir` (host
 * path) for start year `season`, with the source databases found by `find_source` (neutral name to host folder,
 * 0 when absent). Returns the number of new clubs, -1 on a failure. season_records_merged gives each merged file. */
typedef int (*SeasonRecordsFind)(const char *neutral_name, char *folder, size_t room);
int season_records_merge(const char *dir, int season, SeasonRecordsFind find_source, char *log, size_t log_room);
int season_records_merged(int i, const char **name, const uint8_t **data, size_t *size);
void season_records_free(void);

#endif
