/* The size step at a season change (0.7.2 league sizes, slice 3, D13 and D18; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * With changing rules on, each division of the size table reaches the club count of its new row after promotion and
 * relegation. A division with fewer clubs takes the best-placed clubs of the division below that did not go up. A
 * division with more sends its worst-placed clubs that stayed down to the division below. The divisions are taken
 * from the top down, so a division below that gives or takes clubs reaches its own row in its turn.
 *
 * The lowest division of a chain (no division below it in the size table) takes the clubs of its nation with no
 * division, the best reputation first, and sends its surplus to no division (as the original relegates from its
 * lowest league, league_promote_and_relegate 0x689c80 at 0x689e58..0x689e94). A reserve or B side, or a club that a
 * second-team rule bars from the division, never goes up by the step.
 *
 * This file holds the plan only, on a model of the divisions and clubs (host memory, no game memory), so a unit test
 * can run it (tests/seasons/test_season_size_step.c). season_sizes.c builds the model from the tables that it saw
 * at the chain and moves the clubs of the plan. */
#ifndef RECOMP_SEASON_SIZE_STEP_H
#define RECOMP_SEASON_SIZE_STEP_H

#define STEP_MAX_ROWS 64
#define STEP_MAX_LOWER 8     /* the Serie C below the Serie B: the division and its 7 groups (0x42afe3..0x42b124) */
#define STEP_NONE (-1)          /* no division */

typedef struct {
    int div;            /* the division (an index of the plan's divisions), or STEP_NONE */
    int nation;         /* any number: the step takes clubs with no division only of the division's nation */
    int rep;            /* reputation: the order of the clubs with no division */
    int up_ok;          /* 0: never moves up by the step (reserve or B side, a second-team bar) */
    int down_ok;        /* 0: never goes down by the step (a club with a human manager) */
    int leave;          /* 1: leaves a division with too many clubs first (a licence refused, gap 37) */
} StepClub;

typedef struct {
    int want;           /* the count of the new row; below 0: the division takes no part */
    int started;        /* 1: the division made its new table already, so the step leaves it */
    int nation;
    int nlower, lower[STEP_MAX_LOWER];      /* the divisions below it in the chain (indexes) */
    int nrows, rows[STEP_MAX_ROWS];         /* its clubs of the last table (club indexes), the best place first */
    int nfeed, feed[STEP_MAX_LOWER];        /* a lowest division: the divisions below it that play as now (gap 37) */
    int short_by;       /* out: the clubs that it lacks after the step (below 0: the clubs over its row) */
} StepDivision;

typedef struct {
    int club, from, to;     /* division indexes, or STEP_NONE */
    int replaces;           /* a club with no division into a feeder: the club whose place it takes, else -1 */
} StepMove;

/* Plans the step: for each division in index order (the top down) the moves that bring it to its row. The model's
 * clubs take their new divisions as the plan goes. Returns the moves written to out (at most max). */
int season_size_step_plan(StepDivision *div, int ndiv, StepClub *club, int nclub, StepMove *out, int max);

/* A division with no chain (season_sizes.c lone_exchange) sends its row's down places to no division, and the step
 * then fills it to want from its nation's clubs with no division (pool: those that may go up). Returns how many of
 * down go: at most pool + have - want, so the pool can replace them (gap 47). want below 0 (no row): none goes, because the step
 * takes no club into a division with no row, so the count stays. */
int season_size_lone_down(int down, int pool, int have, int want);

/* The clubs that a lone exchange sends to no division (lead decision of 15 November): from the worst place up, down
 * clubs with no human manager (human[k] 0; rows are the best place first). A club with a human manager in the last
 * down rows (the places) stays: kept gets its row. One above the places is skipped and is not kept. Returns the rows written to out, the worst first (fewer than down when too few
 * clubs have no human manager); *nkept the rows written to kept. out and kept hold nrows each. */
int season_size_lone_pick(const int *human, int nrows, int down, int *out, int *kept, int *nkept);

#endif
