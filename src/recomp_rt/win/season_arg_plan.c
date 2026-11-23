/* The fixture plans of the Argentine Primera's zones and derby round (season_sizes_arg.h): pure, no game state
 * (tests/seasons/test_arg_prm_plan.c). */
#include "season_sizes_arg.h"

#define MAX_CLUBS SEASON_ARG_MAX_CLUBS

void season_arg_down_places(int row, int *avg, int *tab)
{
    *tab = 0;
    if (row <= 2011) *avg = 2;
    else if (row <= 2013) *avg = 3;
    else if (row == 2014) *avg = 2;
    else if (row == 2015) *avg = 1;
    else if (row <= 2018) *avg = 4;
    else if (row <= 2020) *avg = 0;     /* 2019/20 and 2021: relegation suspended */
    else if (row == 2021) *avg = 2;
    else if (row == 2023) *avg = 0;     /* 2024: none */
    else *avg = 1, *tab = 1;            /* 2023, 2025, 2026 and the 2027 format */
}

int season_arg_second_down_places(int row_down, int changing_rules)
{
    if (row_down < 0 || row_down > 30) return 0;
    return !changing_rules && row_down > SEASON_ARG_SWAP_DOWN ? SEASON_ARG_SWAP_DOWN : row_down;
}

int season_arg_second_class_down(int clubs, int next_clubs)
{
    return SEASON_ARG_CLASS_DOWN + (next_clubs > 0 && clubs > next_clubs ? clubs - next_clubs : 0);
}

int season_arg_reducido_of(int row, SeasonArgReducido *out)
{
    static const struct {
        int row, places, rounds, pens, zone_last, loser, one, level, each, rel;
        SeasonArgReducidoRound r[SEASON_ARG_REDUCIDO_ROUNDS];
    } rows[] = {
        {2014, 4, 2, 1, 0, -1, 0, 0, 0, 0, {{4, 2, 2}, {0, 1, 2}}},
        {2017, 8, 3, 1, 0, -1, 0, 0, 0, 0, {{8, 4, 1}, {0, 2, 2}, {0, 1, 2}}},
        {2018, 8, 3, 1, 0, -1, 0, 0, 0, 0, {{8, 4, 2}, {0, 2, 2}, {0, 1, 2}}},
        {2019, 6, 3, 1, 4, 1, 1, 1, 1, 0, {{6, 3, 2}, {0, 2, 2}, {0, 1, 1}}},
        {2020, 6, 3, 1, 4, 1, 1, 1, 0, 0, {{6, 3, 2}, {0, 2, 2}, {0, 1, 1}}},
        {2021, 12, 4, 0, 0, -1, 0, 0, 0, 0, {{10, 5, 1}, {1, 3, 1}, {1, 2, 2}, {0, 1, 2}}},
        {2022, 14, 4, 1, 8, 1, 1, 0, 1, 0, {{14, 7, 1}, {0, 4, 2}, {0, 2, 2}, {0, 1, 1}}},
        {2023, 14, 4, 1, 8, 1, 1, 0, 1, 1, {{14, 7, 1}, {0, 4, 2}, {0, 2, 2}, {0, 1, 1}}},
        /* Row 2024, the 2025 season: a final of two legs (en.wikipedia.org 2025 Primera Nacional). Row 2025 keeps that form:
         * no source gives the legs of its final. */
        {2024, 14, 4, 1, 8, 1, 0, 0, 2, 0, {{14, 7, 1}, {0, 4, 2}, {0, 2, 2}, {0, 1, 2}}},
        {2025, 14, 4, 1, 8, 1, 0, 0, 2, 0, {{14, 7, 1}, {0, 4, 2}, {0, 2, 2}, {0, 1, 2}}},
    };
    for (unsigned i = 0; i < sizeof rows / sizeof rows[0]; i++) {
        if (rows[i].row != row) continue;
        out->places = rows[i].places;
        out->rounds = rows[i].rounds;
        out->final_pens = rows[i].pens;
        out->zone_last = rows[i].zone_last;
        out->loser_round = rows[i].loser;
        out->final_one = rows[i].one;
        out->level_pens = rows[i].level;
        out->down_each = rows[i].each;
        out->rel_tie = rows[i].rel;
        /* A zone row plays the zone winners' final first, and its relegation tie. */
        out->stages = rows[i].zone_last ? 1 + rows[i].rel : 0;
        for (int r = 0; r < SEASON_ARG_REDUCIDO_ROUNDS; r++) {
            out->round[r] = rows[i].r[r];
            out->stages += rows[i].r[r].ties;
        }
        return 1;
    }
    return 0;
}

void season_arg_second_zone_split(int n, int a, int *zone)
{
    int in_a = 0, in_b = 0, b = n - a;
    for (int i = 0; i < n; i++) {
        int z = i % 2;
        if (z == 0 && in_a >= a) z = 1;
        if (z == 1 && in_b >= b) z = 0;
        zone[i] = z;
        in_a += z == 0;
        in_b += z == 1;
    }
}

/* The circle method for z clubs (an odd zone rests one club a round). Rounds 0 to c - 1 and their return with the venues
 * reversed. Writes each match of round r at round[r][used[r]++]. Returns the rounds of the zone. */
static int zone_rounds(int z, int first, int round[][SEASON_ARG_ZONE_TIES][2], int *used)
{
    int c = z + (z & 1) - 1, m = c + 1;                 /* m places in the circle; place m - 1 is the rest when z is odd */
    for (int r = 0; r < c; r++)
        for (int k = 0; k < m / 2; k++) {
            int x = k == 0 ? m - 1 : (r + k) % c, y = (r + c - k) % c;
            if (x >= z || y >= z) continue;             /* the rest */
            int h = (r + k) % 2 ? x : y, w = (r + k) % 2 ? y : x;
            round[r][used[r]][0] = first + h;
            round[r][used[r]++][1] = first + w;
            round[c + r][used[c + r]][0] = first + w;
            round[c + r][used[c + r]++][1] = first + h;
        }
    return 2 * c;
}

int season_arg_second_zone_plan(int a, int b, int round[][SEASON_ARG_ZONE_TIES][2])
{
    int used[SEASON_ARG_ZONE_ROUNDS] = {0}, per = (a + b) / 2;
    int ra = 2 * (a + (a & 1) - 1), rb = 2 * (b + (b & 1) - 1), rounds = ra > rb ? ra : rb;
    if (a < 2 || b < 2 || rounds > SEASON_ARG_ZONE_ROUNDS || per > SEASON_ARG_ZONE_TIES) return 0;
    zone_rounds(a, 0, round, used);
    zone_rounds(b, a, round, used);
    for (int r = 0; r < rounds; r++)
        for (int k = used[r]; k < per; k++) round[r][k][0] = round[r][k][1] = -1;
    return rounds;
}

void season_arg_reducido_pairs(const int *sorted, int n, int *out)
{
    for (int i = 0; i < n / 2; i++) {
        out[2 * i] = sorted[i];
        out[2 * i + 1] = sorted[n - 1 - i];
    }
}

static void pair(int round[][SEASON_ARG_MAX_CLUBS / 2][2], int r, int k, int home, int away)
{
    round[r][k][0] = home;
    round[r][k][1] = away;
}

int season_arg_zone_plan(int z, const int *rival, const int *draw, int draw_at,
                         int round[][SEASON_ARG_MAX_CLUBS / 2][2])
{
    if (z < 3 || z % 2 == 0 || z > MAX_CLUBS / 2 || draw_at < 0 || draw_at > z || z + 1 > SEASON_ARG_MAX_ROUNDS)
        return 0;
    /* The drawn round meets other clubs (2025, 2026), or every club's rival again with the venues reversed (2016: the
     * rival twice); a mix of both is no plan. */
    char seen_r[MAX_CLUBS / 2] = {0}, seen_d[MAX_CLUBS / 2] = {0};
    int same = 0;
    for (int i = 0; i < z; i++) {
        if (rival[i] < 0 || rival[i] >= z || draw[i] < 0 || draw[i] >= z || seen_r[rival[i]]++ || seen_d[draw[i]]++)
            return 0;
        same += draw[i] == rival[i];
    }
    if (same && same != z) return 0;
    /* Zone B in the order of its rest rounds: club rival[r] rests in round r, as club r of zone A. */
    for (int c = 0; c < z; c++) {
        int r = c < draw_at ? c : c + 1, k = 0;     /* the circle's round c is the plan's round r */
        for (int d = 1; d <= (z - 1) / 2; d++) {
            int x = (c + d) % z, y = (c - d + z) % z;
            /* Venues by the step alone: each club is the first of a step once and the second once, so it has
             * (z - 1) / 2 home matches in its zone. */
            int first = d % 2 == 1;
            if (first) pair(round, r, k++, x, y); else pair(round, r, k++, y, x);
            if (first) pair(round, r, k++, z + rival[x], z + rival[y]); else pair(round, r, k++, z + rival[y], z + rival[x]);
        }
        if (c % 2 == 0) pair(round, r, k++, c, z + rival[c]); else pair(round, r, k++, z + rival[c], c);
    }
    for (int i = 0; i < z; i++) {
        if (i % 2 == 0) pair(round, draw_at, i, z + draw[i], i); else pair(round, draw_at, i, i, z + draw[i]);
    }
    return z + 1;
}

int season_arg_derby_plan(int n, const int *derby, int derby_at, int round[][SEASON_ARG_MAX_CLUBS / 2][2])
{
    if (n < 2 || n % 2 || n > MAX_CLUBS || derby_at < 0 || derby_at > n - 1 || n > SEASON_ARG_MAX_ROUNDS) return 0;
    for (int i = 0; i < n; i++)
        if (derby[i] < 0 || derby[i] >= n || derby[i] == i || derby[derby[i]] != i) return 0;
    int m = n - 1;
    for (int c = 0; c < m; c++) {
        int r = c < derby_at ? c : c + 1, k = 0;
        if (c % 2 == 0) pair(round, r, k++, m, c); else pair(round, r, k++, c, m);
        for (int d = 1; d < n / 2; d++) {
            int x = (c + d) % m, y = (c - d + m) % m;
            if (d % 2 == 1) pair(round, r, k++, x, y); else pair(round, r, k++, y, x);    /* as in zones */
        }
    }
    for (int i = 0, k = 0; i < n; i++)
        if (i < derby[i]) {
            if (k % 2 == 0) pair(round, derby_at, k++, i, derby[i]); else pair(round, derby_at, k++, derby[i], i);
        }
    return n;
}


/* Sorts idx[0..k) by rep, best first; equal reputations keep their order. */
static void by_reputation(int *idx, int k, const int *rep)
{
    for (int i = 1; i < k; i++) {
        int v = idx[i], j = i;
        for (; j > 0 && rep[idx[j - 1]] < rep[v]; j--) idx[j] = idx[j - 1];
        idx[j] = v;
    }
}

int season_arg_derby_pairs(int n, const int *listed, const int *rep, int *derby)
{
    int orphan[SEASON_ARG_MAX_CLUBS], extra[SEASON_ARG_MAX_CLUBS], no = 0, ne = 0, made = 0;
    for (int i = 0; i < n; i++) {
        derby[i] = listed[i] >= 0 ? listed[i] : -1;
        if (listed[i] == -2) orphan[no++] = i;
        if (listed[i] == -1) extra[ne++] = i;
    }
    by_reputation(orphan, no, rep);
    by_reputation(extra, ne, rep);
    for (int k = 0; k < no && k < ne; k++, made++) {
        derby[orphan[k]] = extra[k];
        derby[extra[k]] = orphan[k];
    }
    /* Left over: the clubs of no pair yet meet the club next to them by reputation. */
    int rest[SEASON_ARG_MAX_CLUBS], nr = 0;
    for (int i = 0; i < n; i++)
        if (derby[i] < 0) rest[nr++] = i;
    by_reputation(rest, nr, rep);
    for (int j = 0; j + 1 < nr; j += 2) derby[rest[j]] = rest[j + 1], derby[rest[j + 1]] = rest[j];
    return made;
}
