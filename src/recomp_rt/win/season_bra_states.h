/* Brazil's state championships by state (0.6.0 slice 20e, Q332; docs/research/2026-10-20-brazil-states.md). */
#ifndef RECOMP_SEASON_BRA_STATES_H
#define RECOMP_SEASON_BRA_STATES_H

#include <stdint.h>

/* A city or club name and its state code (season_bra_states_table.c, made by tools/seasons/make_bra_states.py). The
 * names are in code page 1252, as the game holds them. */
typedef struct {
    int club;                   /* 1: a club's long name, 0: a city name without its state code */
    const char *name;
    const char *state;          /* "CE" */
} SeasonBraName;

extern const SeasonBraName season_bra_names[];
extern const int season_bra_name_count;

/* The state of a club from its long name and its stadium's city name (Q600): the city's suffix "(XX)", else the
 * club's suffix "(XX)", else the city in the table, else the club in the table. Writes the two letters and a 0 to out
 * and returns 1, or returns 0 with no state. No game memory: the ctest calls it. */
int season_bra_state_from(const char *club, const char *city, char out[3]);

/* At a new game and a load (season_comps_reset, from season_change_reset): forgets the round table plans of the
 * objects of the game before. */
void season_bra_states_reset(void);

/* For a history row of competition record rec whose club (winner, else runner-up) is club: 1 when rec is the
 * record of one of the 15 state championships, which plays one state in this game, and the club is of another state
 * (a row that the data files under a region record); 0 when the club is of the record's state; -1 when it is not
 * known (not such a record, a region object that keeps its region, Q603, or a club with no state). */
int season_bra_states_other_state(uint32_t rec, uint32_t club);

#endif
