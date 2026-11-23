/* Test of the World Cup qualifying places with hosts (0.6.2 slice 5, open item 59,
 * src/recomp_rt/win/season_wcq26_places.c).
 *
 * Usage: season_wcq26_places_test
 * The checks: every set of 0 to 6 hosts over the six confederations gives 48 teams (the direct places, the hosts
 * and the 2 of the play-off tournament), within the shapes' limits (UEFA keeps 12 groups and 1 path, so at most 3
 * European hosts; CAF keeps 5 groups, so at most 4 African hosts); the editions of the table and Saudi Arabia
 * 2034; the AFC places split over its two final groups of 5. */
#include "season_wcq26.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

static int teams(const int h[WCQ_CONFS], int direct[WCQ_CONFS])
{
    season_wcq26_places(h, direct);
    int n = 2;
    for (int c = 0; c < WCQ_CONFS; c++) n += direct[c] + h[c];
    return n;
}

static void edition(const char *name, int uefa, int conmebol, int concacaf, int caf, int afc, int ofc,
                    const int want[WCQ_CONFS])
{
    int h[WCQ_CONFS] = {uefa, conmebol, concacaf, caf, afc, ofc}, d[WCQ_CONFS];
    CHECK(teams(h, d) == 48, "%s: %d teams", name, teams(h, d));
    for (int c = 0; c < WCQ_CONFS; c++) CHECK(d[c] == want[c], "%s: confederation %d has %d places, not %d", name, c, d[c], want[c]);
}

int main(void)
{
    /* Every host set of 0 to 6 hosts. */
    int sets = 0;
    int h[WCQ_CONFS], d[WCQ_CONFS];
    for (h[0] = 0; h[0] <= 3; h[0]++)
        for (h[1] = 0; h[1] <= 6; h[1]++)
            for (h[2] = 0; h[2] <= 6; h[2]++)
                for (h[3] = 0; h[3] <= 4; h[3]++)
                    for (h[4] = 0; h[4] <= 6; h[4]++)
                        for (h[5] = 0; h[5] <= 6; h[5]++) {
                            int total = 0;
                            for (int c = 0; c < WCQ_CONFS; c++) total += h[c];
                            if (total > 6) continue;
                            sets++;
                            int n = teams(h, d);
                            CHECK(n == 48, "hosts %d %d %d %d %d %d: %d teams", h[0], h[1], h[2], h[3], h[4], h[5], n);
                            for (int c = 0; c < WCQ_CONFS; c++) CHECK(d[c] >= 0, "confederation %d: %d places", c, d[c]);
                        }
    CHECK(sets > 800, "%d host sets", sets);

    /* The editions: no host (after 2030), 2026, 2030 and Saudi Arabia 2034; one and two OFC hosts. */
    edition("no host", 0, 0, 0, 0, 0, 0, (const int[]){16, 6, 6, 9, 8, 1});
    edition("2026", 0, 0, 3, 0, 0, 0, (const int[]){16, 6, 3, 9, 8, 1});
    edition("2030", 2, 3, 0, 1, 0, 0, (const int[]){14, 3, 6, 8, 8, 1});
    edition("2034", 0, 0, 0, 0, 1, 0, (const int[]){16, 6, 6, 9, 7, 1});
    edition("Japan and South Korea", 0, 0, 0, 0, 2, 0, (const int[]){16, 6, 6, 9, 6, 1});
    edition("one OFC host", 0, 0, 0, 0, 0, 1, (const int[]){16, 6, 6, 9, 8, 0});
    edition("two OFC hosts", 0, 0, 0, 0, 0, 2, (const int[]){16, 6, 6, 9, 7, 0});

    /* The AFC places over its two final groups of 5: group A the odd one, each at most 4 (the fifth plays the
     * play-off). */
    for (int p = 0; p <= 8; p++) {
        int a = season_wcq26_asia_group(p, 0), b = season_wcq26_asia_group(p, 1);
        CHECK(a + b == p, "%d AFC places: %d and %d", p, a, b);
        CHECK(a >= b && a - b <= 1 && a <= 4, "%d AFC places: %d and %d", p, a, b);
    }
    CHECK(season_wcq26_asia_group(7, 0) == 4 && season_wcq26_asia_group(7, 1) == 3, "Saudi Arabia: 4 and 3");

    if (failures) {
        fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    printf("season_wcq26_places: %d host sets, 48 teams each\n", sets);
    return 0;
}
