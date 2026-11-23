/* Right ages and dates in every season (0.6.0 slice 11b, Q344).
 *
 * The Update Team databases expect the original start in 2001, so they store their dates early by some years.
 * When a new game starts in another season, the game adds the shift of each field to the years in memory.
 * The database files stay unchanged. A loaded save needs nothing: it holds the shifted data.
 *
 * The fields, their evidence, the measured shift of each database and the rules are in
 * docs/research/2026-10-17-season-date-shifts.md. The table is made by tools/seasons/make_dates_table.py from
 * docs/research/rules-0.6.0/databases-date-shifts*.tsv (tools/seasons/measure_date_shifts.py measures them).
 *
 * When it runs: right after the load of the database files (0x524d50, which calls the loader 0x528540 with the
 * file source), for the start year of that moment. The loader's own passes (the id pass 0x5315a0 and the staff
 * pass 0x52d20a with its 1998 compare at 0x52d2e1) work on the dates as the database stores them, as with a
 * 2001 start. Every change is kept in a journal. At the creation of the game (0x828110, from season.c) the
 * shift is made again when the start year is not the one of the load: the journal gives back the stored dates
 * first. The start screen keeps the data of a first start (0x81ce00 loads only when no nation is loaded), so a
 * second start in another season gets its shift there.
 *
 * It does nothing when the start year is 2001 (every database plays as it does today) and with the test
 * switch RECOMP_SEASON_CODE=off. */
#ifndef RECOMP_SEASON_DATES_H
#define RECOMP_SEASON_DATES_H

#include <stdint.h>

/* The shifts of one known database, in years, for a game in the database's own season. */
typedef struct {
    const char *name;               /* the neutral name of seasons_known_dbs (seasons_db.h) */
    int8_t birth;                   /* staff date of birth, +0x10 */
    int8_t joined;                  /* staff date joined, +0x3e */
    int8_t contract;                /* staff contract end +0x46, national job dates +0x29 and +0x31 (per row) */
    int8_t officials;               /* officials date of birth, +0x0c */
    int8_t staff_history;           /* staff history year, +0x08, except the kept careers */
    int8_t comp_history;            /* club and nation competition history year, +0x08, every row */
    int8_t last_rows;               /* with comp_history 0: the shift of the 2001 rows of last_ids */
    int8_t nation_history;          /* the nation competition rows (0.6.2 slice 7): their own shift */
    uint16_t last_count;
    const uint16_t *last_ids;       /* competition ids (club comps, then nation comps) */
    uint32_t kept_rows;             /* the staff rows that kept_hex covers, 0 for none */
    const char *kept_hex;           /* bit i (byte i / 8, bit i % 8): the career of staff row i keeps its years */
    uint16_t nation_count;
    const int16_t *nation_comps;    /* pairs (competition id, shift): nation competitions with a shift of their own */
} SeasonDateShifts;

extern const SeasonDateShifts season_date_shifts[];     /* season_dates_table.c */
extern const int season_date_shifts_count;

/* Open item 233: competition history rows of a database whose year stays wrong after the shift of their database
 * (the data holds them one or more years off against the other rows: Norwegian Premier Division 1937 to 2007 one
 * year late in every database from 2010/11 on, the Irish league seasons before 2003 one year early). Each entry is
 * one row of the file: it gets delta years on top of its shift. docs/research/history-years/wrong-rows.tsv gives
 * each row with its right year and the reference (the 3.9.68 row or the research source); the table is made by
 * tools/seasons/make_history_years_table.py, the rows found by tools/seasons/history_years_check.py. */
typedef struct {
    uint8_t nation;                 /* 0: club_comp_history.dat, 1: nation_comp_history.dat */
    int8_t delta;                   /* the years to add on top of the shift; 0: the row matches no real edition and
                                     * stands on a year that a corrected row takes, so it becomes a free row */
    uint16_t year;                  /* the year of the row in the file (+0x08): the row must still hold it */
    uint16_t comp;                  /* the competition id of the row (+0x04): the row must still hold it */
    uint32_t row;                   /* the row in its file */
} SeasonYearFix;

typedef struct {
    const char *name;               /* the neutral name of seasons_known_dbs */
    const SeasonYearFix *fixes;     /* by nation, then row */
    uint32_t count;
} SeasonYearFixes;

extern const SeasonYearFixes season_year_fixes[];       /* season_history_years_table.c */
extern const int season_year_fixes_count;

/* The shifts that a load of the database folder (host path, "" for the game's own Data) gets in start year N, in
 * years: a date of the files plus the shift is the real date (slice 15b writes its new rows by them). A contract end
 * y gets contract_real when y >= D and y >= 2010, else contract_old. The career of staff row i keeps its years
 * (shift kept) when bit i of row->kept_hex is set. 0 when nothing moves (all shifts 0). */
typedef struct {
    int D, N, birth, joined, careers, kept, contract_old, contract_real;
    const SeasonDateShifts *row;
} SeasonShiftPlan;
int season_dates_plan(const char *folder, int N, SeasonShiftPlan *out);
/* The shift of a contract end in year y by that rule (the dates of player_setup.cfg take it too, player_setup.c). */
int season_dates_contract_shift(const SeasonShiftPlan *p, int y);
int season_dates_career_kept(const SeasonDateShifts *s, uint32_t row);

/* The row of a known database by its neutral name, or NULL. */
const SeasonDateShifts *season_dates_for(const char *name);

/* At the creation of a new game (season.c, create_game 0x828110), after the start year is set: shifts the
 * loaded data for that year when the load used another year. */
void season_dates_at_create(void);

/* At the end of a game and at the load of a saved game (season.c): the loaded data is no longer the data of
 * a database load, so the journal is dropped. */
void season_dates_forget(void);

/* The check of a Custom database folder (host path) against the game's own data, for Setup Game: 1 when its
 * dates could be checked (shift in *shift). 0, and Setup Game shows "Ages could not be checked", when:
 * - fewer than 2,000 people (first name, second name, day of birth) match the game's own data;
 * - fewer than 80 % of the matches agree on one shift;
 * - the game's own Data is not the 3.9.68 data (its signature);
 * - a file cannot be read (index.dat, staff.dat, first_names.dat, second_names.dat of either folder), or the
 *   staff table does not fit its file.
 * Cached per folder (path, size and time of staff.dat). */
int season_dates_custom_check(const char *folder, int *shift);

/* At each human turn (testrun.c): the test log of RECOMP_DATES_LOG=<name>;<name>;... (see season_dates.c). */
void season_dates_log_turn(void);

#endif
