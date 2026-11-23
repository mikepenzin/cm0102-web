/* The plan of the Swedish third tier in two groups (issue 106). Pure code, so ctest season_swe_groups links it alone.
 * The game side is season_swe_groups.c. */
#ifndef RECOMP_SEASON_SWE_GROUPS_PLAN_H
#define RECOMP_SEASON_SWE_GROUPS_PLAN_H

typedef struct {
    int clubs;      /* the clubs of the division */
    int size;       /* the clubs of each group */
    int rounds;     /* the rounds of a group: each pair meets twice */
    int up;         /* the places up of each group */
    int down;       /* the places down of each group */
} SweGroupsPlan;

/* 1 and the plan when the row's split reads "2 groups of N" and its count is 2 x N, else 0 (the class's six groups). */
int season_swe_groups_plan(const char *split, int clubs, int up, int down, SweGroupsPlan *out);

/* The group of each of n clubs: 0 Norra for the `size` clubs of the highest score, 1 Sodra for the others. Clubs of an
 * equal score keep their order. n is at most SWE_GROUPS_MAX_CLUBS. */
#define SWE_GROUPS_MAX_CLUBS 128
void season_swe_groups_deal(const int *north, int n, int size, int *group);

#endif
