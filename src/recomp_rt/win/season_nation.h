/* The structured nation rules of the rules table (0.6.0 slice 17): the transfer windows, the foreign player
 * limits and the work permit rule of each nation and season. The table season_nation_rules is made by
 * tools/seasons/encode_rules.py (src/recomp_rt/win/season_nation_table.c: do not edit by hand); its docstring
 * gives the meaning of the values. This file is host code: the lookup uses no game memory, so a test can run it
 * (tests/seasons/test_season_nation.c). The kind that writes the game's memory is season_kind_nation.c. */
#ifndef RECOMP_SEASON_NATION_H
#define RECOMP_SEASON_NATION_H

#include <stdint.h>

#define SEASON_NATION_KEEP (-1)         /* no new information: keep what the game has */
#define SEASON_NATION_NONE 255          /* no limit (a limit byte of 0xff means no rule in the game) */
#define SEASON_NATION_WINDOWS_NONE 0    /* windows: no transfer windows, the club can sign at any time */
#define SEASON_NATION_MAX_WINDOWS 6

#define SEASON_NATION_PARTIAL_WINDOWS 1 /* flags: the game keeps window 2 of this nation (Italy) */

typedef struct {
    uint8_t sd, sm, ed, em;             /* start day and month, end day and month (months 1 to 12) */
} SeasonNationWindow;

typedef struct {
    const char *name;                   /* the nation of the rules table */
    uint32_t key_global;                /* g_key_nation_<name>: the game's global that holds the nation's record id */
    int flags;
} SeasonNation;

typedef struct {
    const char *nation;
    int season;                         /* the start year: 2001 for 2001/02 */
    int windows;                        /* SEASON_NATION_KEEP, SEASON_NATION_WINDOWS_NONE or the count in win */
    SeasonNationWindow win[SEASON_NATION_MAX_WINDOWS];
    int squad_non_eu, squad_foreign;    /* match squad of a first division fixture: KEEP, NONE or N */
    int sign_non_eu, sign_foreign;      /* club caps at a signing: KEEP, NONE or N */
    int brexit;                         /* 1: players of the EU and EEA need a permit as non-EU players */
} SeasonNationRule;

extern const SeasonNation season_nations[];
extern const int season_nation_count;
extern const SeasonNationRule season_nation_rules[];
extern const int season_nation_rule_count;

/* The rules of a nation in a season: every row of the nation up to that season, in order, each value that is
 * not KEEP replacing the one before (so a row of "unknown" keeps the earlier rule, and the rules stay after the
 * last row, Q248). `windows` is KEEP when no row up to the season gives any. */
typedef struct {
    int windows;
    SeasonNationWindow win[SEASON_NATION_MAX_WINDOWS];
    int squad_non_eu, squad_foreign, sign_non_eu, sign_foreign;
    int brexit;
} SeasonNationState;

/* Fills *out. Returns 1 when the nation has a row up to the season, 0 when it has none (*out holds only KEEP). */
int season_nation_state(const char *nation, int season, SeasonNationState *out);

/* The season of the rows that a season of the game reads for a nation. The Republic of Ireland plays calendar seasons
 * from 2003 when the calendar move is on (moved: season_rules_moved, gap row 2 of 0.7.2), and its rows from 2003/04
 * hold the real season of the next year ("2005" in row 2004/05), so season S from 2003 reads row S - 1 (open item 258).
 * Any other nation, and Ireland with the move off, reads row S. */
int season_nation_row_season(const char *nation, int season, int moved);

/* The match squad record of a fixture of a nation under the state: rec holds bytes +0..+2 (foreign) and +3..+5
 * (non-EU) of the 42-byte record that the lookup 0x8cf6b0 fills, 0xff for no rule; top is 1 for a fixture of the
 * nation's first division. A number acts in the first division only, "no limit" in every competition. A state
 * with a non-EU number and no foreign limit changes the kind of the game's limit in every competition of the
 * nation: when the record has no non-EU slot set, each set foreign slot moves to its non-EU slot (+0 to +3, +2 to
 * +5) before the numbers are given. Croatia (issue 135): the filler 0x518620 writes 4 foreign players at +0, and
 * from 2013/14 the HNS counts only players not of the EU or EEA. The kind adds no limit the game does not have.
 * Returns 1 when the record changed. */
int season_nation_squad_record(uint8_t rec[6], const SeasonNationState *st, int top);

/* The nation of the table with that name, or NULL. */
const SeasonNation *season_nation_find(const char *name);

/* The Brexit rule for one signing (Q249, R10): 1 when a player of the EU needs the permit decision. It holds when
 * the club's nation is one of England, Scotland, Wales and Northern Ireland and the table has the rule for it in
 * the season, the date has reached the year of the rule, the player is of the EU, and neither of his nationalities
 * (NULL for none) is a UK nation or the Republic of Ireland (the Common Travel Area). A club in the Republic of
 * Ireland is never touched, and an Irish player at a UK club goes through. */
int season_brexit_blocks(const char *club_nation, int season, int year, int rule_year, int eu, const char *nat1,
                         const char *nat2);

#endif
