/* The rules of the Belgian play-offs by season (season_bel_plan.c). Pure: no game state. */
#ifndef RECOMP_SEASON_BEL_PLAN_H
#define RECOMP_SEASON_BEL_PLAN_H

#define SEASON_BEL_PO3_WINNER (-1)  /* season_bel_top_entrants: the winner of the relegation play-off */

typedef struct {
    int final_legs;     /* the Play-off II final: 2 legs or 1 match */
    int europa_legs;    /* the Europa League play-off: 2 legs or 1 match */
    int b_clubs;        /* the second-division clubs in Play-off II */
    int stages;         /* the stages of the top division after its table */
} SeasonBelForm;

/* 1 and the form for a season with the Play-off II final (row year: 2009 for 2009/10), else 0. */
int season_bel_form(int season, SeasonBelForm *f);

/* The place of the Play-off I club in the Europa League play-off. cup_winner_place: the cup winner's final place in
 * the top division, 0 when it has none in Play-off I. */
int season_bel_europa_place(int season, int cup_winner_place);

/* The Play-off II group (0 A, 1 B) of a place of the table (from 1), or -1. */
int season_bel_group_of_place(int place);

/* The Play-off II group of the second-division club of rank b (from 0), or -1. */
int season_bel_group_of_b(int b);

/* 1 when the 15th and 16th play the relegation play-off. */
int season_bel_relegation_playoff(int season);

/* The top-division clubs in the second division's final round: the count and their rows (from 0) of a table of n
 * clubs, or SEASON_BEL_PO3_WINNER. */
int season_bel_top_entrants(int season, int n, int row[2]);

/* 1 when top-division entrant k goes down. winner: the entrant that won the final round, -1 for a second-division
 * club. */
int season_bel_final_round_down(int entrants, int winner, int k);

/* Who goes up from the second division. */
#define SEASON_BEL_UP_BOTH 0        /* the champion and the winner of the final round */
#define SEASON_BEL_UP_CHAMPION 1    /* the champion alone: no final round */
#define SEASON_BEL_UP_FINAL 2       /* the winner of the final between the period winners alone */
int season_bel_second_up(int season);

#endif
