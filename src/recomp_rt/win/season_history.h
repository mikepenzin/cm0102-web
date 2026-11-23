/* Competition history between 2001 and the start season (0.6.0 slice 11c, Q347).
 *
 * The season databases before 2010/11 hold no competition history between 2001 and the season before the
 * start. (Four databases of 2010/11 and 2011/12 hold no US Open Cup row at all, though Con_Champ::pick_clubs
 * 0x4c11a0 reads its newest winner for CONCACAF place 1: plan.tsv gives them the researched US Open Cup rows only,
 * tools/seasons/history_fill.py, GAP_COMPETITIONS.) At the database load of a new game in the season of such a database, after the year shift of slice
 * 11b, the game adds the researched rows (docs/research/history-0.6.0/, the research of each fact with its
 * sources) to the history table in memory. The database files stay unchanged. A loaded save needs nothing: it
 * holds the rows.
 *
 * For each competition that holds history rows in the database and each label year from 2002 to the start
 * year that the research covers, the rows of the database for that year are cleared (their competition becomes
 * 0, the free row of the original, 0x4afce0), and the researched rows are written. The research wins over the
 * rows of the database: most of those rows hold the result of another season (slice doc
 * docs/research/2026-10-17-competition-history-fill.md).
 *
 * It runs from season_dates.c, right after the shift, and it is undone with the shift when the shift is made
 * again for another year. The researched rows come for a start year that is the season of the database, and for
 * the official data sets (3.9.60, 3.9.65, 3.9.68) in every start year from 2002 (0.6.2 open item 149): their
 * history is that of 2001 in every start year (the shift leaves it), so a game from 2004 shows Euro 2004 and the
 * OFC Nations Cups of 2002 and 2004. Each such row holds from the first start year after its edition ended
 * (plan-official.tsv). An Update Team database in another season moves its whole history (season_dates.c), so it
 * gets no rows there; nor does a Custom database. Nothing runs with RECOMP_SEASON_CODE=off.
 *
 * The records that the season code adds (open item 74, 0.6.2). No database holds the history of a competition
 * whose record the season code adds (season_comps.h, `like`), such as the Finalissima. In every season game,
 * with a shift plan or without one (a 2001 game with changing rules on), the fill writes the past editions of
 * such a record whose label year is not after the start year (season_history.c, new_comp_rows). The base game
 * adds no record, so nothing changes there.
 *
 * Duplicate rows. Many season databases hold the same history row twice: the same competition, label year and
 * four places (September 2010 ODB: 175 rows, among them the English Premier Division 2001 Chelsea / Manchester
 * United, which the shift of slice 11b makes 2010; tests/seasons/history_duplicates_check.py lists every
 * database). The winners tab 0x4991f0 then shows the year twice. Before the fill, every season database load
 * with a shift (season_dates.c) clears the second and later copy of each such row in the same way, and the undo
 * gives them back. So does a load of the 3.9.60 or the 3.9.65 data, which has no shift plan in a 2001 start (open
 * item 227: 3.9.60 holds the African Super Cup 2000 Hearts of Oak / Zamalek and the Merconorte Cup 2000 Atletico
 * Nacional / Millonarios twice, 3.9.65 the same two). The base game and its 3.9.68 data (2 such rows) are not
 * touched, nor is a Custom database without a plan.
 *
 * Rows after the start (0.6.2 slice 7, open items 3 and 17). The data holds results of editions that the game
 * then plays itself: the 3.9.68 data holds 128 club rows and 3 nation rows of the label year 2002 (the 2001/02
 * season, Arsenal) and the results of August to December 2001 (Charity Shield 2001 Liverpool / Manchester
 * United), in a game that starts in July 2001; the season databases hold the calendar-year leagues and the
 * super cups of their start year. The game then writes the same edition again, so the history shows the year
 * twice, and it reads the newest row (Con_Champ::pick_clubs 0x4c11a0 through 0x4afd80): its 2001 Charity Shield
 * is Liverpool against Arsenal, the 2002 champion. So at the creation of a new game every row with a label after
 * the start year S goes (season_history_drop_after_start, with the undo record; in the official data also the
 * label S of a split season, which that data labels by the year where it starts), and when the game writes a row
 * (0x4afce0), every other row of that competition with a later label goes, and in the calendar year S the row of
 * the data with the same label (not when the data holds two or more of them: the halves of a split season). A row
 * of a Brazilian region record that names a club of another state than the one the game plays on it (the data files
 * several state championships there, open item 176) is neither cleared nor counted in that test, unless every row of
 * that label is of another state: then the test takes them all (open item 189, season_history_start.c). A row
 * of the year S of a competition that ends before July (the English Premier Division 2001 in a 2001/02 game)
 * stays: the game writes no row of that label. A second edition of a competition in one calendar year (a final that
 * moves across the new year, as the Emperor's Cup) takes the next free label, unless the competition is a split
 * season or the row of that label is one the game wrote on the same day (the Inter-Toto Cup winners). */
#ifndef RECOMP_SEASON_HISTORY_H
#define RECOMP_SEASON_HISTORY_H

#include <stdint.h>

/* from, to: the start years for which a row of an official data set holds (plan-official.tsv; to 0xffff: every
 * later start year). to 0: the row holds for the start year of its entry (plan.tsv, new_comp_rows). */
typedef struct {
    const char *comp;               /* competition name (Latin-1, as the data writes it) */
    uint16_t label;                 /* label year whose rows the database loses */
    uint16_t from, to;
} SeasonHistClear;

typedef struct {
    const char *comp;
    uint16_t label;
    const char *place[4];           /* first, second, third, host: full club or national side names, "" for none */
    const char *nation[4];          /* the nation of each, to choose between clubs of the same name */
    uint16_t from, to;
} SeasonHistRow;

typedef struct {
    const char *db;                 /* the neutral name of the database (seasons_db.h, season_dates.h) */
    uint16_t start;                 /* the start year that the rows are for: the season of the database */
    uint16_t last;                  /* 0: start only; else the entry holds for start to last (an official data set,
                                     * open item 149), and each row for its own from and to */
    const SeasonHistClear *clears;
    uint32_t n_clears;
    const SeasonHistRow *rows;
    uint32_t n_rows;
} SeasonHistDb;

extern const SeasonHistDb season_history_dbs[];          /* season_history_table.c */
extern const int season_history_dbs_count;

/* The corrected rows of issues 145 and 165 (docs/research/history-rows/, season_history_rows_table.c). Every season
 * game takes them at the end of the fill; the base game takes none. A name may list the spellings of the databases,
 * joined by '|'. */
typedef struct {                    /* lists.tsv: the whole history of a competition */
    const char *comp;
    uint16_t label;
    uint16_t from;                  /* the first start year that holds the row: the game plays earlier editions */
    const char *place[4];           /* first, second, third, host */
} SeasonHistListRow;

typedef struct {                    /* corrections.tsv: one field of one edition */
    const char *comp;
    uint16_t label;
    uint8_t field;                  /* 0 first, 1 second, 2 third, 3 host */
    const char *guard;              /* the winner that the row must hold */
    const char *accept;             /* the field stays when it holds one of these names */
    const char *write;              /* "": the field becomes empty */
} SeasonHistRowFix;

typedef struct {                    /* clears.tsv: rows of editions that never took place */
    const char *comp;
    uint16_t from_label;
} SeasonHistClearFrom;

extern const SeasonHistListRow season_history_list_rows[];
extern const int season_history_list_rows_count;
extern const SeasonHistRowFix season_history_row_fixes[];
extern const int season_history_row_fixes_count;
extern const SeasonHistClearFrom season_history_clears_from[];
extern const int season_history_clears_from_count;

/* After the shift of a database load or a new shift (season_dates.c): the rows of database `name` for start
 * year N: first the duplicate rows go (above), then the researched rows and the rows of the added records.
 * name NULL (no shift plan): only the rows of the added records, and the duplicate rows go when duplicates is 1
 * (the 3.9.60 or the 3.9.65 data, open item 227). */
void season_history_fill(const char *name, int N, int duplicates);

/* At the creation of a new game, after the shift and the fill: clears every row with a label after S (above), and
 * with official (the 3.9.60, 3.9.65 or 3.9.68 data) the rows of S of a split season. */
void season_history_drop_after_start(int S, int official);

/* 1 when the rows after the start go (the head): the season code is on (and, with FIX_BASE_GAME 0, the game is
 * not the base game). */
int season_history_after_start_on(void);

/* Before a new shift (season_dates.c): gives back the rows and the row count of the last fill. */
void season_history_undo(void);

/* When the loaded data no longer comes from a database load (season_dates_forget): drops the undo record. */
void season_history_forget(void);

/* The number of rows that the last fill added and cleared (a test reads them). */
void season_history_counts(uint32_t *added, uint32_t *cleared, uint32_t *unmatched);

/* At the creation of a new game, after the drop (season_dates.c): the test log of RECOMP_HISTORY_LOG, before the
 * game keeps only the rows of the selected nations. */
void season_history_log_create(void);

/* At each human turn (testrun.c): the test log of RECOMP_HISTORY_LOG=<competition>;... (season_history.c). */
void season_history_log_turn(void);

#endif
