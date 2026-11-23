/* The youth pool rule (0.6.2 slice 20, open item 52, src/recomp_rt/win/youth_pool.c).
 *
 * 1. The cases of the rule: the shares of the 2001 data are not short, the nations of the 2041 forum save are.
 * 2. A model of a world: one loaded nation of 2000 players and 40 nations of 23 to 62 players that are not
 *    loaded, 300 years from 2001, three seeds. Each day a player of 31 or more retires with a chance of 1 in 400,
 *    and each retirement gives one new player, with the ages of regenerate_as_youth_player 0x7abac0: 15 to 20
 *    for the loaded nation. For the others, 23 to 27 when rand(start players) < rand(1000), else 17 to 23. With
 *    the rule of the original alone, small nations run out of players under 21 or under 23. With the fix (young
 *    ages for a short nation, and the new player of a nation that is not short going to a nation that runs out)
 *    no nation of 23 players or more has none under 21 or none under 23 at any check, one each decade. A small
 *    nation grows until its own retirements keep its youth pool: at most 35 players in 300 years. */
#include "youth_pool.h"

#include <stdio.h>
#include <stdlib.h>

static int fails;

static void expect(int got, int want, const char *what)
{
    if (got != want) {
        printf("FAIL %s: %d, not %d\n", what, got, want);
        fails++;
    }
}

static unsigned seed = 1;
static int rnd(int n) { seed = seed * 1103515245u + 12345u; return n > 0 ? (int)((seed >> 16) % (unsigned)n) : 0; }

/* The young ages of 0x7ac3dd. */
static int young_age(void)
{
    int r = rnd(10000);
    if (r == 0) return 15 + rnd(4);
    if (r < 5000) return 16 + rnd(3);
    if (r < 9976) return 17 + rnd(4);
    return 21 + rnd(6);
}

#define NATIONS 41
#define PEOPLE 5000
static int nat[PEOPLE], days[PEOPLE], people, start[NATIONS];
static int count[NATIONS], u20[NATIONS], u21[NATIONS], u22[NATIONS], u23[NATIONS];

static void tally(void)
{
    for (int n = 0; n < NATIONS; n++) count[n] = u20[n] = u21[n] = u22[n] = u23[n] = 0;
    for (int i = 0; i < people; i++) {
        int n = nat[i], a = days[i] / 365;
        count[n]++;
        u20[n] += a < 20;
        u21[n] += a < 21;
        u22[n] += a < 22;
        u23[n] += a < 23;
    }
}

/* Checks, one each decade from 2011, of a nation of 23 players or more with none under 21 or none under 23. */
static int empty_checks(int fix, int *checks, int *smallest_growth)
{
    int empty = 0;
    *checks = 0;
    *smallest_growth = 0;
    for (unsigned s = 1; s <= 3; s++) {
        seed = s;
        people = 0;
        for (int n = 0; n < NATIONS; n++) {
            start[n] = n == 0 ? 2000 : 23 + rnd(40);
            for (int k = 0; k < start[n]; k++, people++) {
                nat[people] = n;
                days[people] = (17 + rnd(15)) * 365 + rnd(365);
            }
        }
        for (int d = 0; d < 300 * 365; d++) {
            if (d % 3650 == 3649) {
                tally();
                for (int n = 1; n < NATIONS; n++)
                    if (count[n] >= YOUTH_POOL_SQUAD) {
                        (*checks)++;
                        empty += u21[n] == 0 || u23[n] == 0;
                    }
            }
            int counted = 0;
            for (int i = 0; i < people; i++) {
                if (++days[i] < 31 * 365 || rnd(400)) continue;
                if (!counted) tally();
                counted = 1;
                int n = nat[i], a = days[i] / 365, young = n == 0;
                count[n]--, u20[n] -= a < 20, u21[n] -= a < 21, u22[n] -= a < 22, u23[n] -= a < 23;
                int needs = youth_pool_short(count[n], u21[n], u23[n]) || youth_pool_runs_out(count[n], u20[n], u22[n]);
                if (fix && needs && youth_pool_older_rule(n == 0 ? 2 : 0, start[n]))
                    young = 1;
                else if (fix && !needs) {
                    int best = -1;            /* the nation that runs out with the most players, as youth_pool.c */
                    for (int x = 1; x < NATIONS; x++)
                        if (x != n && youth_pool_runs_out(count[x], u20[x], u22[x]) &&
                            (best < 0 || count[x] > count[best]))
                            best = x;
                    if (best >= 0) n = best, young = 1;
                }
                a = young ? young_age() : rnd(start[n]) < rnd(1000) ? 23 + rnd(5) : 17 + rnd(7);
                nat[i] = n;
                days[i] = a * 365 + rnd(365);
                count[n]++, u20[n] += a < 20, u21[n] += a < 21, u22[n] += a < 22, u23[n] += a < 23;
            }
        }
        tally();
        for (int n = 1; n < NATIONS; n++)
            if (count[n] - start[n] > *smallest_growth && start[n] < 30) *smallest_growth = count[n] - start[n];
    }
    return empty;
}

int main(void)
{
    expect(youth_pool_age(2041, 20, 2020, 19), 21, "age the day after the birthday");
    expect(youth_pool_age(2041, 20, 2020, 21), 20, "age the day before the birthday");
    expect(youth_pool_age(2041, 20, 2020, 20), 21, "age on the birthday");

    expect(youth_pool_short(31, 2, 4), 0, "Jamaica 2001 (all leagues): 6 % under 21, 13 % under 23");
    expect(youth_pool_short(31, 3, 5), 0, "Norway 2001 (England only)");
    expect(youth_pool_short(47, 0, 0), 1, "Belgium 2041 (forum save)");
    expect(youth_pool_short(44, 0, 1), 1, "Austria 2041 (forum save)");
    expect(youth_pool_short(109, 6, 11), 0, "109 players, 6 under 21, 11 under 23");
    expect(youth_pool_short(109, 5, 11), 1, "109 players, 5 under 21");
    expect(youth_pool_short(109, 6, 10), 1, "109 players, 10 under 23");
    expect(youth_pool_short(22, 0, 0), 0, "22 players fill no squad");
    expect(youth_pool_short(23, 0, 3), 1, "23 players, none under 21");
    expect(youth_pool_short(23, 2, 3), 0, "23 players, 2 under 21, 3 under 23");

    expect(youth_pool_older_rule(0, 349), 1, "not loaded, 349 players at the start");
    expect(youth_pool_older_rule(0, 350), 0, "not loaded, 350 players at the start");
    expect(youth_pool_older_rule(2, 10), 0, "a loaded nation");

    int checks, growth, before = empty_checks(0, &checks, &growth);
    printf("rule of the original: %d of %d decade checks find a nation with none under 21 or under 23\n", before,
           checks);
    expect(before > checks / 10, 1, "the rule of the original empties the youth pool of small nations");
    int after = empty_checks(1, &checks, &growth);
    printf("with the fix: %d of %d, the most a nation of under 30 players grew: %d\n", after, checks, growth);
    expect(after, 0, "decade checks with none under 21 or under 23 with the fix");
    expect(growth <= 40, 1, "a nation of under 30 players grows by 40 or fewer in 300 years (35 measured)");

    if (fails) return 1;
    printf("youth pool: all checks pass\n");
    return 0;
}
