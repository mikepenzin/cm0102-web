/* Unit test of the MLS plan (0.7.2 gap row 7, season_mls_plan.c): the season table, the conferences by name, and
 * the regular season of every real season and of other club counts. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "season_mls_plan.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); \
                                       printf("\n"); } } while (0)

/* The real conference sizes (East, West) of each season, from the research table. */
static const int REAL[][3] = {
    {2002, 5, 5},   {2003, 5, 5},   {2004, 5, 5},   {2005, 6, 6},   {2006, 6, 6},   {2007, 7, 6},   {2008, 7, 7},
    {2009, 7, 8},   {2010, 8, 8},   {2011, 9, 9},   {2012, 10, 9},  {2013, 10, 9},  {2014, 10, 9},  {2015, 10, 10},
    {2016, 10, 10}, {2017, 11, 11}, {2018, 11, 12}, {2019, 12, 12}, {2020, 14, 12}, {2021, 14, 13}, {2022, 14, 14},
    {2023, 15, 14}, {2024, 15, 14}, {2025, 15, 15}, {2026, 15, 15},
};

static void test_table(void)
{
    CHECK(!season_mls_schedule(2001), "2001 is the original's form");
    CHECK(!season_mls_playoff(2001), "2001 is the original's play-off");
    const MlsSchedule *s = season_mls_schedule(2002);
    CHECK(s && s->own == 4 && s->other == 2 && s->games == 28 && s->unit == 2, "2002: 4 own, 2 other, 28 games, unit 2");
    s = season_mls_schedule(2011);
    CHECK(s && s->own == 2 && s->other == 2 && s->games == 34, "2011: every club twice, 34 games");
    s = season_mls_schedule(2023);
    CHECK(s && s->own == 2 && s->other == 0 && s->own_to == 28 && s->games == 34, "2023: 28 own, 34 games");
    CHECK(season_mls_schedule(2030) == season_mls_schedule(MLS_LAST_KNOWN), "later seasons keep the last known form");
    const MlsPlayoff *p = season_mls_playoff(2003);
    CHECK(p && p->east == 4 && p->west == 4 && p->legs[2] == 2 && p->legs[3] == 1 && p->final_neutral,
          "2003: 4 a conference, two-leg semi-finals, one-match conference final, neutral final");
    p = season_mls_playoff(2023);
    CHECK(p && p->east == 9 && p->west == 9 && p->legs[0] == 1 && p->legs[1] == 2 && !p->final_neutral,
          "2023: 9 a conference, wild card match, round one as two legs, final at the better club");
    p = season_mls_playoff(2019);
    CHECK(p && p->east == 7 && p->legs[1] == 1, "2019: 7 a conference, one-match rounds");
    p = season_mls_playoff(2020);
    CHECK(p && p->east == 10 && p->west == 8, "2020: 10 East and 8 West");
    p = season_mls_playoff(2008);
    CHECK(p && p->east == 2 && p->west == 2 && p->wild == 4, "2008: 2 a conference and 4 wild cards");
}

static void test_names(void)
{
    CHECK(season_mls_conference("MetroStars", 2002) == MLS_EAST, "MetroStars East");
    CHECK(season_mls_conference("Kansas City Wizards", 2004) == MLS_WEST, "Kansas City West in 2004");
    CHECK(season_mls_conference("Kansas City Wizards", 2005) == MLS_EAST, "Kansas City East in 2005");
    CHECK(season_mls_conference("Sporting Kansas City", 2015) == MLS_WEST, "Sporting Kansas City West in 2015");
    CHECK(season_mls_conference("Houston Dynamo", 2011) == MLS_EAST, "Houston East in 2011");
    CHECK(season_mls_conference("Houston Dynamo", 2016) == MLS_WEST, "Houston West in 2016");
    CHECK(season_mls_conference("Nashville SC", 2022) == MLS_WEST, "Nashville West in 2022");
    CHECK(season_mls_conference("Nashville SC", 2023) == MLS_EAST, "Nashville East in 2023");
    CHECK(season_mls_conference("New York City FC", 2016) == MLS_EAST, "NYCFC East");
    CHECK(season_mls_conference("New York Red Bulls", 2016) == MLS_EAST, "Red Bulls East");
    CHECK(season_mls_conference("LA Galaxy", 2016) == MLS_WEST, "LA Galaxy West");
    CHECK(season_mls_conference("Los Angeles Galaxy", 2016) == MLS_WEST, "Los Angeles Galaxy West");
    CHECK(season_mls_conference("Los Angeles FC", 2018) == MLS_WEST, "LAFC West");
    CHECK(season_mls_conference("D.C. United", 2002) == MLS_EAST, "D.C. United East");
    CHECK(season_mls_conference("Dallas Burn", 2002) == MLS_WEST, "Dallas West");
    CHECK(season_mls_conference("Toronto FC", 2007) == MLS_EAST, "Toronto East");
    CHECK(season_mls_conference("CF Montreal", 2021) == MLS_EAST, "Montreal East");
    CHECK(season_mls_conference("Unknown Town", 2010) == -1, "an unknown club");
}

/* Checks one plan: every club plays `games` (or one fewer when n * games is odd), no club twice in a round, the
 * base meetings kept, the home games balanced. */
static void check_plan(const MlsSchedule *s, int n, const int *conf, int season, int exact)
{
    static MlsPlan p;
    char why[200] = "";
    int r = season_mls_plan(s, n, conf, season, &p, why, sizeof why);
    CHECK(r > 0, "%d: %d clubs: no plan (%s)", season, n, why);
    if (r <= 0) return;
    CHECK(r <= s->games + 8, "%d: %d clubs: %d rounds for %d games", season, n, r, s->games);
    int played[MLS_MAX_CLUBS] = {0}, homes[MLS_MAX_CLUBS] = {0};
    static int busy[MLS_MAX_ROUNDS][MLS_MAX_CLUBS];
    memset(busy, 0, sizeof busy);
    int meet[MLS_MAX_CLUBS][MLS_MAX_CLUBS];
    memset(meet, 0, sizeof meet);
    for (int k = 0; k < p.matches; k++) {
        const MlsMatch *m = &p.m[k];
        if (m->round < 0 || m->round >= r || m->home == m->away) {
            CHECK(0, "%d: match %d has round %d of %d, clubs %d and %d", season, k, m->round, r, m->home, m->away);
            continue;
        }
        CHECK(!busy[m->round][m->home] && !busy[m->round][m->away], "%d: a club plays twice in round %d", season,
              m->round);
        busy[m->round][m->home] = busy[m->round][m->away] = 1;
        played[m->home]++;
        played[m->away]++;
        homes[m->home]++;
        meet[m->home][m->away]++;
        meet[m->away][m->home]++;
    }
    int low = s->games, high = s->games;
    if (!exact) low = s->games - 1;
    if (exact) CHECK(p.own == s->own && p.other == s->other, "%d: the real season keeps its base meetings", season);
    for (int i = 0; i < n; i++) {
        CHECK(played[i] >= low && played[i] <= high, "%d: %d clubs: club %d plays %d of %d", season, n, i, played[i],
              s->games);
        CHECK(abs(2 * homes[i] - played[i]) <= 2, "%d: club %d has %d home games of %d", season, i, homes[i],
              played[i]);
        for (int j = 0; j < n; j++) {
            if (j == i) continue;
            int base = p.conf[i] == p.conf[j] ? p.own : p.other;
            CHECK(meet[i][j] >= base, "%d: clubs %d and %d meet %d times, base %d", season, i, j, meet[i][j], base);
            CHECK(meet[i][j] == p.meet[i][j], "%d: the meeting table and the matches differ", season);
        }
    }
}

static void test_real_seasons(void)
{
    for (size_t k = 0; k < sizeof REAL / sizeof REAL[0]; k++) {
        int season = REAL[k][0], east = REAL[k][1], n = east + REAL[k][2], conf[MLS_MAX_CLUBS];
        for (int i = 0; i < n; i++) conf[i] = i < east ? MLS_EAST : MLS_WEST;
        check_plan(season_mls_schedule(season), n, conf, season, 1);
    }
}

/* Club counts that are not real (a game whose database lacks a club): 10 to 30, splits near half, and clubs of
 * no known conference. */
static void test_other_counts(void)
{
    for (int season = MLS_FIRST_SEASON; season <= MLS_LAST_KNOWN + 1; season++)
        for (int n = 10; n <= 30; n++)
            for (int d = -1; d <= 1; d++) {
                int east = n / 2 + d, conf[MLS_MAX_CLUBS];
                for (int i = 0; i < n; i++) conf[i] = i < east ? MLS_EAST : MLS_WEST;
                if (d == 1) conf[n - 1] = -1;
                check_plan(season_mls_schedule(season), n, conf, season, 0);
            }
}

static void test_playoff_shape(void)
{
    int b[2], t[4], pairs[32];
    int r = season_mls_playoff_rounds(season_mls_playoff(2023), 15, 14, b, t);
    CHECK(r == 4 && b[0] == 9 && b[1] == 9 && t[0] == 8 && t[3] == 1, "2023: brackets of 9, rounds to 8, 4, 2, 1");
    r = season_mls_playoff_rounds(season_mls_playoff(2003), 5, 5, b, t);
    CHECK(r == 2 && b[0] == 4 && t[0] == 2 && t[1] == 1, "2003: brackets of 4, rounds to 2 and 1");
    r = season_mls_playoff_rounds(season_mls_playoff(2020), 14, 12, b, t);
    CHECK(r == 4 && b[0] == 10 && b[1] == 8, "2020: 10 East and 8 West, four rounds");
    r = season_mls_playoff_rounds(season_mls_playoff(2008), 7, 7, b, t);
    CHECK(r == 2 && b[0] == 4 && b[1] == 4, "2008: 2 a conference and 4 wild clubs make brackets of 4");
    r = season_mls_playoff_rounds(season_mls_playoff(2019), 12, 12, b, t);
    CHECK(r == 3 && b[0] == 7 && t[0] == 4, "2019: brackets of 7, rounds to 4, 2, 1");
    r = season_mls_playoff_rounds(season_mls_playoff(2023), 3, 3, b, t);
    CHECK(r == 2 && b[0] == 3 && t[0] == 2, "a small league: brackets of 3, rounds to 2 and 1");
    int m = season_mls_round_pairs(9, 8, pairs);
    CHECK(m == 1 && pairs[0] == 7 && pairs[1] == 8, "9 to 8: the 8th against the 9th");
    m = season_mls_round_pairs(8, 4, pairs);
    CHECK(m == 4 && pairs[0] == 0 && pairs[1] == 7 && pairs[6] == 3 && pairs[7] == 4, "8 to 4: 1 v 8 .. 4 v 5");
    m = season_mls_round_pairs(7, 4, pairs);
    CHECK(m == 3 && pairs[0] == 1 && pairs[1] == 6 && pairs[4] == 3 && pairs[5] == 4, "7 to 4: the first has a bye");
    m = season_mls_round_pairs(4, 8, pairs);
    CHECK(m == 0, "4 to 8: no match");
}

int main(void)
{
    test_table();
    test_playoff_shape();
    test_names();
    test_real_seasons();
    test_other_counts();
    if (fails) {
        printf("%d failures\n", fails);
        return 1;
    }
    printf("season_mls_plan: all checks pass\n");
    return 0;
}
