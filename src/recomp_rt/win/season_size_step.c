/* The size step at a season change: the plan (season_size_step.h). */
#include "season_size_step.h"

static int count_in(const StepClub *club, int nclub, int d)
{
    int n = 0;
    for (int i = 0; i < nclub; i++) n += club[i].div == d;
    return n;
}

/* 1 when a division below takes part: it has a row and has not made its new table. */
static int open_division(const StepDivision *div, int ndiv, int d)
{
    return d >= 0 && d < ndiv && div[d].want >= 0 && !div[d].started;
}

static int add_move(StepMove *out, int max, int *n, StepClub *club, int c, int to)
{
    if (*n >= max) return 0;
    out[*n] = (StepMove){c, club[c].div, to, -1};
    (*n)++;
    club[c].div = to;
    return 1;
}

/* The club of no division of a nation with the best reputation that may go up, or -1. */
static int best_free(const StepClub *club, int nclub, int nation)
{
    int best = -1;
    for (int c = 0; c < nclub; c++) {
        if (club[c].div != STEP_NONE || club[c].nation != nation || !club[c].up_ok) continue;
        if (best < 0 || club[c].rep > club[best].rep) best = c;
    }
    return best;
}

int season_size_step_plan(StepDivision *div, int ndiv, StepClub *club, int nclub, StepMove *out, int max)
{
    int n = 0;
    for (int d = 0; d < ndiv; d++) {
        StepDivision *v = &div[d];
        v->short_by = 0;
        if (v->want < 0 || v->started) continue;
        int open = 0;
        for (int k = 0; k < v->nlower; k++) open += open_division(div, ndiv, v->lower[k]);
        int have = count_in(club, nclub, d);
        if (have < v->want) {
            int need = v->want - have;
            if (open) {
                /* The best place first, the divisions below in their order at each place. */
                for (int place = 0; place < STEP_MAX_ROWS && need > 0; place++)
                    for (int k = 0; k < v->nlower && need > 0; k++) {
                        int l = v->lower[k];
                        if (!open_division(div, ndiv, l) || place >= div[l].nrows) continue;
                        int c = div[l].rows[place];
                        if (c < 0 || c >= nclub || club[c].div != l || !club[c].up_ok) continue;
                        if (add_move(out, max, &n, club, c, d)) need--;
                    }
            } else if (!v->nlower) {
                /* Gap 37: a lowest division whose divisions below play as now (the Ykkonen groups) takes their best
                 * placed clubs that stayed, the places in turn, as real life promoted more clubs. The club of its
                 * nation with no division of the best reputation takes each one's place, so the group keeps its
                 * count; with none left, no club of the group goes up. */
                for (int place = 0; place < STEP_MAX_ROWS && need > 0; place++)
                    for (int k = 0; k < v->nfeed && need > 0; k++) {
                        int f = v->feed[k];
                        if (f < 0 || f >= ndiv || place >= div[f].nrows) continue;
                        int c = div[f].rows[place];
                        if (c < 0 || c >= nclub || club[c].div != f || !club[c].up_ok) continue;
                        int r = best_free(club, nclub, v->nation);
                        if (r < 0 || n + 2 > max) continue;
                        add_move(out, max, &n, club, c, d);
                        add_move(out, max, &n, club, r, f);
                        out[n - 1].replaces = c;
                        need--;
                    }
                /* The lowest division: the clubs of its nation with no division, the best reputation first. A
                 * division with feeders takes none straight: their up_ok was checked against the feeder. */
                while (need > 0 && !v->nfeed) {
                    int best = -1;
                    for (int c = 0; c < nclub; c++) {
                        if (club[c].div != STEP_NONE || club[c].nation != v->nation || !club[c].up_ok) continue;
                        if (best < 0 || club[c].rep > club[best].rep) best = c;
                    }
                    if (best < 0 || !add_move(out, max, &n, club, best, d)) break;
                    need--;
                }
            }
            v->short_by = need;
        } else if (have > v->want) {
            int surplus = have - v->want;
            /* Gap 37: a club that lost its licence leaves first, to no division. */
            for (int r = 0; r < v->nrows && surplus > 0; r++) {
                int c = v->rows[r];
                if (c < 0 || c >= nclub || club[c].div != d || !club[c].leave || !club[c].down_ok) continue;
                if (add_move(out, max, &n, club, c, STEP_NONE)) surplus--;
            }
            /* The worst place first, only the clubs of its last table that are still in it. */
            for (int r = v->nrows - 1; r >= 0 && surplus > 0; r--) {
                int c = v->rows[r];
                if (c < 0 || c >= nclub || club[c].div != d || !club[c].down_ok) continue;
                int to = STEP_NONE;
                if (open) {
                    /* The division below with the most need. */
                    int most = 0;
                    to = -2;
                    for (int k = 0; k < v->nlower; k++) {
                        int l = v->lower[k];
                        if (!open_division(div, ndiv, l)) continue;
                        int need = div[l].want - count_in(club, nclub, l);
                        if (to == -2 || need > most) to = l, most = need;
                    }
                } else if (v->nlower) {
                    continue;
                }
                if (add_move(out, max, &n, club, c, to)) surplus--;
            }
            v->short_by = -surplus;
        }
    }
    return n;
}

int season_size_lone_down(int down, int pool, int have, int want)
{
    if (down <= 0) return 0;
    if (want < 0) return 0;     /* no row: the step takes no club back, so none goes */
    int most = pool + have - want;
    if (most < 0) most = 0;
    return down < most ? down : most;
}

int season_size_lone_pick(const int *human, int nrows, int down, int *out, int *kept, int *nkept)
{
    int n = 0;
    *nkept = 0;
    for (int k = nrows - 1; k >= 0 && n < down; k--) {
        if (!human[k]) out[n++] = k;
        else if (k >= nrows - down) kept[(*nkept)++] = k;   /* only a club in the places stays; one above is safe */
    }
    return n;
}
