/* The MLS in a season game (season_mls.c): the original's form with clubs it does not know, and the MLS of real life
 * from 2002 (0.7.2 gap row 7, season_mls_plan.h). */
#ifndef RECOMP_SEASON_MLS_H
#define RECOMP_SEASON_MLS_H

/* The club count of the division in a season of the new form (its rules row), or -1 when the original's form
 * plays (season_clubs.c class_size). */
int season_mls_division_size(const char *division, int season);

#endif
