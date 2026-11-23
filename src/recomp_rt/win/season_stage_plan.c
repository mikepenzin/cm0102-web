/* The plan of a qualifying stage's match days (items 203 and 204 of 0.6.2). Pure code (season_stage_plan.h).
 *
 * The cause. Each qualifier moved on its own, as add_fixture stored it, to the nearest free FIFA day of its year
 * (season_friendly_dates.c fifa_target_by), with an end bound but no start bound and no matchday order. In a 2021
 * start (run set a2021s1, replay of 2031 to 2033) the AFC third round of the 2034 cycle has 10 weekly matchdays on
 * days 230 to 293 of 2033, and the FIFA days in reach were 159 to 164 and 271 to 281. Matchdays 1 to 4 took 271, 274,
 * 278 and 281; matchdays 5 and 6 went back to June (164 and 161), before matchday 1 and in reverse order (item 203);
 * matchdays 7 and 8 found no free FIFA day, kept 272 and 279, 1 or 2 days from matchdays 3 and 4 of the same sides,
 * and add_fixture's clash move (0x5aaa25) moved them 3 to 5 days (item 204).
 *
 * Real life: the rounds of a qualifying stage play in order, on the FIFA windows; the AFC third round of the 2026
 * cycle played its 10 matchdays in the windows of September, October and November 2024 and March and June 2025, two
 * a window (en.wikipedia.org, "2026 FIFA World Cup qualification – AFC third round").
 *
 * The rule. The stage's days are planned once, from its round table, before its fixtures are made: rounds keep their
 * order, at least SEASON_STAGE_GAP days apart; each takes a FIFA day in reach, else its own day, else another day
 * between its neighbours; the plan with the most rounds on FIFA days and then the least total move wins. A small
 * dynamic programme over the rounds and the candidate days finds it: best[r][z] is the best score of rounds 0 to r
 * with round r on day z, and round r + 1 on day z' takes the best of round r on any day up to z' - GAP (a running
 * maximum over z). */
#include "season_stage_plan.h"
#include "season_uefa_days.h"

#include <string.h>

#define FIFA_SCORE 1000000LL            /* a round on a FIFA day outweighs any total move (spans are under 1200) */
#define NONE (-(1LL << 60))

static long long best[SEASON_STAGE_MAX][SEASON_STAGE_SPAN];
static long long run_best[SEASON_STAGE_MAX][SEASON_STAGE_SPAN];   /* max of best[r][0..i] */
static short run_at[SEASON_STAGE_MAX][SEASON_STAGE_SPAN];         /* the earliest i that gives it */
static short from_at[SEASON_STAGE_MAX][SEASON_STAGE_SPAN];        /* round r - 1's day for best[r][i] */

static int year_of(long z)
{
    int y, m, d;
    season_from_abs_day(z, &y, &m, &d);
    return y;
}

int season_stage_plan(const long own[], int n, long first, long last, long last_fifa, SeasonStageFifa fifa,
                      void *ctx, long out[])
{
    for (int r = 0; r < n && r < SEASON_STAGE_MAX; r++) out[r] = own[r];
    if (n < 1 || n > SEASON_STAGE_MAX || !fifa) return -1;
    for (int r = 1; r < n; r++)
        if (own[r] <= own[r - 1]) return -1;
    long d0 = own[0] < first ? own[0] : first, d1 = own[n - 1] > last_fifa ? own[n - 1] : last_fifa;
    if (last > d1) d1 = last;
    if (d1 - d0 + 1 > SEASON_STAGE_SPAN || d1 < d0) return -1;
    int span = (int)(d1 - d0 + 1);
    static signed char is_fifa[SEASON_STAGE_SPAN];
    for (int i = 0; i < span; i++) is_fifa[i] = (signed char)(fifa(d0 + i, ctx) != 0);
    for (int r = 0; r < n; r++) {
        int ry = year_of(own[r]);
        for (int i = 0; i < span; i++) {
            long z = d0 + i;
            best[r][i] = NONE;
            from_at[r][i] = -1;
            int on_fifa = is_fifa[i] && z >= first && z <= last_fifa;
            int ok = year_of(z) == ry && (z == own[r] || on_fifa || (z >= first && z <= last));
            if (!ok) continue;
            long move = z > own[r] ? z - own[r] : own[r] - z;
            long long v = (is_fifa[i] && (on_fifa || z == own[r]) ? FIFA_SCORE : 0) - move;
            if (r == 0) {
                best[r][i] = v;
                continue;
            }
            int j = i - SEASON_STAGE_GAP;
            if (j < 0 || run_best[r - 1][j] == NONE) continue;
            best[r][i] = v + run_best[r - 1][j];
            from_at[r][i] = run_at[r - 1][j];
        }
        for (int i = 0; i < span; i++) {
            if (i && run_best[r][i - 1] >= best[r][i]) {
                run_best[r][i] = run_best[r][i - 1];
                run_at[r][i] = run_at[r][i - 1];
            } else {
                run_best[r][i] = best[r][i];
                run_at[r][i] = (short)i;
            }
        }
    }
    if (run_best[n - 1][span - 1] == NONE) return -1;
    int i = run_at[n - 1][span - 1], on = 0;
    for (int r = n - 1; r >= 0; r--) {
        out[r] = d0 + i;
        on += is_fifa[i] && ((out[r] >= first && out[r] <= last_fifa) || out[r] == own[r]);
        i = from_at[r][i];
        if (r && i < 0) {                                       /* cannot happen: best[r] had a round r - 1 */
            for (int k = 0; k < n; k++) out[k] = own[k];
            return -1;
        }
    }
    return on;
}

void season_stage_window(const long plan[], int n, int r, long first, long last, long *lo, long *hi)
{
    /* The days between two planned days a < b are split: round r - 1 may go up to a + (b - a - GAP) / 2, round r
     * from there + GAP. So a fixture of round r - 1 that cannot take its day never passes one of round r that
     * cannot take its own, and the two stay GAP days apart (review of items 203 and 204). */
    *lo = r > 0 ? plan[r - 1] + (plan[r] - plan[r - 1] - SEASON_STAGE_GAP) / 2 + SEASON_STAGE_GAP : first;
    *hi = r + 1 < n ? plan[r] + (plan[r + 1] - plan[r] - SEASON_STAGE_GAP) / 2 : last;
    if (*lo < first) *lo = first;
}
