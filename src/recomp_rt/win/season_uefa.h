/* The formats of the UEFA club competitions by season (0.6.0 slice 22; spec
 * docs/specs/2026-10-16-seasons-0.6.0-spec.md, slice 22; research docs/research/2026-10-17-uefa-formats.md).
 *
 * The table (season_uefa_table.c, made by tools/seasons/make_uefa_table.py from
 * docs/research/rules-0.6.0/uefa.tsv) holds one row per season: the rounds of the Champions League, of the UEFA
 * Cup and of the Intertoto Cup. Slices 23 to 25 add their seasons as rows of the same table. The code reads a
 * format from the row. It does not test a year.
 *
 * The row of a season S for the game that runs (season_uefa_row): NULL for the base game (start year 2001,
 * changing rules off) and with RECOMP_SEASON_CODE=off, so the original runs as it is. With changing rules on,
 * the row of S. With changing rules off, the row of the start year (Q232). Before the first row the first row
 * applies, after the last row the last row (Q248). */
#ifndef RECOMP_SEASON_UEFA_H
#define RECOMP_SEASON_UEFA_H

#include <stdint.h>

#define SEASON_UEFA_ROUNDS 12

typedef struct {
    const char *name;               /* QR, PR, Q1, Q2, Q3, PO, R1 to R4, Groups, Groups 2, KO; a qualifying round
                                     * of two paths (2009/10 on) is two rounds: "Q3 CP" and "Q3 LP" (Champions
                                     * League: Champions Path, League Path), "Q2 CP" and "Q2 MP" (Europa League:
                                     * Champions Path, main path) */
    int clubs;
} SeasonUefaRound;

typedef struct {
    int season;                     /* the start year: 2003 for 2003/04 */
    int cl_clubs;                   /* Champions League clubs in all */
    int cl_round_count;
    SeasonUefaRound cl_rounds[SEASON_UEFA_ROUNDS];
    int uc_round_count;
    SeasonUefaRound uc_rounds[SEASON_UEFA_ROUNDS];
    int uc_groups, uc_group_clubs, uc_meetings;     /* UEFA Cup group stage, 0 when it has none */
    int uc_through;                 /* the clubs of a UEFA Cup group that go to the knockout (2 or 3) */
    int uic_clubs;                  /* Intertoto Cup clubs, -1 when not held */
    int uic_rounds, uic_winners;
    const char *uic_entry;          /* the UEFA Cup round of its winners: R1 or Q2 */
    const char *sc_stadium;         /* the Super Cup match: stadium and city (Windows-1252), day, month 0 to 11 */
    const char *sc_city;
    int sc_day, sc_month;
    const char *drops;              /* where the losers of Champions League qualifying rounds enter the Europa
                                     * League: "Q3>PO PO>G" (2009/10 on), "Q3-LP>G" a path; "" before 2009/10 */
    /* Slice 24 (2021/22 on). */
    int uc_ko_po;                   /* clubs of the Europa League knockout round play-offs (the group runners-up
                                     * and the Champions League thirds; the winners go to the round of 16), 0
                                     * when the groups send their clubs to the first knockout round */
    int away_goals;                 /* 1: away goals decide a level two-legged tie (to 2020/21); 0: no away goals
                                     * rule, extra time and penalties (U9, 2021/22 on), in all three cups */
    const char *suspended;          /* nations with no UEFA places, by name, separated by commas ("Russia"
                                     * from 2022/23, Q293); "" for none */
    int ecl_round_count;            /* the Conference League (U8, 2021/22 on), 0 when not held */
    SeasonUefaRound ecl_rounds[SEASON_UEFA_ROUNDS];
    int ecl_groups, ecl_group_clubs, ecl_ko_po;
    int sc_extra_time;              /* 1: a level Super Cup has extra time, then penalties; 0: penalties after 90
                                     * minutes (2023 on) */
    /* Slice 24, part b: the Conference League groups (meetings, places through a group) and where the losers of
     * the other two cups enter it: "c:PR>Q2 c:Q1>Q2 e:Q3>PO e:PO>G" (c: Champions League, e: Europa League). */
    int ecl_meetings, ecl_through;
    const char *ecl_drops;
    /* Slice 25 (2024/25 on): the matches of a club in the league phase of the Champions League, the Europa League
     * and the Conference League (8, 8 and 6), 0 for a group stage. A league phase is the "Groups" round of 36
     * (uc_groups 1, uc_group_clubs 36, uc_through 24): places 1 to 8 go to the round of 16 and 9 to 24 to the
     * knockout phase play-offs ("KO PO" 16, then "KO" 16). */
    int cl_league, uc_league, ecl_league;
} SeasonUefa;

/* The entry codes of the access list (club +0x1df) in a season with an access list (season_uefa_seeding.c).
 * They replace the original's codes (1 to 4 for the Champions League, build_entrants 0x584fa0; 1, 2, 3 and
 * 10 * b + 3 for the UEFA Cup, 0x902b90) only in those seasons, where the runtime builds the entrants of both
 * cups. The group stage is 1, as the original's, so the group pots (0x586f60: code, then reputation) put the
 * clubs that enter there first. */
enum {
    SEASON_UEFA_CL_G = 1,
    SEASON_UEFA_CL_PO_LP, SEASON_UEFA_CL_PO_CP, SEASON_UEFA_CL_Q3_LP, SEASON_UEFA_CL_Q3_CP,
    SEASON_UEFA_CL_Q2_LP, SEASON_UEFA_CL_Q2_CP, SEASON_UEFA_CL_Q1, SEASON_UEFA_CL_PR,
    SEASON_UEFA_CL_CODES                                        /* 10: codes 1 to 9 */
};
/* The Europa League codes are 11 to 16, apart from the Champions League's: a Champions League loser that goes
 * to the Europa League keeps its code, so the two are never counted as one. The group pots then put the
 * Champions League losers first (0x586f60 sorts by code). */
enum {
    SEASON_UEFA_EL_G = 11, SEASON_UEFA_EL_PO, SEASON_UEFA_EL_Q3, SEASON_UEFA_EL_Q2, SEASON_UEFA_EL_Q1, SEASON_UEFA_EL_PR,
    SEASON_UEFA_EL_CODES                                        /* 17: codes 11 to 16 */
};
/* Slice 24, part b: the Conference League codes are 21 to 25, apart from the other two cups' for the same reason. A
 * round has one path in the game, as the Europa League's: "Q2 CP" and "Q2 MP" are one round. */
enum {
    SEASON_UEFA_ECL_G = 21, SEASON_UEFA_ECL_PO, SEASON_UEFA_ECL_Q3, SEASON_UEFA_ECL_Q2, SEASON_UEFA_ECL_Q1,
    SEASON_UEFA_ECL_CODES                                       /* 26: codes 21 to 25 */
};
#define SEASON_UEFA_CODE_SLOTS 30                               /* arrays indexed by an entry code */

/* The code of an access list token of n characters ("Q3-CP") for comp 'c', 'e' or 'k', 0 when unknown. */
int season_uefa_code(char comp, const char *tok, int n);

/* The clubs of a round of the row by code: the Champions League round of a code ("Q3 CP", or "Q3" when it has
 * no paths), the Europa League round of all paths ("Q2 CP" + "Q2 MP"). 0 when the row has no such round. */
int season_uefa_code_clubs(const SeasonUefa *r, char comp, int code);

/* The Champions League losers of the row that enter Europa League round code (the drops of the row), or (slice
 * 24, a code of 21 to 25) the Champions League and Europa League losers that enter that Conference League round
 * (ecl_drops). */
int season_uefa_dropped(const SeasonUefa *r, int el_code);

/* The new clubs of each entry code of comp ('c', 'e' or 'k') that the round sizes of row r give: the clubs of the
 * round, less the winners of the round below in its path, less (Europa League) the Champions League losers that
 * enter it. out has SEASON_UEFA_CODE_SLOTS entries. Out[code], 0 for codes the row does not play. */
void season_uefa_new_clubs(const SeasonUefa *r, char comp, int *out);

/* The access list by association rank (slice 23, Q369: from 2009/10; the original's brackets before). */
typedef struct {
    int season;
    char comp;                      /* c Champions League, e Europa League, h CL title holder, H EL title holder
                                     * (EL place), X EL title holder with a CL place, f fair play places, L a
                                     * nation whose last EL place is its league cup winner's (places: the name);
                                     * slice 24: k Conference League, K its title holder, Y the same with an EL
                                     * place, M as L for the Conference League */
    int rank_from, rank_to;         /* 1 is the top association; 0 for h, H, X, f, K and Y. Places "" (2021/22
                                     * on): the ranks of the bracket have no place in that competition */
    const char *places;             /* the entry round of each place in domestic order (CL: the champion first;
                                     * EL: the cup winner first): G, PO, PO-CP, PO-LP, Q3, Q3-CP, Q3-LP, Q2, Q2-CP,
                                     * Q2-LP, Q1, PR */
} SeasonUefaAccess;

extern const SeasonUefaAccess season_uefa_access[];
extern const int season_uefa_access_count;

/* The places of comp ('c' or 'e') for an association of rank (1 is the top) in season S, or NULL before the
 * first season of the access list. A rank between brackets (Liechtenstein's, which has no league) takes the
 * next bracket. A rank after the last takes the last. After the last season the last season applies (Q248). */
const char *season_uefa_access_places(int season, char comp, int rank);

/* The places of a rule row ('h', 'H', 'X', 'f') in season S, or NULL. */
const char *season_uefa_access_rule(int season, char comp);

/* 1 when the last Europa League place of the nation named name ("England") goes to its league cup winner in S. */
int season_uefa_access_league_cup(int season, const char *name);

/* Slice 24: 1 when the last Conference League place of that nation goes to its league cup winner in S (rows M). */
int season_uefa_access_ecl_league_cup(int season, const char *name);

extern const SeasonUefa season_uefa_rows[];
extern const int season_uefa_row_count;

/* The row of season S in the table alone, with the first and last row for a season outside it. */
const SeasonUefa *season_uefa_table_row(int season);

/* The row of season S for the game that runs, or NULL: the original's formats (see above). */
const SeasonUefa *season_uefa_row(int season);

/* The clubs of a named round of a row's Champions League or UEFA Cup, 0 when the row has no such round. */
int season_uefa_cl_round(const SeasonUefa *r, const char *name);
int season_uefa_uc_round(const SeasonUefa *r, const char *name);

/* The clubs of a round of all paths: "Q3" gives "Q3" or "Q3 CP" + "Q3 LP" (+ "Q3 MP"). */
int season_uefa_cl_level(const SeasonUefa *r, const char *name);
int season_uefa_uc_level(const SeasonUefa *r, const char *name);

/* The clubs of a named round of a row's Conference League, and of a round of all its paths. */
int season_uefa_ecl_round(const SeasonUefa *r, const char *name);
int season_uefa_ecl_level(const SeasonUefa *r, const char *name);

/* 1 when the clubs of the nation named name have no UEFA places in row r (a suspension). */
int season_uefa_suspended(const SeasonUefa *r, const char *name);

/* 1 when the Champions League of row r has one group stage and a knockout round of 16 (2003/04 on). */
int season_uefa_cl_round_of_16(const SeasonUefa *r);

/* The rules report (season_report.c): writes the uefa lines of one season and phase. */
void season_uefa_report(void *file, int season, const char *phase);

/* Slice 23, season_uefa_qual.c: the game only. */
uint32_t season_uefa_cl_qual_table(uint32_t cl, int *rounds, uint32_t *name, uint32_t *league);
int season_uefa_uc_entrants(uint32_t uc);
uint32_t season_uefa_uc_qual_table(uint32_t uc, uint32_t rec, int *rounds);
int season_uefa_uc_direct_first(uint32_t uc);
void season_uefa_uc_fill_drops(uint32_t uc);
int season_uefa_report_clubs(uint32_t obj);

/* Slice 24, part b: the kind of a Uefa_Cup object (vtable 0x970790): 'e' the Europa League (its record id is
 * the key id [0x9cf6f0]), 'k' the Conference League (the record of the def new_conference_league of
 * season_comps.c), 0 for any other object. The record pointer +4 is set at 0x90231d, before the first call of
 * the constructor, so the kind holds inside it too. season_uefa_ecl_id gives the record id of the Conference
 * League, -1 when the game has none. */
#define SEASON_UEFA_ECL_RECORD "UEFA Europa Conference League"     /* its record's long name in the data */
#define SEASON_UEFA_ECL_FILE "new_conference_league"
int season_uefa_cup_kind(uint32_t obj);
int32_t season_uefa_ecl_id(void);
uint32_t season_uefa_ecl_object(void);

/* Slice 24, part b, season_uefa_qual.c: the Conference League's entry table and list, its round table of stage
 * -1, the first slot after its rounds and the losers of the other two cups into its slots. */
int season_uefa_ecl_entrants(uint32_t ecl);
uint32_t season_uefa_ecl_qual_table(uint32_t ecl, uint32_t rec, int *rounds);
int season_uefa_ecl_direct_first(uint32_t ecl);
void season_uefa_ecl_fill_drops(uint32_t ecl);

/* Slice 25, season_uefa.c: "League Phase" for the stage name id 0x41f of competition object obj when row
 * season gives it a league phase (regulations 2024/25, Art. 13.02: "league phase"), else NULL. */
const char *season_uefa_stage_name(uint32_t obj, int season, int id);

/* Slice 24, season_uefa.c: flag 0x4 (no away goals rule) in +0x0f of each two-legged round of a round table of
 * a season whose row has no away goals rule. The game only. */
void season_uefa_no_away_goals(uint32_t table, int rounds, int season);

/* O259: the real dates of the UEFA knockout rounds, one row per competition ('c' Champions League, 'u' UEFA Cup and
 * Europa League, 'k' Conference League), season and round kind (SEASON_UEFA_KO_*), from
 * docs/research/rules-0.6.0/uefa-ko-dates.tsv (season_uefa_dates_table.c, made by tools/seasons/make_uefa_dates.py).
 * Months 0 to 11, year offsets from the season's first year, gap the days from the first leg to the second (0 for
 * the final). */
enum { SEASON_UEFA_KO_PO, SEASON_UEFA_KO_R16, SEASON_UEFA_KO_QF, SEASON_UEFA_KO_SF, SEASON_UEFA_KO_FINAL };
typedef struct {
    char comp;
    int season, round, draw_day, draw_month, draw_year, day, month, year_off, gap;
} SeasonUefaKoDate;
extern const SeasonUefaKoDate season_uefa_ko_dates_table[];
extern const int season_uefa_ko_dates_count;

/* The row of comp, season and round kind, or NULL. */
const SeasonUefaKoDate *season_uefa_ko_date(char comp, int season, int round);

/* season_uefa_dates.c, the game only: the draw date, the first leg and the days to the second leg of each round r
 * of a knockout round table from the rows of comp and season (kinds[r] its round kind). A round with no row keeps
 * its dates. Returns the count of rounds it dated. */
int season_uefa_ko_dates(uint32_t table, char comp, int season, const int *kinds, int rounds);

#endif
