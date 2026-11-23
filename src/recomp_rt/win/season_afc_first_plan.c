/* The plan and the days of the AFC first round (item 196, season_afc_first.h). Pure code.
 *
 * Real life (lead-notes research 196, rows of int-world.tsv wc_asia_league): the AFC played a two-leg first round
 * of its weakest nations, ordered by the FIFA ranking, before the groups: 2006 (14 teams), 2018 and 2022 (12),
 * 2026 (20). The game orders the round by the value its pot seeding uses: the national team's reputation (club
 * +0x80, compare_clubs_by_reputation_desc_nulls_last 0x4b07d0, the sort of the pool builder 0x913f60 at
 * 0x9141cf, whose order the draw 0x912bd0 deals into the pots). The days: 12 and 17 October 2023 in the 2026 cycle,
 * the latest of the real first rounds that falls in the game's time between the pool (25 July of c) and the draw of
 * the groups (25 March of c + 1). */
#include "season_afc_first.h"
#include "season_uefa_days.h"

#include <stdlib.h>
#include <string.h>

int season_afc_first_plan(int pool, int places, SeasonAfcFirstPlan *plan)
{
    memset(plan, 0, sizeof *plan);
    if (places < 0) places = 0;
    if (places > AFC_FIRST_MAX_POOL / 2) places = AFC_FIRST_MAX_POOL / 2;
    if (pool > 2 * places) pool = 2 * places;
    if (pool < 0) pool = 0;
    int w = pool > places ? pool - places : 0;
    plan->ties = w;
    plan->byes = places - w;
    if (w == 0) {
        for (int i = 0; i < pool; i++) plan->list[i] = i;
        plan->byes = pool;
        return 0;
    }
    int first = pool - 2 * w;                   /* the first of the 2W: the bottom of the pool */
    for (int k = 0; k < w; k++) {
        plan->pair[k][0] = first + k;
        plan->pair[k][1] = pool - 1 - k;
    }
    for (int i = 0; i < plan->byes; i++) plan->list[i] = i;
    for (int k = 0; k < w; k++) plan->list[plan->byes + k] = plan->pair[k][0];
    return w;
}

static int cmp_long(const void *a, const void *b)
{
    long x = *(const long *)a, y = *(const long *)b;
    return x < y ? -1 : x > y;
}

long season_afc_first_build_day(int c) { return season_abs_day(c, 8, 1); }

int season_afc_first_days(int c, long today, long groups_draw, SeasonAfcFirstFree free_fn, void *ctx, long *draw,
                          long *leg1, long *leg2)
{
    long days[2 * SEASON_FIFA_YEAR_MAX];
    int n = 0, found = 0;
    for (int y = c; y <= c + 1; y++) {
        int doys[SEASON_FIFA_YEAR_MAX], k = season_fifa_year_days(y, doys, SEASON_FIFA_YEAR_MAX);
        long base = season_abs_day(y, 1, 1);
        for (int i = 0; i < k; i++) days[n++] = base + doys[i];
    }
    qsort(days, (size_t)n, sizeof days[0], cmp_long);
    long last = groups_draw - AFC_FIRST_DRAW_GAP;
    /* The passes: a free window from 1 October, a free window from August (the build day), the first window from
     * 1 October, the first window from August. */
    for (int pass = 0; pass < 4 && !found; pass++) {
        long from = pass % 2 == 0 ? season_abs_day(c, 10, 1) : season_abs_day(c, 8, 1);
        if (from < today + 8) from = today + 8;
        for (int i = 0; i < n && !found; i++) {
            if (days[i] < from) continue;
            for (int j = i + 1; j < n && days[j] - days[i] <= 6 && !found; j++) {
                if (days[j] - days[i] < 3 || days[j] > last) continue;
                if (pass < 2 && free_fn && !free_fn(days[i], days[j], ctx)) continue;
                *leg1 = days[i];
                *leg2 = days[j];
                found = 1;
            }
        }
    }
    if (found) *draw = *leg1 - 7 > today ? *leg1 - 7 : today + 1;
    return found;
}
