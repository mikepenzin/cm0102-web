/* The fixture plans of the Argentine Primera (0.7.2 league sizes, slice 15, D9): season_arg_plan.c.
 *
 * Two zones of 15 (the 2025 and 2026 form): 16 rounds of 15 matches; each club meets each club of its zone once, its
 * rival of the other zone once (in the round where both rest in their zone) and its drawn opponent once (in the drawn
 * round); no club plays twice in a round. One table of 30 with a derby round (2015, 2016/17): 30 rounds; each pair
 * meets once, the derby pairs twice (once in the derby round). Odd zones of other sizes and other counts too. */
#include "season_sizes_arg.h"

#include <stdio.h>
#include <string.h>

static int fails;

#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

static int round[SEASON_ARG_MAX_ROUNDS][SEASON_ARG_MAX_CLUBS / 2][2];

/* Each round holds every club once; returns the meetings of each pair in meet[a][b] (a < b) and the home matches. */
static void count(int n, int rounds, int meet[][SEASON_ARG_MAX_CLUBS], int *home, const char *what)
{
    memset(meet, 0, sizeof(int) * SEASON_ARG_MAX_CLUBS * SEASON_ARG_MAX_CLUBS);
    memset(home, 0, sizeof(int) * SEASON_ARG_MAX_CLUBS);
    for (int r = 0; r < rounds; r++) {
        int seen[SEASON_ARG_MAX_CLUBS] = {0};
        for (int k = 0; k < n / 2; k++) {
            int h = round[r][k][0], a = round[r][k][1];
            CHECK(h >= 0 && h < n && a >= 0 && a < n && h != a, "%s: round %d match %d: %d v %d", what, r, k, h, a);
            if (h < 0 || h >= n || a < 0 || a >= n || h == a) continue;
            seen[h]++;
            seen[a]++;
            home[h]++;
            meet[h < a ? h : a][h < a ? a : h]++;
        }
        for (int c = 0; c < n; c++) CHECK(seen[c] == 1, "%s: round %d: club %d plays %d times", what, r, c, seen[c]);
    }
}

static void zones(int z, int draw_at, int shift)
{
    int rival[SEASON_ARG_MAX_CLUBS / 2], draw[SEASON_ARG_MAX_CLUBS / 2];
    for (int i = 0; i < z; i++) {
        rival[i] = (i * 7 + 3) % z;               /* any perfect matching */
        draw[i] = (rival[i] + shift) % z;
    }
    char what[64];
    snprintf(what, sizeof what, "zones of %d, drawn round %d", z, draw_at + 1);
    int rounds = season_arg_zone_plan(z, rival, draw, draw_at, round);
    CHECK(rounds == z + 1, "%s: %d rounds", what, rounds);
    if (rounds != z + 1) return;
    static int meet[SEASON_ARG_MAX_CLUBS][SEASON_ARG_MAX_CLUBS];
    int home[SEASON_ARG_MAX_CLUBS];
    count(2 * z, rounds, meet, home, what);
    for (int a = 0; a < 2 * z; a++)
        for (int b = a + 1; b < 2 * z; b++) {
            int same = (a < z) == (b < z), want = same;
            if (!same) {
                int x = a < z ? a : b, y = (a < z ? b : a) - z;
                want = (rival[x] == y) + (draw[x] == y);
            }
            CHECK(meet[a][b] == want, "%s: clubs %d and %d meet %d times, want %d", what, a, b, meet[a][b], want);
        }
    /* The drawn round holds only inter-zone matches; club i of zone A rests in its zone in round i (before the drawn
     * round) and plays its rival there. */
    for (int k = 0; k < z; k++) {
        int h = round[draw_at][k][0], a = round[draw_at][k][1];
        CHECK((h < z) != (a < z), "%s: drawn round match %d is in one zone", what, k);
    }
    for (int i = 0; i < z; i++) {
        int r = i < draw_at ? i : i + 1, found = 0;
        for (int k = 0; k < z; k++) {
            int h = round[r][k][0], a = round[r][k][1];
            found += (h == i && a == z + rival[i]) || (a == i && h == z + rival[i]);
        }
        CHECK(found == 1, "%s: club %d does not meet its rival in round %d", what, i, r + 1);
    }
    for (int c = 0; c < 2 * z; c++)
        CHECK(home[c] >= rounds / 2 - 2 && home[c] <= rounds / 2 + 2, "%s: club %d has %d home matches of %d", what, c,
              home[c], rounds);
}

static void derby(int n, int derby_at)
{
    int partner[SEASON_ARG_MAX_CLUBS];
    for (int i = 0; i < n; i += 2) partner[i] = i + 1, partner[i + 1] = i;
    char what[64];
    snprintf(what, sizeof what, "one table of %d, derby round %d", n, derby_at + 1);
    int rounds = season_arg_derby_plan(n, partner, derby_at, round);
    CHECK(rounds == n, "%s: %d rounds", what, rounds);
    if (rounds != n) return;
    static int meet[SEASON_ARG_MAX_CLUBS][SEASON_ARG_MAX_CLUBS];
    int home[SEASON_ARG_MAX_CLUBS];
    count(n, rounds, meet, home, what);
    for (int a = 0; a < n; a++)
        for (int b = a + 1; b < n; b++)
            CHECK(meet[a][b] == 1 + (partner[a] == b), "%s: clubs %d and %d meet %d times", what, a, b, meet[a][b]);
    for (int k = 0; k < n / 2; k++) {
        int h = round[derby_at][k][0], a = round[derby_at][k][1];
        CHECK(partner[h] == a, "%s: derby round match %d is %d v %d", what, k, h, a);
    }
    for (int c = 0; c < n; c++)
        CHECK(home[c] >= rounds / 2 - 2 && home[c] <= rounds / 2 + 2, "%s: club %d has %d home matches of %d", what, c,
              home[c], rounds);
}

/* The 2016 form (row 2015): each club meets its rival twice, in its rest round and in the derby round, once at home. */
static void rival_twice(int z, int draw_at)
{
    int rival[SEASON_ARG_MAX_CLUBS / 2];
    for (int i = 0; i < z; i++) rival[i] = (i * 2 + 1) % z;
    char what[64];
    snprintf(what, sizeof what, "zones of %d, the rival twice, round %d", z, draw_at + 1);
    int rounds = season_arg_zone_plan(z, rival, rival, draw_at, round);
    CHECK(rounds == z + 1, "%s: %d rounds", what, rounds);
    if (rounds != z + 1) return;
    static int meet[SEASON_ARG_MAX_CLUBS][SEASON_ARG_MAX_CLUBS];
    int home[SEASON_ARG_MAX_CLUBS];
    count(2 * z, rounds, meet, home, what);
    for (int i = 0; i < z; i++) {
        CHECK(meet[i][z + rival[i]] == 2, "%s: club %d meets its rival %d times", what, i, meet[i][z + rival[i]]);
        int at_home = 0;
        for (int r = 0; r < rounds; r++)
            for (int k = 0; k < z; k++) at_home += round[r][k][0] == i && round[r][k][1] == z + rival[i];
        CHECK(at_home == 1, "%s: club %d is at home to its rival %d times", what, i, at_home);
    }
}

/* The derby partners: the real pairs, then a club of no list takes the partner of the club it replaced. */
static void derby_pairs(void)
{
    /* 8 clubs. Real pairs 0-1, 2-3. Club 4's partner and club 6's partner are not in the division. Clubs 5 and 7 are of
     * no list (5 has the better reputation). */
    int listed[8] = {1, 0, 3, 2, -2, -1, -2, -1}, rep[8] = {50, 50, 50, 50, 40, 90, 70, 30}, derby[8];
    int made = season_arg_derby_pairs(8, listed, rep, derby);
    CHECK(made == 2, "derby pairs: %d by replacement, want 2", made);
    CHECK(derby[0] == 1 && derby[1] == 0 && derby[2] == 3 && derby[3] == 2, "derby pairs: a real pair changed");
    /* The orphans by reputation (6, then 4) take the clubs of no list by reputation (5, then 7). */
    CHECK(derby[6] == 5 && derby[5] == 6 && derby[4] == 7 && derby[7] == 4, "derby pairs: replacement %d %d %d %d",
          derby[4], derby[5], derby[6], derby[7]);
    /* A pair with both clubs gone: its two replacements and any left over meet by reputation. */
    int l2[6] = {1, 0, -1, -1, -1, -1}, r2[6] = {9, 9, 10, 40, 30, 20}, d2[6];
    made = season_arg_derby_pairs(6, l2, r2, d2);
    CHECK(made == 0, "derby pairs: %d by replacement with no orphan, want 0", made);
    CHECK(d2[3] == 4 && d2[4] == 3 && d2[5] == 2 && d2[2] == 5, "derby pairs: left over %d %d %d %d", d2[2], d2[3],
          d2[4], d2[5]);
    for (int i = 0; i < 6; i++) CHECK(d2[d2[i]] == i, "derby pairs: %d is not a pairing", i);
}

int main(void)
{
    derby_pairs();
    rival_twice(15, 7);
    rival_twice(15, 15);
    zones(15, 7, 1);        /* 2025: the drawn round is fecha 8 */
    zones(15, 5, 4);        /* 2026: fecha 6 */
    zones(15, 0, 2);
    zones(15, 15, 3);
    zones(13, 6, 5);
    zones(5, 2, 1);
    derby(30, 23);          /* 2015 and 2016/17: fecha 24 of 30 */
    derby(30, 0);
    derby(30, 29);
    derby(24, 11);
    derby(2, 0);
    /* Refusals: an even zone, a rival twice, a derby that is not a pairing. */
    int r2[4] = {0, 0, 1, 2}, d2[4] = {1, 2, 3, 0}, p3[4] = {1, 0, 3, 3};
    CHECK(season_arg_zone_plan(4, r2, d2, 0, round) == 0, "an even zone gives a plan");
    CHECK(season_arg_zone_plan(3, r2, d2, 0, round) == 0, "a rival twice gives a plan");
    CHECK(season_arg_derby_plan(4, p3, 0, round) == 0, "a derby that is not a pairing gives a plan");
    int r5[5] = {0, 1, 2, 3, 4}, d5[5] = {0, 2, 3, 4, 1};     /* club 0 meets its rival twice, the others not */
    CHECK(season_arg_zone_plan(5, r5, d5, 0, round) == 0, "one drawn opponent that is the rival gives a plan");
    /* Item 286: the clubs that the rows send down. Row: {by the averages, by the annual table}. */
    static const int rows[][3] = {{2001, 2, 0}, {2011, 2, 0}, {2012, 3, 0}, {2013, 3, 0}, {2014, 2, 0}, {2015, 1, 0},
                                  {2016, 4, 0}, {2017, 4, 0}, {2018, 4, 0}, {2019, 0, 0}, {2020, 0, 0}, {2021, 2, 0},
                                  {2022, 1, 1}, {2023, 0, 0}, {2024, 1, 1}, {2027, 1, 1}};
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        int avg = -1, tab = -1;
        season_arg_down_places(rows[i][0], &avg, &tab);
        CHECK(avg == rows[i][1] && tab == rows[i][2], "the relegation places of a row");
    }
    /* Item 286: the second division plays the row's places with changing rules on (2017/18: 6), and the 4 of the swap
     * 0x4068d0 (cmp bl, 4 at 0x4069fb) with changing rules off. */
    CHECK(season_arg_second_down_places(6, 1) == 6, "6 places with changing rules on");
    CHECK(season_arg_second_down_places(6, 0) == 4, "the swap's 4 places with changing rules off");
    CHECK(season_arg_second_down_places(3, 0) == 3, "a row under the swap's 4 keeps its places");
    CHECK(season_arg_second_down_places(-1, 1) == 0, "no places from an unknown row");
    /* Item 291: the class's 2001/02 form sends 2 down by the averages (0x408666) and the step the surplus to the next row. */
    CHECK(season_arg_second_class_down(25, 20) == 7, "2001/02: 2 and the 5 that the row of 20 leaves over");
    CHECK(season_arg_second_class_down(25, 25) == 2, "the class alone sends 2 down");
    CHECK(season_arg_second_class_down(25, -1) == 2, "no next row: the class's 2");
    CHECK(season_arg_second_class_down(20, 25) == 2, "a bigger next row sends no more down");
    /* Item 291: the Torneo Reducido of each row, and the pairs of a round. */
    SeasonArgReducido rd;
    CHECK(!season_arg_reducido_of(2016, &rd), "no Reducido in row 2016");
    CHECK(season_arg_reducido_of(2014, &rd) && rd.places == 4 && rd.rounds == 2 && rd.stages == 3 && rd.final_pens,
          "row 2014: 4 clubs, semi-finals and a final");
    CHECK(rd.round[0].fresh == 4 && rd.round[0].ties == 2 && rd.round[0].legs == 2, "row 2014: two legs");
    CHECK(season_arg_reducido_of(2017, &rd) && rd.places == 8 && rd.stages == 7 && rd.round[0].legs == 1 &&
          rd.round[1].legs == 2 && rd.round[2].legs == 2, "row 2017: one-match quarter-finals");
    CHECK(season_arg_reducido_of(2018, &rd) && rd.places == 8 && rd.round[0].legs == 2, "row 2018: two legs");
    CHECK(season_arg_reducido_of(2021, &rd) && rd.places == 12 && rd.rounds == 4 && rd.stages == 11 && !rd.final_pens,
          "row 2021: 12 clubs in four rounds");
    CHECK(rd.round[0].fresh == 10 && rd.round[0].ties == 5 && rd.round[1].fresh == 1 && rd.round[1].ties == 3 &&
          rd.round[2].fresh == 1 && rd.round[2].ties == 2 && rd.round[3].fresh == 0 && rd.round[3].ties == 1,
          "row 2021: 4th to 13th, then the 3rd, then the 2nd");
    for (int row = 2014; row <= 2021; row++) {
        if (!season_arg_reducido_of(row, &rd) || rd.zone_last) continue;
        int in = 0, left = 0;
        for (int r = 0; r < rd.rounds; r++) {
            in += rd.round[r].fresh;
            CHECK(left + rd.round[r].fresh == 2 * rd.round[r].ties, "every club of a round plays");
            left = rd.round[r].ties;
        }
        CHECK(in == rd.places && left == 1, "every place enters and one club wins");
    }
    int seeds[6] = {0, 2, 3, 6, 8, 11}, ties[6];
    season_arg_reducido_pairs(seeds, 6, ties);
    CHECK(ties[0] == 0 && ties[1] == 11 && ties[2] == 2 && ties[3] == 8 && ties[4] == 3 && ties[5] == 6,
          "the best place meets the worst");
    /* Item 291: the zone rows. */
    CHECK(season_arg_reducido_of(2020, &rd) && rd.zone_last == 4 && rd.places == 6 && rd.rounds == 3 && rd.loser_round == 1 &&
          rd.final_one && rd.level_pens && rd.down_each == 0 && !rd.rel_tie && rd.stages == 7,
          "row 2020: 2nd to 4th of each zone, the final's loser in the semi-finals, no relegation");
    CHECK(season_arg_reducido_of(2019, &rd) && rd.zone_last == 4 && rd.down_each == 1, "row 2019: the last of each zone");
    CHECK(season_arg_reducido_of(2022, &rd) && rd.zone_last == 8 && rd.places == 14 && rd.rounds == 4 && rd.loser_round == 1 &&
          rd.final_one && !rd.level_pens && rd.down_each == 1 && !rd.rel_tie && rd.stages == 15, "row 2022");
    CHECK(rd.round[0].fresh == 14 && rd.round[0].ties == 7 && rd.round[0].legs == 1 && rd.round[1].ties == 4 &&
          rd.round[1].legs == 2 && rd.round[3].legs == 1, "row 2022: one match, two legs, and a final of one match");
    CHECK(season_arg_reducido_of(2023, &rd) && rd.rel_tie && rd.down_each == 1 && rd.stages == 16, "row 2023: a relegation tie");
    CHECK(season_arg_reducido_of(2024, &rd) && rd.down_each == 2 && !rd.rel_tie, "row 2024: the last two of each zone");
    CHECK(!rd.final_one && rd.round[3].legs == 2 && rd.final_pens, "row 2024: a final of two legs");
    CHECK(season_arg_reducido_of(2025, &rd) && rd.down_each == 2, "row 2025: the last two of each zone");
    for (int row = 2019; row <= 2025; row++) {
        if (!season_arg_reducido_of(row, &rd) || !rd.zone_last) continue;
        int left = 0;
        for (int r = 0; r < rd.rounds; r++) {
            CHECK(left + rd.round[r].fresh + (r == rd.loser_round) == 2 * rd.round[r].ties, "a zone row's round is full");
            left = rd.round[r].ties;
        }
        CHECK(left == 1 && rd.places == 2 * (rd.zone_last - 1), "a zone row ends in one winner");
    }
    int zone[37], in_a = 0;
    season_arg_second_zone_split(37, 19, zone);
    for (int i = 0; i < 37; i++) in_a += zone[i] == 0;
    CHECK(in_a == 19, "19 clubs in zone A");
    static int zr[SEASON_ARG_ZONE_ROUNDS][SEASON_ARG_ZONE_TIES][2];
    static const int sizes[][3] = {{16, 16, 30}, {17, 18, 34}, {19, 18, 38}, {19, 19, 38}, {18, 18, 34}};
    for (unsigned i = 0; i < sizeof sizes / sizeof sizes[0]; i++) {
        int a = sizes[i][0], b = sizes[i][1], n = a + b, rounds = season_arg_second_zone_plan(a, b, zr);
        CHECK(rounds == sizes[i][2], "the rounds of two zones");
        static int met[40][40];
        memset(met, 0, sizeof met);
        for (int r = 0; r < rounds; r++) {
            char seen[40] = {0};
            for (int k = 0; k < n / 2; k++) {
                int h = zr[r][k][0], w = zr[r][k][1];
                if (h < 0) continue;
                CHECK(!seen[h] && !seen[w], "a club plays once a round");
                seen[h] = seen[w] = 1;
                CHECK((h < a) == (w < a), "a match inside one zone");
                met[h][w]++;
            }
        }
        for (int h = 0; h < n; h++)
            for (int w = 0; w < n; w++)
                CHECK(met[h][w] == (h != w && (h < a) == (w < a)), "each pair of a zone meets home and away");
    }
    if (fails) {
        printf("%d checks failed\n", fails);
        return 1;
    }
    printf("arg_prm plans: all checks passed\n");
    return 0;
}
