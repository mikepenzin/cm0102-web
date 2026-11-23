/* The rows of the data that go when the game writes the row of its start year (season_history.c, the wrap of
 * comp_util_add_history_record 0x4afce0). */
#ifndef RECOMP_SEASON_HISTORY_START_H
#define RECOMP_SEASON_HISTORY_START_H

/* The data rows of one competition with the label of the start year, before the game writes its row: n rows, and
 * for each the state test of season_bra_states_other_state (1: a club of another state than the one the game plays
 * on the record, 0: of that state, -1: not known). Sets clear[i] to 1 for each row that goes. Returns the count. */
int season_history_start_clear(int n, const int *state, unsigned char *clear);

#endif
