/* The I liga's promotion play-off after the Ekstraklasa's spring stage (0.6.2 open item 104, season_pol_playoff.c). */
#ifndef RECOMP_SEASON_POL_PLAYOFF_H
#define RECOMP_SEASON_POL_PLAYOFF_H

/* For season_playoffs_hold: the I liga of league table entry e with an edition before year. 0: none; 1: its play-off
 * is complete; 2: it is not, and *file is "pol_second". Read only. */
int season_pol_playoff_unfinished(int e, int year, const char **file);

#endif
