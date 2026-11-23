/* The Croatian and Greek places of a rules row (0.7.3 slice 3, open items 281 and 282). Pure rules, no game memory:
 * season_sizes_cro.c and season_divisions.c write the results. A row value below 0 is n/a or unknown. */
#ifndef SEASON_CRO_GRE_PLACES_H
#define SEASON_CRO_GRE_PLACES_H

/* The promotion places of a division: the row's for the Druga HNL of one group, else the class's. */
int season_cro_gre_up(const char *file, int one_group, int row_up, int class_up);

/* The relegation places of a lowest division (South of the Druga HNL, Greek second tier): the row's, else the class's. */
int season_cro_gre_down(const char *file, int row_down, int class_down);

/* The relegation places of the Super League over two groups: both groups' promotions, or the row when it sends more. */
int season_gre_prm_down(int group_ups, int row_down);

/* 1 when a season plays one tie between two divisions: one play-off place in each, one group, changing rules on. */
int season_tie_played(int po_up, int po_down, int two_groups, int changing_rules);

/* The clubs before the lower club of the tie: the places before the row's first play-off place, else the direct ones. */
int season_tie_skip(int po_up_first, int direct_up);

#endif
