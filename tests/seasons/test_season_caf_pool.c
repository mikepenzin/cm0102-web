/* Test of the CAF World Cup qualifying plan for every African nation (0.6.2 item 199,
 * src/recomp_rt/win/season_caf_pool_plan.c).
 *
 * Usage: season_caf_pool_test
 * The checks: the original's round of 50 stays for a pool of 50; a larger pool takes groups of 6 in the old shape
 * (53 nations: 46 in the first round, 7 byes, as the 42 and 9 of 2006); a World Cup of 48 teams keeps groups of 5
 * (54 nations, 9 groups: 18 in the first round); for every pool and group count the byes and the first round's
 * winners fill the groups exactly, and no pool too small for one tie gets a plan. */
#include "season_caf_pool.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static void one(int pool, int groups, int wcq26, int entrants, int first, int size)
{
    SeasonCafPlan p;
    CHECK(season_caf_plan(pool, groups, wcq26, &p) == 1, "pool %d: no plan", pool);
    CHECK(p.entrants == entrants && p.first == first && p.group_size == size,
          "pool %d groups %d wcq26 %d: %d entrants, %d first, size %d", pool, groups, wcq26, p.entrants, p.first,
          p.group_size);
}

int main(void)
{
    one(50, 5, 0, 50, 50, 5);       /* the original: 25 ties, no bye */
    one(52, 5, 0, 52, 44, 6);
    one(53, 5, 0, 53, 46, 6);
    one(54, 5, 0, 54, 48, 6);
    one(70, 5, 0, 60, 60, 6);       /* more than two rounds can take: the best 60 */
    one(54, 9, 1, 54, 18, 5);       /* 2026: 9 groups of 5 */
    one(53, 8, 1, 53, 26, 5);       /* 2030: 8 groups, Morocco a host */
    for (int wcq26 = 0; wcq26 < 2; wcq26++)
        for (int groups = 1; groups <= 10; groups++)
            for (int pool = 0; pool <= 120; pool++) {
                SeasonCafPlan p;
                int ok = season_caf_plan(pool, groups, wcq26, &p);
                int size = wcq26 || pool <= 10 * groups ? 5 : 6;
                if (pool <= groups * size) {
                    CHECK(!ok, "pool %d groups %d: a plan with no tie", pool, groups);
                    continue;
                }
                CHECK(ok, "pool %d groups %d wcq26 %d: no plan", pool, groups, wcq26);
                CHECK(p.first >= 2 && p.first % 2 == 0 && p.first <= p.entrants && p.entrants <= pool,
                      "pool %d groups %d: first %d of %d", pool, groups, p.first, p.entrants);
                CHECK(p.entrants - p.first + p.first / 2 == groups * p.group_size,
                      "pool %d groups %d: %d byes and %d winners for %d places", pool, groups,
                      p.entrants - p.first, p.first / 2, groups * p.group_size);
            }
    printf(failures ? "FAILED: %d\n" : "PASS\n", failures);
    return failures != 0;
}
