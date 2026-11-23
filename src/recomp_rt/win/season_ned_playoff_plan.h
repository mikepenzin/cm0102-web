/* The Dutch nacompetitie by season: the pure plan of the knockout (issue 150). season_ned_playoff.c plays it. */
#ifndef RECOMP_SEASON_NED_PLAYOFF_PLAN_H
#define RECOMP_SEASON_NED_PLAYOFF_PLAN_H

typedef enum {
    NED_PO_GROUPS,      /* to 2004/05: the class's two groups of four */
    NED_PO_NONE,        /* 2019/20: abandoned, no play-off */
    NED_PO_TEN,         /* 2005/06 to 2018/19: two brackets of five, each for one place */
    NED_PO_SEVEN,       /* 2020/21 to 2024/25 and from 2026/27: six clubs, then the Eredivisie 16th in the semi-final */
    NED_PO_LADDER       /* 2025/26: four rounds, the Eredivisie 16th in the final */
} NedPoForm;

#define NED_PO_MAX_LIST 7       /* the most clubs of a bracket */
#define NED_PO_MAX_ROUNDS 4
#define NED_PO_MAX_TIES 3
#define NED_PO_UPPER 100        /* entrant code: NED_PO_UPPER + k is the k-th play-off club of the Eredivisie table */

typedef struct {
    int id;                     /* the round id (+0x07 of a round record) */
    int clubs, ties;            /* the clubs and ties of the round */
    int add, first;             /* the new clubs of the round and the list index of the first of them */
} NedPoRound;

NedPoForm ned_po_form(int season);
/* The legs of each tie of the season: 1 in 2020/21, else 2. */
int ned_po_legs(int season);
int ned_po_brackets(NedPoForm form);
int ned_po_rounds(NedPoForm form);
/* The Eerste Divisie clubs of the play-off, and the Eredivisie clubs. */
int ned_po_lower_clubs(NedPoForm form);
int ned_po_upper_clubs(NedPoForm form);
/* The days from the draw of the first round to the last match. */
int ned_po_days(NedPoForm form, int legs);
/* 1 when the round exists; *out gets it. */
int ned_po_round(NedPoForm form, int round, NedPoRound *out);
/* The entrants of a bracket in list order: a seed from 0 (the best placed Eerste Divisie entrant) or NED_PO_UPPER + k.
 * Returns the count, 0 for no such bracket. */
int ned_po_entrants(NedPoForm form, int bracket, int out[NED_PO_MAX_LIST]);
/* The two sides of a tie: a list index from 0, or -(t + 1) for the winner of tie t of the round before. The draw puts
 * the sides in home order by their table places (season_ned_playoff_draw_round). Returns 1 when the tie exists. */
int ned_po_tie(NedPoForm form, int round, int tie, int *first, int *second);

#endif
