/* ctest season_uefa_league (0.6.0 slice 25): the league phase draw of season_uefa_league_draw.c. For many
 * seeds and nation mixes, with 8 matches (4 pots of 9) and 6 matches (6 pots of 6): each club has its matches,
 * half at home, its opponents by pot (8: two of each pot, one at home; 6: one of each pot, one at home of each
 * pot pair), no opponent twice, each club once on each match day, and the nation rules when the draw keeps them.
 * Then the knockout bracket: each place 9 to 24 and 1 to 8 once, in its pairs. */
#include "season_uefa_league.h"

#include <stdio.h>
#include <string.h>

static int fails;
#define CHECK(c, ...)                                                                                   \
    do {                                                                                                \
        if (!(c)) {                                                                                     \
            if (fails < 20) printf("FAIL %s:%d: ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n"); \
            fails++;                                                                                    \
        }                                                                                               \
    } while (0)

static int lcg(int n, void *ctx)
{
    unsigned *s = ctx;
    *s = *s * 1103515245u + 12345u;
    return (int)((*s >> 8) % (unsigned)n);
}

static int stop_after(int n, void *ctx)
{
    int *left = ctx;
    return --*left < 0 ? -1 : 0 % n;
}

/* Nation mixes: counts of the largest nations, the rest one club each. */
static const int MIXES[][8] = {
    {5, 5, 4, 4, 4, 3, 3, 2},       /* a Champions League of 2024/25 */
    {3, 3, 3, 2, 2, 2, 2, 2},       /* a Europa League */
    {2, 2, 2, 2, 1, 1, 1, 1},       /* a Conference League */
    {6, 5, 5, 4, 3, 0, 0, 0},       /* more clubs of one nation than the rules can hold well */
};

static void check_draw(const SeasonLeagueDraw *d, const int *nation, int days, const char *what)
{
    int s = days == 8 ? 9 : 6;
    int seen[SEASON_LEAGUE_CLUBS][SEASON_LEAGUE_CLUBS];
    int games[SEASON_LEAGUE_CLUBS] = {0}, homes[SEASON_LEAGUE_CLUBS] = {0};
    int by_pot[SEASON_LEAGUE_CLUBS][6], home_pot[SEASON_LEAGUE_CLUBS][6];
    memset(seen, 0, sizeof seen);
    memset(by_pot, 0, sizeof by_pot);
    memset(home_pot, 0, sizeof home_pot);
    for (int day = 0; day < days; day++) {
        int on[SEASON_LEAGUE_CLUBS] = {0};
        for (int m = 0; m < SEASON_LEAGUE_DAY_MATCHES; m++) {
            int h = d->match[day][m].home, a = d->match[day][m].away;
            CHECK(h >= 0 && h < 36 && a >= 0 && a < 36 && h != a, "%s: bad match %d v %d", what, h, a);
            if (h < 0 || h >= 36 || a < 0 || a >= 36) continue;
            CHECK(!on[h] && !on[a], "%s: a club twice on day %d", what, day);
            on[h] = on[a] = 1;
            CHECK(!seen[h][a] && !seen[a][h], "%s: %d and %d meet twice", what, h, a);
            seen[h][a] = 1;
            games[h]++, games[a]++, homes[h]++;
            by_pot[h][a / s]++, by_pot[a][h / s]++;
            home_pot[h][a / s]++;
        }
    }
    for (int c = 0; c < 36; c++) {
        CHECK(games[c] == days && homes[c] == days / 2, "%s: club %d has %d games, %d at home", what, c, games[c],
              homes[c]);
        for (int p = 0; p < 36 / s; p++) {
            if (days == 8) {
                CHECK(by_pot[c][p] == 2 && home_pot[c][p] == 1, "%s: club %d meets pot %d %d times, %d at home",
                      what, c, p, by_pot[c][p], home_pot[c][p]);
            } else {
                CHECK(by_pot[c][p] == 1, "%s: club %d meets pot %d %d times", what, c, p, by_pot[c][p]);
                if (p % 2 == 0)
                    CHECK(home_pot[c][p] + home_pot[c][p + 1] == 1, "%s: club %d at home %d times in pots %d, %d",
                          what, c, home_pot[c][p] + home_pot[c][p + 1], p, p + 1);
            }
        }
        if (d->relax < 2)
            for (int o = 0; o < 36; o++)
                CHECK(!(seen[c][o] || seen[o][c]) || nation[c] != nation[o], "%s: %d and %d of one nation meet", what,
                      c, o);
        if (d->relax < 1)
            for (int n = 0; n < 36; n++) {
                int k = 0;
                for (int o = 0; o < 36; o++) k += (seen[c][o] || seen[o][c]) && nation[o] == n;
                CHECK(k <= 2, "%s: club %d meets %d clubs of nation %d", what, c, k, n);
            }
    }
}

/* Decision L8 of the 0.7.3 spec: the pots of season_league_pots keep both nation rules in every draw. The clubs of
 * the largest nations stand together at the top of the seed order, the worst case for one pot. A nation of 7 is the
 * most that 8 matches can hold: k clubs play 8k matches against the 36 - k others, who each meet at most two. */
static void check_pots(void)
{
    static const int CLUSTER[][6] = {{7, 0, 0, 0, 0, 0}, {7, 6, 0, 0, 0, 0}, {6, 5, 5, 4, 0, 0}, {5, 5, 4, 4, 4, 3}};
    for (int days = 8; days >= 6; days -= 2) {
        int s = days == 8 ? 9 : 6;
        for (int c = 0; c < 4; c++)
            for (unsigned seed = 1; seed <= 25; seed++) {
                int seeds[36], n = 0, id = 0, order[36], nation[36], used[36] = {0};
                for (int k = 0; k < 6; k++, id++)
                    for (int j = 0; j < CLUSTER[c][k]; j++) seeds[n++] = id;
                while (n < 36) seeds[n++] = id++;
                season_league_pots(days, seeds, order);
                for (int i = 0; i < 36; i++) {
                    CHECK(order[i] >= 0 && order[i] < 36 && !used[order[i]], "days %d cluster %d: order is no permutation",
                          days, c);
                    if (order[i] >= 0 && order[i] < 36) used[order[i]] = 1, nation[i] = seeds[order[i]];
                }
                for (int p = 0; p < 36 / s; p++)
                    for (int x = 0; x < 36; x++) {
                        int k = 0, all = 0;
                        for (int i = 0; i < 36; i++) all += nation[i] == x;
                        for (int i = p * s; i < (p + 1) * s; i++) k += nation[i] == x;
                        int limit = days == 8 ? (9 - all < 4 ? 9 - all : 4) : (6 - (all + 1) / 2 < 3 ? 6 - (all + 1) / 2 : 3);
                        CHECK(!all || k <= limit, "days %d cluster %d: pot %d holds %d clubs of nation %d", days, c, p, k, x);
                    }
                SeasonLeagueDraw d;
                unsigned r = seed;
                CHECK(season_league_draw(days, nation, lcg, &r, &d), "days %d cluster %d seed %u: no draw", days, c, seed);
                CHECK(d.relax == 0, "days %d cluster %d seed %u: draw rules %d", days, c, seed, d.relax);
                check_draw(&d, nation, days, "pots");
            }
        /* Pots with no surplus stay in the game's seed order. */
        int plain[36], order[36];
        for (int i = 0; i < 36; i++) plain[i] = i / 2;
        season_league_pots(days, plain, order);
        for (int i = 0; i < 36; i++) CHECK(order[i] == i, "days %d: club %d moved with no surplus", days, i);
    }
}

/* Decision L8: no club of a lower pot has a higher reputation than a club of the pot above, and clubs of equal
 * reputation keep the seed order they come in. */
static void check_reputation(void)
{
    for (unsigned seed = 1; seed <= 200; seed++) {
        int rep[36], order[36], used[36] = {0};
        unsigned r = seed;
        for (int i = 0; i < 36; i++) rep[i] = 1000 + 500 * lcg(17, &r);
        season_league_by_reputation(rep, order);
        for (int i = 0; i < 36; i++) {
            CHECK(order[i] >= 0 && order[i] < 36 && !used[order[i]], "seed %u: order is no permutation", seed);
            if (order[i] >= 0 && order[i] < 36) used[order[i]] = 1;
        }
        for (int s = 9; s >= 6; s -= 3)
            for (int i = 0; i < 36; i++)
                for (int j = (i / s + 1) * s; j < 36; j++)
                    CHECK(rep[order[i]] >= rep[order[j]], "seed %u: pot %d holds %d, a lower pot %d", seed, i / s,
                          rep[order[i]], rep[order[j]]);
        for (int i = 0; i < 35; i++)
            CHECK(rep[order[i]] != rep[order[i + 1]] || order[i] < order[i + 1], "seed %u: a tie left the seed order", seed);
    }
}

int main(void)
{
    check_reputation();
    check_pots();
    int relaxed[2][3] = {{0}};
    for (int days = 8; days >= 6; days -= 2)
        for (int mix = 0; mix < 4; mix++)
            for (unsigned seed = 1; seed <= 60; seed++) {
                int nation[36], n = 0, id = 0;
                for (int k = 0; k < 8; k++, id++)
                    for (int j = 0; j < MIXES[mix][k]; j++) nation[n++] = id;
                while (n < 36) nation[n++] = id++;
                /* Shuffle the clubs into the pots, as seeds would. */
                unsigned s = seed * 7919u;
                for (int i = 35; i > 0; i--) {
                    int j = lcg(i + 1, &s), t = nation[i];
                    nation[i] = nation[j], nation[j] = t;
                }
                SeasonLeagueDraw d;
                char what[64];
                snprintf(what, sizeof what, "days %d mix %d seed %u", days, mix, seed);
                unsigned r = seed;
                int ok = season_league_draw(days, nation, lcg, &r, &d);
                CHECK(ok, "%s: no draw", what);
                if (!ok) continue;
                check_draw(&d, nation, days, what);
                relaxed[days == 6][d.relax]++;
            }
    printf("8 matches: %d, %d, %d draws with relax 0, 1, 2; 6 matches: %d, %d, %d\n", relaxed[0][0], relaxed[0][1],
           relaxed[0][2], relaxed[1][0], relaxed[1][1], relaxed[1][2]);
    CHECK(relaxed[0][0] >= 150 && relaxed[1][0] >= 150, "too many draws without the nation rules");

    /* A random function that stops gives no draw. */
    int left = 100, nat[36];
    for (int i = 0; i < 36; i++) nat[i] = i;
    SeasonLeagueDraw d;
    CHECK(!season_league_draw(8, nat, stop_after, &left, &d), "a stopped draw gives a draw");
    CHECK(!season_league_draw(7, nat, lcg, &left, &d), "7 matches gives a draw");

    for (unsigned seed = 1; seed <= 50; seed++) {
        int po[16], r16[8], used[37] = {0};
        unsigned r = seed;
        CHECK(season_league_knockout(lcg, &r, po, r16), "no knockout");
        for (int k = 0; k < 8; k++) {
            int u = po[2 * k], sd = po[2 * k + 1];
            CHECK(sd >= 9 && sd <= 16 && u >= 17 && u <= 24, "tie %d: %d v %d", k, u, sd);
            CHECK((sd + 1) / 2 + (u + 1) / 2 == 17, "tie %d: pairs %d and %d do not meet", k, sd, u);
            CHECK(r16[k] >= 1 && r16[k] <= 8, "round of 16 seed %d", r16[k]);
            /* 1/2 meets the 15/16 tie, 3/4 the 13/14 tie, 5/6 the 11/12 tie, 7/8 the 9/10 tie. */
            CHECK((r16[k] + 1) / 2 + (sd + 1) / 2 == 9, "seed %d meets the winner of the %d tie", r16[k], sd);
            used[u]++, used[sd]++, used[r16[k]]++;
        }
        for (int p = 1; p <= 24; p++) CHECK(used[p] == 1, "place %d used %d times", p, used[p]);
    }
    if (fails) {
        printf("%d checks failed\n", fails);
        return 1;
    }
    printf("season_uefa_league: all checks passed\n");
    return 0;
}
