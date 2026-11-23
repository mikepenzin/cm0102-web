/* Test of the open order of level clubs under head-to-head (0.6.2 open item 167, src/recomp_rt/win/season_level_ties.c).
 *
 * The case of the run set (pass 3, a2026s3, I liga 2074/75): places 2 and 3 ended level on 52 points. On day 144 of
 * 2075 the champion was on 52 too with a match left, the head-to-head table of the level clubs held three clubs, and
 * its order of the two finished clubs was not the order of the two alone (day 147). The original took the order of
 * two finished level clubs as final (0x684fc9..0x684fd5, 0x6850a0..0x6850ae) and gave both the promotion status in
 * turn. A save cannot replay that tie: a load changes the later match results. So the days' real tables are here. */
#include "season_level_ties.h"

#include <stdio.h>

static int failures;

#define CHECK(cond, ...) \
    do { if (!(cond)) { failures++; printf("FAIL line %d, expected: ", __LINE__); printf(__VA_ARGS__); \
         printf("\n"); } } while (0)

int main(void)
{
    /* The I liga 2074/75 of the run set (pass 3, a2026s3), all 18 rows (played, points) after the matches of day 144
     * of 2075, read by a probe of a replay from leg003's save: the champion on 52 with a match left, places 2 and 3
     * finished on 52. The original gave the second of the three-club head-to-head table promotion that day. */
    LevelRow d144[] = {{33, 52}, {34, 52}, {34, 52}, {34, 50}, {34, 48}, {34, 48}, {34, 48}, {34, 48}, {34, 47},
                       {34, 47}, {34, 46}, {34, 46}, {34, 46}, {34, 43}, {34, 42}, {33, 41}, {34, 40}, {34, 33}};
    CHECK(level_ties_open(d144, 18, 34, 3) == 1, "day 144 of 2075: the champion can still leave 52: open");
    /* Day 147: the champion won (53) and every club has finished: the order of the two on 52 is final. */
    LevelRow d147[] = {{34, 53}, {34, 52}, {34, 52}, {34, 50}, {34, 48}, {34, 48}, {34, 48}, {34, 48}, {34, 47},
                       {34, 47}, {34, 46}, {34, 46}, {34, 46}, {34, 43}, {34, 42}, {34, 42}, {34, 40}, {34, 33}};
    CHECK(level_ties_open(d147, 18, 34, 3) == 0, "day 147 of 2075: all clubs finished: settled");

    /* A level club with a match left can leave. */
    LevelRow b[] = {{33, 52}, {34, 52}, {34, 52}, {34, 50}};
    CHECK(level_ties_open(b, 4, 34, 3) == 1, "a level club with a match left can leave: open");

    /* A club on 50 with one match left ends on 50, 51 or 53: never on 52. */
    LevelRow a[] = {{33, 50}, {34, 52}, {34, 52}, {34, 48}};
    CHECK(level_ties_open(a, 4, 34, 3) == 0, "one match cannot give 2 points: settled");
    /* On 51 with one match left a draw gives 52. */
    LevelRow a2[] = {{33, 51}, {34, 52}, {34, 52}, {34, 48}};
    CHECK(level_ties_open(a2, 4, 34, 3) == 1, "a draw reaches 52: open");

    /* Every club finished: the order is final. */
    LevelRow c[] = {{34, 53}, {34, 52}, {34, 52}, {34, 50}};
    CHECK(level_ties_open(c, 4, 34, 3) == 0, "all clubs finished: settled");

    /* The clubs that still play cannot reach 52 (48 + 3) nor stand on it. */
    LevelRow d[] = {{34, 55}, {34, 52}, {34, 52}, {33, 48}};
    CHECK(level_ties_open(d, 4, 34, 3) == 0, "no club can reach the level points: settled");

    /* A club above the level clubs with a match left cannot come down to them. */
    LevelRow e[] = {{33, 55}, {34, 52}, {34, 52}, {34, 50}};
    CHECK(level_ties_open(e, 4, 34, 3) == 0, "a club above cannot fall to the level points: settled");

    /* No two finished clubs level: nothing to keep open (the original already treats a level club with matches
     * left as open). */
    LevelRow f[] = {{34, 52}, {33, 52}, {33, 51}, {34, 49}};
    CHECK(level_ties_open(f, 4, 34, 3) == 0, "one finished club on 52 only: settled");

    /* Two matches left: 46 + 6 reaches 52; 47 + 5 (no: 3 + 1 is 4, 3 + 3 is 6) does not; 48 + 4 (a win and a draw)
     * does. */
    LevelRow g[] = {{34, 60}, {34, 52}, {34, 52}, {32, 46}};
    CHECK(level_ties_open(g, 4, 34, 3) == 1, "a club two wins from the level points: open");
    LevelRow g2[] = {{34, 60}, {34, 52}, {34, 52}, {32, 47}};
    CHECK(level_ties_open(g2, 4, 34, 3) == 0, "5 points from two matches cannot be made: settled");
    LevelRow g3[] = {{34, 60}, {34, 52}, {34, 52}, {32, 48}};
    CHECK(level_ties_open(g3, 4, 34, 3) == 1, "a win and a draw reach the level points: open");

    /* A level pair at the bottom works the same way. */
    LevelRow h[] = {{34, 60}, {33, 30}, {34, 28}, {34, 28}};
    CHECK(level_ties_open(h, 4, 34, 3) == 0, "the club above (30) cannot fall to 28: settled");
    LevelRow i[] = {{34, 60}, {34, 30}, {34, 28}, {34, 28}, {33, 26}};
    CHECK(level_ties_open(i, 5, 34, 3) == 0, "a club below on 26 with a match left cannot make 2 points: settled");
    LevelRow i2[] = {{34, 60}, {34, 30}, {34, 28}, {34, 28}, {33, 25}};
    CHECK(level_ties_open(i2, 5, 34, 3) == 1, "a club below on 25 with a match left can win to 28: open");

    /* Two points a win (old rules) and no rows. */
    LevelRow j[] = {{34, 40}, {34, 40}, {33, 38}};
    CHECK(level_ties_open(j, 3, 34, 2) == 1, "two points a win: 38 + 2 reaches 40");
    LevelRow j2[] = {{34, 40}, {34, 40}, {33, 37}};
    CHECK(level_ties_open(j2, 3, 34, 2) == 0, "two points a win: 37 + 2 is 39, not 40");
    CHECK(level_ties_open(j, 0, 34, 3) == 0, "no rows: settled");
    CHECK(level_ties_open(0, 3, 34, 3) == 0, "no table: settled");

    if (failures) {
        printf("%d failures\n", failures);
        return 1;
    }
    printf("season_level_ties: all checks pass\n");
    return 0;
}
