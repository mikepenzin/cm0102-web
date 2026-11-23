/* The size step at a season change (0.7.2 league sizes, slice 3, D13): season_size_step.c.
 *
 * Each case builds divisions and clubs after the promotion chain: the rows of a division are its last table, the
 * best place first; a club's division is where the chain left it. */
#include "season_size_step.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

static StepClub club[200];
static int nclub;
static StepDivision div[16];
static StepMove mv[200];

static void reset(void)
{
    memset(club, 0, sizeof club);
    memset(div, 0, sizeof div);
    nclub = 0;
}

/* A division of n clubs at the places 1..n, all still in it, of nation 1. Returns the index of its first club. */
static int add_division(int d, int want, int n)
{
    div[d].want = want;
    div[d].nation = 1;
    int first = nclub;
    for (int i = 0; i < n; i++) {
        club[nclub] = (StepClub){d, 1, 100, 1, 1};
        div[d].rows[div[d].nrows++] = nclub++;
    }
    return first;
}

static int add_free(int nation, int rep, int up_ok)
{
    club[nclub] = (StepClub){STEP_NONE, nation, rep, up_ok, 1};
    return nclub++;
}

static void below(int upper, int lower)
{
    div[upper].lower[div[upper].nlower++] = lower;
}

static int count_in(int d)
{
    int n = 0;
    for (int i = 0; i < nclub; i++) n += club[i].div == d;
    return n;
}

static int moved(int n, int c, int from, int to)
{
    for (int i = 0; i < n; i++)
        if (mv[i].club == c && mv[i].from == from && mv[i].to == to) return 1;
    return 0;
}

/* Serie A from 18 to 20 (2004/05): 2 down and 2 up by the places, then A takes the best two of B that stayed. B (the
 * lowest division here) then takes two clubs with no division of its nation, the best reputation first, and no
 * reserve side and no club of another nation. */
static void grow(void)
{
    reset();
    int a = add_division(0, 20, 18), b = add_division(1, 22, 22);
    below(0, 1);
    club[a + 16].div = club[a + 17].div = 1;     /* relegated */
    club[b + 0].div = club[b + 1].div = 0;       /* promoted */
    int r = add_free(1, 900, 0), f1 = add_free(1, 500, 1), f2 = add_free(1, 400, 1), f3 = add_free(1, 300, 1);
    int other = add_free(2, 999, 1);
    int n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(count_in(0) == 20, "grow: A holds %d", count_in(0));
    CHECK(count_in(1) == 22, "grow: B holds %d", count_in(1));
    CHECK(moved(n, b + 2, 1, 0) && moved(n, b + 3, 1, 0), "grow: A takes B's 3rd and 4th");
    CHECK(moved(n, f1, STEP_NONE, 1) && moved(n, f2, STEP_NONE, 1), "grow: B takes the two best free clubs");
    CHECK(club[r].div == STEP_NONE && club[f3].div == STEP_NONE && club[other].div == STEP_NONE,
          "grow: no reserve side, third free club or club of another nation");
    CHECK(n == 4, "grow: %d moves", n);
    CHECK(div[0].short_by == 0 && div[1].short_by == 0, "grow: short %d %d", div[0].short_by, div[1].short_by);
}

/* Ligue 1 from 20 to 18: the places moved 2 and 2 in pairs, so A sends its worst two that stayed (17th and 18th:
 * 19th and 20th went down already) to B. B, the lowest, has 22 for a row of 20 and sends its worst two that stayed
 * to no division, but not a club with a human manager. */
static void shrink(void)
{
    reset();
    int a = add_division(0, 18, 20), b = add_division(1, 20, 20);
    below(0, 1);
    club[a + 18].div = club[a + 19].div = 1;
    club[b + 0].div = club[b + 1].div = 0;
    club[b + 19].down_ok = 0;
    int n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(count_in(0) == 18 && count_in(1) == 20, "shrink: %d and %d", count_in(0), count_in(1));
    CHECK(moved(n, a + 17, 0, 1) && moved(n, a + 16, 0, 1), "shrink: A's 17th and 18th go down");
    CHECK(moved(n, b + 18, 1, STEP_NONE) && moved(n, b + 17, 1, STEP_NONE), "shrink: B's 19th and 18th leave");
    CHECK(club[b + 19].div == 1, "shrink: the human club stays");
    CHECK(n == 4, "shrink: %d moves", n);

    /* A club with a human manager in a division above the lowest is not sent down either: the next worst goes. */
    reset();
    a = add_division(0, 18, 20);
    add_division(1, 20, 20);
    below(0, 1);
    club[a + 19].down_ok = 0;
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(club[a + 19].div == 0 && moved(n, a + 18, 0, 1) && moved(n, a + 17, 0, 1), "shrink: the human club stays up");
}

/* A division below that made its new table, or that takes no part, gives no club: A stays short and says so. */
static void started(void)
{
    reset();
    add_division(0, 20, 18);
    add_division(1, 22, 22);
    below(0, 1);
    div[1].started = 1;
    int n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(n == 0 && div[0].short_by == 2, "started: %d moves, short %d", n, div[0].short_by);
    div[1].started = 0;
    div[1].want = -1;
    div[0].short_by = 0;
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(n == 0 && div[0].short_by == 2, "no part: %d moves, short %d", n, div[0].short_by);
    CHECK(count_in(1) == 22, "no part: B holds %d", count_in(1));
}

/* Two divisions below (Serie B over the two C1 groups): a short division takes by place, first of the first group
 * then of the second; a reserve side of the first is passed over. A division with too many sends a club to the
 * group below with the most need. */
static void two_below(void)
{
    reset();
    add_division(0, 22, 20);
    int c1 = add_division(1, 18, 18), c2 = add_division(2, 18, 18);
    below(0, 1);
    below(0, 2);
    club[c1].up_ok = 0;
    int n = season_size_step_plan(div, 3, club, nclub, mv, 200);
    CHECK(moved(n, c1 + 1, 1, 0) && moved(n, c2, 2, 0), "two below: takes the 2nd of C1a and the 1st of C1b");
    CHECK(count_in(0) == 22, "two below: B holds %d", count_in(0));
    CHECK(div[1].short_by == 1 && div[2].short_by == 1, "two below: the lowest have no free club: %d %d",
          div[1].short_by, div[2].short_by);

    reset();
    int b = add_division(0, 20, 22);
    add_division(1, 18, 18);
    add_division(2, 18, 17);
    below(0, 1);
    below(0, 2);
    div[1].want = div[2].want = -2;     /* only the destinations matter here: they take no part */
    n = season_size_step_plan(div, 3, club, nclub, mv, 200);
    CHECK(n == 0 && div[0].short_by == -2, "two below, no part: %d moves, short %d", n, div[0].short_by);
    div[1].want = 18;
    div[2].want = 18;
    div[0].short_by = 0;
    n = season_size_step_plan(div, 3, club, nclub, mv, 200);
    CHECK(moved(n, b + 21, 0, 2), "two below: the worst goes to the group with the most need");
    CHECK(count_in(0) == 20, "two below: B holds %d", count_in(0));
}

/* A division at its row moves nothing. */
static void steady(void)
{
    reset();
    add_division(0, 18, 18);
    add_division(1, 20, 20);
    below(0, 1);
    add_free(1, 500, 1);
    CHECK(season_size_step_plan(div, 2, club, nclub, mv, 200) == 0, "steady: moves");
}

/* A club that joined a division outside the chain (the Conference club of League Two) is not in its last table, but
 * it counts: League Two at 24 takes nobody. Two lowest divisions of one nation share the clubs with no division. */
static void outsiders_and_pool(void)
{
    reset();
    add_division(0, 24, 23);
    club[nclub++] = (StepClub){0, 1, 100, 1, 1};     /* joined from the Conference */
    int n = season_size_step_plan(div, 1, club, nclub, mv, 200);
    CHECK(n == 0 && count_in(0) == 24, "outsider: %d moves, %d clubs", n, count_in(0));

    reset();
    add_division(0, 12, 10);
    add_division(1, 12, 11);
    int f1 = add_free(1, 900, 1), f2 = add_free(1, 800, 1);
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(moved(n, f1, STEP_NONE, 0) && moved(n, f2, STEP_NONE, 0), "pool: the first lowest takes the best two");
    CHECK(div[0].short_by == 0 && div[1].short_by == 1, "pool: short %d %d", div[0].short_by, div[1].short_by);
}

/* Gap row 8: the lowest division of the size table over a division outside it (want: the count that it holds). A
 * surplus goes down into that division, which sends as many of its own worst clubs to no division; a lack takes its
 * best clubs, which it replaces with clubs of its nation with no division. */
static void outside(void)
{
    reset();
    int u = add_division(0, 22, 24), l = add_division(1, 20, 20);
    below(0, 1);
    int n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(moved(n, u + 23, 0, 1) && moved(n, u + 22, 0, 1), "outside: the worst two go down");
    CHECK(moved(n, l + 19, 1, STEP_NONE) && moved(n, l + 18, 1, STEP_NONE), "outside: the division below sends two out");
    CHECK(count_in(0) == 22 && count_in(1) == 20 && n == 4, "outside: %d and %d, %d moves", count_in(0), count_in(1), n);

    reset();
    u = add_division(0, 24, 22);
    l = add_division(1, 20, 20);
    below(0, 1);
    int f1 = add_free(1, 700, 1), f2 = add_free(1, 600, 1);
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(moved(n, l, 1, 0) && moved(n, l + 1, 1, 0), "outside: the best two of the division below go up");
    CHECK(moved(n, f1, STEP_NONE, 1) && moved(n, f2, STEP_NONE, 1), "outside: it takes two clubs with no division");
    CHECK(count_in(0) == 24 && count_in(1) == 20 && div[1].short_by == 0, "outside: %d and %d", count_in(0),
          count_in(1));

    /* Eight groups below (the Serie C under the Serie B): each takes one of a surplus of 8. */
    reset();
    add_division(0, 20, 28);
    for (int g = 1; g <= STEP_MAX_LOWER; g++) {
        add_division(g, 4, 4);
        below(0, g);
    }
    n = season_size_step_plan(div, 1 + STEP_MAX_LOWER, club, nclub, mv, 200);
    int each = 1;
    for (int g = 1; g <= STEP_MAX_LOWER; g++) each &= count_in(g) == 4;
    CHECK(count_in(0) == 20 && each && n == 16, "outside groups: %d clubs, %d moves", count_in(0), n);
}

/* The plan stops at max moves and says how many it lacks. */
static void truncated(void)
{
    reset();
    add_division(0, 20, 16);
    add_division(1, 20, 20);
    below(0, 1);
    int n = season_size_step_plan(div, 2, club, nclub, mv, 3);
    CHECK(n == 3 && count_in(0) == 19 && div[0].short_by == 1, "max: %d moves, %d clubs, short %d", n, count_in(0),
          div[0].short_by);
}

static int replaced(int n, int c, int to, int feeder)
{
    for (int i = 0; i < n; i++)
        if (mv[i].club == c && mv[i].from == STEP_NONE && mv[i].to == to && mv[i].replaces == feeder) return 1;
    return 0;
}

/* The Veikkausliiga from 12 to 14 (2003; gap 37 of 0.7.2): its division below plays as now (D16), so it is a lowest
 * division of the step with two feeders, the Ykkonen groups (no row: want -1). After the chain (one down, one up)
 * it takes the best placed clubs of the groups that stayed, the places in turn (place 1 of each group, then place
 * 2), and the club with no division of the best reputation takes each one's place in its group. With no club of
 * no division for the group it takes nobody from it. */
static void feeders(void)
{
    reset();
    int a = add_division(0, 14, 12), g1 = add_division(1, -1, 10), g2 = add_division(2, -1, 10);
    div[0].feed[div[0].nfeed++] = 1;
    div[0].feed[div[0].nfeed++] = 2;
    club[a + 11].div = 1;                          /* relegated to group 1 */
    club[g1 + 0].div = 0;                          /* group 1's winner promoted */
    int f1 = add_free(1, 900, 1), f2 = add_free(1, 800, 1), f3 = add_free(1, 700, 1);
    int n = season_size_step_plan(div, 3, club, nclub, mv, 200);
    CHECK(count_in(0) == 14 && count_in(1) == 10 && count_in(2) == 10, "feeders: %d %d %d", count_in(0),
          count_in(1), count_in(2));
    CHECK(moved(n, g2 + 0, 2, 0) && moved(n, g1 + 1, 1, 0), "feeders: group 2's 1st and group 1's 2nd go up");
    CHECK(replaced(n, f1, 2, g2 + 0) && replaced(n, f2, 1, g1 + 1), "feeders: the best of no division take their places");
    CHECK(club[f3].div == STEP_NONE && div[0].short_by == 0, "feeders: f3 stays, short %d", div[0].short_by);

    /* No club with no division: the groups keep their clubs, the division lacks two. */
    reset();
    add_division(0, 14, 12);
    add_division(1, -1, 10);
    div[0].feed[div[0].nfeed++] = 1;
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(n == 0 && count_in(1) == 10 && div[0].short_by == 2, "feeders none: %d moves, short %d", n, div[0].short_by);

    /* No feeder club may go up (up_ok 0): a club with no division never joins the division straight, as its entry
     * check was made against the feeder (0.7.2 sz10 review). */
    reset();
    add_division(0, 13, 12);
    int h = add_division(1, -1, 10);
    div[0].feed[div[0].nfeed++] = 1;
    for (int k = 0; k < 10; k++) club[h + k].up_ok = 0;
    int f = add_free(1, 900, 1);
    n = season_size_step_plan(div, 2, club, nclub, mv, 200);
    CHECK(club[f].div == STEP_NONE && div[0].short_by == 1, "feeders barred: free club in %d, short %d", club[f].div,
          div[0].short_by);
}

/* The Veikkausliiga from 14 to 13 (2006): AC Allianssi lost its licence. A club marked to leave goes to no division
 * before the worst placed club; with no such club the worst placed goes. */
static void leavers(void)
{
    reset();
    int a = add_division(0, 13, 14);
    club[a + 4].leave = 1;
    int n = season_size_step_plan(div, 1, club, nclub, mv, 200);
    CHECK(n == 1 && moved(n, a + 4, 0, STEP_NONE), "leave: the 5th leaves, %d moves", n);
    CHECK(club[a + 13].div == 0, "leave: the 14th stays");
}

/* Gap 47: a division with no chain sends down only as many clubs as its no-division pool can replace. */
static void lone_guard(void)
{
    CHECK(season_size_lone_down(1, 5, 12, 12) == 1, "guard: 1 down, pool 5, 12 to 12");
    CHECK(season_size_lone_down(1, 0, 12, 12) == 0, "guard: 1 down, empty pool, 12 to 12");
    CHECK(season_size_lone_down(2, 0, 14, 12) == 2, "guard: 2 down, empty pool, 14 to 12 (no refill)");
    CHECK(season_size_lone_down(3, 0, 14, 12) == 2, "guard: 3 down, empty pool, 14 to 12");
    CHECK(season_size_lone_down(1, 2, 12, 14) == 0, "guard: 1 down, pool 2, 12 to 14 (the pool fills the growth)");
    CHECK(season_size_lone_down(1, 3, 12, 14) == 1, "guard: 1 down, pool 3, 12 to 14");
    CHECK(season_size_lone_down(2, 1, 12, -1) == 0, "guard: 2 down, pool 1, no row (none goes, the count stays)");
    CHECK(season_size_lone_down(0, 9, 12, 12) == 0 && season_size_lone_down(-2, 9, 12, 12) == 0,
          "guard: no places");
    CHECK(season_size_lone_down(6, 9, 18, 12) == 6, "guard: Wales 2009, 18 to 12");
}

/* Lead decision of 15 November: a club with a human manager never goes to no division; the next worst club with no
 * human manager goes in its place. Rows are the best place first. */
static void lone_human(void)
{
    int out[16], kept[16], nk = -1;
    int none[12] = {0};
    int n = season_size_lone_pick(none, 12, 2, out, kept, &nk);
    CHECK(n == 2 && out[0] == 11 && out[1] == 10 && nk == 0, "pick: no human, %d picks %d %d, %d kept", n, out[0],
          out[1], nk);
    int worst[12] = {0};
    worst[11] = 1;
    n = season_size_lone_pick(worst, 12, 2, out, kept, &nk);
    CHECK(n == 2 && out[0] == 10 && out[1] == 9 && nk == 1 && kept[0] == 11, "pick: human last, %d picks %d %d, "
          "%d kept %d", n, out[0], out[1], nk, kept[0]);
    int above[12] = {0};
    above[10] = 1;
    n = season_size_lone_pick(above, 12, 1, out, kept, &nk);
    CHECK(n == 1 && out[0] == 11 && nk == 0, "pick: human above the places, %d picks %d, %d kept", n, out[0], nk);
    int two[12] = {0};
    two[11] = two[10] = 1;
    n = season_size_lone_pick(two, 12, 1, out, kept, &nk);
    CHECK(n == 1 && out[0] == 9 && nk == 1 && kept[0] == 11, "pick: two humans last, 1 place, %d picks %d, %d kept",
          n, out[0], nk);
    int gap[12] = {0};
    gap[11] = gap[9] = 1;
    n = season_size_lone_pick(gap, 12, 2, out, kept, &nk);
    CHECK(n == 2 && out[0] == 10 && out[1] == 8 && nk == 1 && kept[0] == 11, "pick: humans 12th and 10th, 2 places, "
          "%d picks %d %d, %d kept", n, out[0], out[1], nk);
    int all[3] = {1, 1, 1};
    n = season_size_lone_pick(all, 3, 1, out, kept, &nk);
    CHECK(n == 0 && nk == 1 && kept[0] == 2, "pick: only humans (the one in the place stays), %d picks, %d kept", n, nk);
    n = season_size_lone_pick(none, 12, 0, out, kept, &nk);
    CHECK(n == 0 && nk == 0, "pick: no places, %d picks", n);
}

int main(void)
{
    feeders();
    leavers();
    lone_human();
    lone_guard();
    outsiders_and_pool();
    truncated();
    grow();
    shrink();
    started();
    two_below();
    steady();
    outside();
    printf("%s season_size_step (%d failures)\n", fails ? "FAIL" : "PASS", fails);
    return fails != 0;
}
