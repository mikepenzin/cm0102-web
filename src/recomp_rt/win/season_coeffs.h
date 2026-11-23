/* The real UEFA season coefficients of the season databases (0.6.2 open item 106).
 *
 * A nation record holds six season coefficients, doubles at +0xe0 to +0x108, oldest first. At a new game in start
 * year S the game expects the seasons (S-6)/(S-5) to (S-1)/S there: the first season ranks the nations by +0xe0 to
 * +0x100 (UefaSeeding::roll_coefficients 0x903b50 makes no shift in the first season, 0x903b6f), the European
 * Performance Spots of the first season read +0x108 (season_uefa_seeding.c), and each later season change shifts
 * the six down one place and writes the game's own points to +0x108 (0x903b50). So +0x108, the season before the
 * start, counts in the five-season totals of the next five seasons.
 *
 * Max, 3 and 4 November 2026: every season database and extra database takes the real values of the six seasons
 * (researched, kassiesa.net and UEFA). Some hold 0 at +0x108 (October 2022 to October 2026 and April 2024: the
 * season before the start is missing). Some hold all six one season old (Summer 2002, September 2005, September
 * 2008, September 2011 ODB, October 2019, November 2020 and the extra databases December 2008, March 2009, March 2012
 * ODB, March 2020, April 2021 and April 2023) or one season early (Summer 2006, October 2021 and the extra databases
 * June 2010 and April 2022). The others hold a few wrong values (1 to 13 nations each). Our own Summer 2002,
 * Summer 2004 and Summer 2006 hold the real values from 4 November 2026 (tools/seasons/own_db/coeffs.py). Their
 * rows fix a copy of an earlier zip.
 *
 * At the database load of a new game (season_dates.c, after the shift and the history fill), the game writes the
 * six values of each nation of the table, in every start year (also 2001, where no shift runs). A new shift for
 * another year undoes the write first, as it undoes the history fill. The database files stay unchanged. A loaded
 * save needs nothing: it holds the nation records, and a save of an older build keeps its old values.
 *
 * A row is written only when the record of its nation id holds that id and its six values are those of the known
 * database or already the real ones (a nation.dat that a player changed keeps its own values). Nothing runs with
 * RECOMP_SEASON_CODE=off or for a database the table does not name.
 *
 * The table (season_coeffs_table.c) is made by tools/seasons/make_coeff_table.py from
 * docs/research/2026-11-03-database-coefficients.tsv, which the tool measures from the databases and
 * docs/research/2026-11-03-uefa-association-coefficients.tsv. */
#ifndef RECOMP_SEASON_COEFFS_H
#define RECOMP_SEASON_COEFFS_H

#include <stdint.h>

typedef struct {
    uint16_t id;                    /* the nation id (the record index) in the database */
    const char *nation;             /* its name in the database, for the log */
    double found[6];                /* the database's six values at +0xe0..+0x108 */
    double real[6];                 /* the real values of the six seasons */
} SeasonCoeff;

typedef struct {
    const char *db;                 /* the neutral name of the database (seasons_db.h) */
    const SeasonCoeff *rows;
    int n_rows;
} SeasonCoeffDb;

extern const SeasonCoeffDb season_coeff_dbs[];          /* season_coeffs_table.c */
extern const int season_coeff_dbs_count;

/* After the shift of a database load (season_dates.c): the rows of database `name` (NULL: none). */
void season_coeffs_fill(const char *name);

/* Before a new shift (season_dates.c): gives back the values of the last fill. */
void season_coeffs_undo(void);

/* When the loaded data no longer comes from a database load (season_dates_forget): drops the undo record. */
void season_coeffs_forget(void);

#endif
