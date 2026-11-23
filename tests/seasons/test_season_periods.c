/* The periods of a league of any size (0.7.2 league sizes, slice 10, D6): season_periods.c.
 *
 * The class periods here are the original's: HolFirst::add_result 0x5f65b0 starts periods 1, 2 and 3 at rounds 8, 16
 * and 26 (0x5f65ce, 0x5f65da, 0x5f65e6) of 34, and HolFirst::create_period_stage 0x5f6ad0 gives them 8, 8, 10 and 8
 * matches (0x5f6be2, 0x5f6bd6). BelSecond::add_result 0x420110 starts periods 1 and 2 at rounds 10 and 22 (0x42012e,
 * 0x42013a) of 34, and BelSecond::create_period_stage 0x4204d0 gives them 10, 12 and 12 (0x4205b9, 0x4205cb). */
#include "season_periods.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

static const int HOL[] = {8, 16, 26};
static const int BEL[] = {10, 22};

/* Every period has at least one round, the lengths add up to the rounds, and each length is the gap of its bounds. */
static void check_parts(const char *name, const int *starts, int parts, int rounds)
{
    int sum = 0, prev = 0;
    for (int p = 0; p < parts; p++) {
        int len = season_period_length(starts, parts, 34, rounds, p);
        int end = p + 1 < parts ? season_period_bound(starts, parts, 34, rounds, p) : rounds;
        CHECK(len >= 1, "%s %d: period %d has %d rounds", name, rounds, p, len);
        CHECK(len == end - prev, "%s %d: period %d length %d, bounds %d..%d", name, rounds, p, len, prev, end);
        sum += len;
        prev = end;
    }
    CHECK(sum == rounds, "%s %d: lengths add up to %d", name, rounds, sum);
}

int main(void)
{
    /* Gap 33: a row's own count of periods, R / P rounds each from round 0, the rest in no period (-1). The Eerste
     * Divisie 2005/06: six periods of six matches in 38 rounds; First Division B 2016/17: two periods of 14 in 28. */
    CHECK(season_period_own_length(38, 6) == 6 && season_period_own_length(28, 2) == 14, "own lengths");
    CHECK(season_period_own_length(5, 6) == 0 && season_period_own_length(28, 1) == 0, "own: no length");
    CHECK(season_period_own_of(38, 6, 0) == 0 && season_period_own_of(38, 6, 5) == 0 &&
          season_period_own_of(38, 6, 6) == 1 && season_period_own_of(38, 6, 35) == 5 &&
          season_period_own_of(38, 6, 36) == -1 && season_period_own_of(38, 6, 37) == -1, "own: periods of 38/6");
    CHECK(season_period_own_of(28, 2, 13) == 0 && season_period_own_of(28, 2, 14) == 1 &&
          season_period_own_of(28, 2, 27) == 1, "own: periods of 28/2");

    /* The class's own rounds give the class's bounds and lengths (the base game and a league at its class count). */
    for (int k = 0; k < 3; k++)
        CHECK(season_period_bound(HOL, 4, 34, 34, k) == HOL[k], "HolFirst bound %d at 34", k);
    for (int k = 0; k < 2; k++)
        CHECK(season_period_bound(BEL, 3, 34, 34, k) == BEL[k], "BelSecond bound %d at 34", k);
    CHECK(season_period_length(HOL, 4, 34, 34, 2) == 10 && season_period_length(HOL, 4, 34, 34, 0) == 8,
          "HolFirst lengths at 34");
    CHECK(season_period_length(BEL, 3, 34, 34, 0) == 10 && season_period_length(BEL, 3, 34, 34, 2) == 12,
          "BelSecond lengths at 34");

    /* Eerste Divisie of 20 (38 rounds): 8 x 38 / 34 = 8.94 -> 9, 16 x 38 / 34 = 17.9 -> 18, 26 x 38 / 34 = 29.06 -> 29. */
    CHECK(season_period_bound(HOL, 4, 34, 38, 0) == 9 && season_period_bound(HOL, 4, 34, 38, 1) == 18 &&
          season_period_bound(HOL, 4, 34, 38, 2) == 29, "HolFirst bounds at 38");
    CHECK(season_period_length(HOL, 4, 34, 38, 3) == 9, "HolFirst last period at 38");
    /* Belgian Second Division of 19 (38 rounds): 11.2 -> 11, 24.6 -> 25. Of 8 in four meetings (28): 8.2 -> 8, 18.1 -> 18. */
    CHECK(season_period_bound(BEL, 3, 34, 38, 0) == 11 && season_period_bound(BEL, 3, 34, 38, 1) == 25,
          "BelSecond bounds at 38");
    CHECK(season_period_bound(BEL, 3, 34, 28, 0) == 8 && season_period_bound(BEL, 3, 34, 28, 1) == 18,
          "BelSecond bounds at 28");
    for (int r = 3; r <= 60; r++) {
        check_parts("HolFirst", HOL, 4, r < 4 ? 4 : r);
        check_parts("BelSecond", BEL, 3, r);
    }
    /* A bound the class does not have, or too few rounds for the periods: the original value. */
    CHECK(season_period_bound(HOL, 4, 34, 2, 1) == 16, "HolFirst at 2 rounds keeps its bound");
    CHECK(season_period_length(HOL, 4, 34, 2, 1) == 8, "HolFirst at 2 rounds keeps its length");
    CHECK(season_period_bound(HOL, 4, 34, 38, 3) == -1, "no bound after the last period");

    /* A period stage of an even count is done when every club played its rounds; of an odd count (a club rests each
     * round, (n - 1) / 2 matches a round) when its matches are played: the played of all rows add up to twice the
     * matches. */
    CHECK(season_period_stage_done(20, 9, 20 * 9), "20 clubs, 9 rounds, all played");
    CHECK(!season_period_stage_done(20, 9, 20 * 9 - 2), "20 clubs, 9 rounds, one match to play");
    CHECK(season_period_stage_done(19, 11, 11 * 18), "19 clubs, 11 rounds, 99 matches played");
    CHECK(!season_period_stage_done(19, 11, 11 * 18 - 2), "19 clubs, 11 rounds, one match to play");
    CHECK(!season_period_stage_done(19, 0, 0), "a stage of no rounds is not a period");

    /* The fifth and sixth periods of the Eerste Divisie 2005/06 to 2008/09 (gap 33): the original's words ("Fourth
     * Period" long and plural, "Period 4" short: 0x4b9a44, 0x4ba0d0, 0x4ba758 at id 0x43c) with the next number. */
    CHECK(!strcmp(season_period_name(5, 0), "Fifth Period") && !strcmp(season_period_name(6, 0), "Sixth Period"),
          "long names of periods 5 and 6");
    CHECK(!strcmp(season_period_name(5, 1), "Period 5") && !strcmp(season_period_name(6, 1), "Period 6"),
          "short names of periods 5 and 6");
    CHECK(season_period_name(4, 0) == NULL && season_period_name(7, 1) == NULL, "the game names periods 1 to 4");

    if (fails) printf("%d failures\n", fails);
    else printf("season_periods: all checks pass\n");
    return fails != 0;
}
