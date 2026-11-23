/* The periods of a league of any size (season_periods.h; 0.7.2 league sizes, slice 10, D6).
 *
 * D6: the period limits and lengths of the class scale to the rounds of the season, rounded, so that the lengths add
 * up to the rounds. The callers are season_sizes_hol.c (HolFirst) and season_sizes_bel.c (BelSecond). */
#include "season_periods.h"

int season_period_bound(const int *starts, int parts, int class_rounds, int rounds, int k)
{
    if (!starts || parts < 2 || k < 0 || k > parts - 2 || class_rounds < 1) return -1;
    if (rounds < parts || rounds == class_rounds) return starts[k];
    /* round(starts[k] x rounds / class_rounds), half up */
    int b = (2 * starts[k] * rounds + class_rounds) / (2 * class_rounds);
    /* At least one round in each period before it and after it. */
    if (b < k + 1) b = k + 1;
    if (b > rounds - (parts - 1 - k)) b = rounds - (parts - 1 - k);
    /* After the bound before it (the clamps keep the order for any rounds of at least parts). */
    if (k > 0) {
        int before = season_period_bound(starts, parts, class_rounds, rounds, k - 1);
        if (b <= before) b = before + 1;
    }
    return b;
}

int season_period_length(const int *starts, int parts, int class_rounds, int rounds, int p)
{
    if (!starts || parts < 2 || p < 0 || p > parts - 1 || class_rounds < 1) return -1;
    if (rounds < parts) rounds = class_rounds;
    int from = p == 0 ? 0 : season_period_bound(starts, parts, class_rounds, rounds, p - 1);
    int to = p == parts - 1 ? rounds : season_period_bound(starts, parts, class_rounds, rounds, p);
    return to - from;
}

int season_period_stage_done(int clubs, int rounds, int played_sum)
{
    if (clubs < 2 || rounds < 1) return 0;
    int per_round = clubs / 2;      /* an odd count: (clubs - 1) / 2, one club rests */
    return played_sum >= 2 * per_round * rounds;
}

int season_period_own_length(int rounds, int p)
{
    return p >= 2 && rounds >= p ? rounds / p : 0;
}

int season_period_own_of(int rounds, int p, int round)
{
    int len = season_period_own_length(rounds, p);
    if (!len || round < 0 || round >= p * len) return -1;
    return round / len;
}

const char *season_period_name(int period, int abbreviated)
{
    static const char *const names[2][2] = {{"Fifth Period", "Sixth Period"}, {"Period 5", "Period 6"}};
    return period == 5 || period == 6 ? names[abbreviated != 0][period - 5] : (const char *)0;
}
