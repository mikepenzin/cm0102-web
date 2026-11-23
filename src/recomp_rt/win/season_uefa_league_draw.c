/* The league phase draw (season_uefa_league.h), 0.6.0 slice 25. No game memory: the game passes the nations of
 * the clubs and its random number function. Evidence and the method: docs/research/2026-10-19-uefa-league-phase.md.
 *
 * The opponents. Each block of two pots (p, q) gives every club of p its opponents in q:
 *   8 matches, p == q: a permutation of the 9 clubs with no fixed point and no 2-cycle; club a hosts pi(a), so
 *   it plays one club of its pot at home and another away.
 *   8 matches, p != q: two bijections, s (p to q) and t (q to p), with t(s(a)) != a; a hosts s(a), b hosts t(b).
 *   6 matches, p == q: a perfect matching of the 6 clubs; p != q: a bijection. The home side comes after: the
 *   matches between two pot pairs (1 and 2, 3 and 4, 5 and 6) give each club two of them, so they form cycles,
 *   and each cycle is turned one way, so each club plays one of the two at home.
 * Each block is drawn with the nation rules against the opponents of the blocks before it; a block that finds no
 * draw starts the whole draw again. After TRIES draws the "at most two of one nation" rule is dropped, after
 * twice as many the "not of one nation" rule too.
 *
 * The match days. The matches of each club fill the days one by one: each day is a perfect matching of the
 * matches not yet placed, found by a search from the lowest club with no match that day. A day with no matching
 * starts the days again; after DAY_TRIES the opponents are drawn again. */
#include "season_uefa_league.h"

#include <string.h>

#define N SEASON_LEAGUE_CLUBS
#define TRIES 200
#define DAY_TRIES 50
#define SEARCH_STEPS 20000

typedef struct {
    SeasonLeagueRandom rnd;
    void *ctx;
    int stop;
} Rng;

typedef struct {
    Rng r;
    const int *nation;
    int relax;
    int opp[N][SEASON_LEAGUE_MAX_DAYS];     /* the opponents of each club */
    int home[N][SEASON_LEAGUE_MAX_DAYS];    /* 1: club plays opp at home */
    int n_opp[N];
} Draw;

static int rng(Rng *g, int n)
{
    if (g->stop || n <= 1) return 0;
    int r = g->rnd(n, g->ctx);
    if (r < 0) {
        g->stop = 1;
        return 0;
    }
    return r < n ? r : n - 1;
}

static int rnd(Draw *d, int n) { return rng(&d->r, n); }

static void shuffle(Draw *d, int *a, int n)
{
    for (int i = n - 1; i > 0; i--) {
        int j = rnd(d, i + 1), t = a[i];
        a[i] = a[j];
        a[j] = t;
    }
}

static int met(const Draw *d, int a, int b)
{
    for (int i = 0; i < d->n_opp[a]; i++)
        if (d->opp[a][i] == b) return 1;
    return 0;
}

/* 1 when a may meet b under the rules (and they have not met). */
static int allowed(const Draw *d, int a, int b)
{
    if (a == b || met(d, a, b)) return 0;
    if (d->relax >= 2) return 1;
    if (d->nation[a] == d->nation[b]) return 0;
    if (d->relax >= 1) return 1;
    int na = 0, nb = 0;
    for (int i = 0; i < d->n_opp[a]; i++) na += d->nation[d->opp[a][i]] == d->nation[b];
    for (int i = 0; i < d->n_opp[b]; i++) nb += d->nation[d->opp[b][i]] == d->nation[a];
    return na < 2 && nb < 2;
}

static void add(Draw *d, int a, int b, int a_home)
{
    d->opp[a][d->n_opp[a]] = b, d->home[a][d->n_opp[a]++] = a_home;
    d->opp[b][d->n_opp[b]] = a, d->home[b][d->n_opp[b]++] = a_home < 0 ? -1 : !a_home;
}

static void drop(Draw *d, int a, int b)
{
    d->n_opp[a]--;
    d->n_opp[b]--;
}

/* A bijection from the clubs from[0..n-1] to to[0..n-1] (each from-club hosts its partner when host is 1, or is
 * the away side when 0; -1: no side yet). Backtracking in random order. The partner must not have
 * met the club already (allowed() tests it: the second bijection of an 8-match block). */
static int bijection(Draw *d, const int *from, const int *to, int n, int i, int *used, int host, int *steps)
{
    if (i == n) return 1;
    if (++*steps > SEARCH_STEPS || d->r.stop) return 0;
    int order[9];
    for (int k = 0; k < n; k++) order[k] = k;
    shuffle(d, order, n);
    for (int k = 0; k < n; k++) {
        int j = order[k];
        if (used[j] || !allowed(d, from[i], to[j])) continue;
        used[j] = 1;
        add(d, from[i], to[j], host);
        if (bijection(d, from, to, n, i + 1, used, host, steps)) return 1;
        drop(d, from[i], to[j]);
        used[j] = 0;
    }
    return 0;
}

/* 8 matches, p == q: each club hosts one club of the pot and visits another (a permutation with no fixed point
 * and no 2-cycle: the "met" test of allowed stops a 2-cycle). */
static int own_pot(Draw *d, const int *pot, int n, int *steps)
{
    int used[9] = {0};
    return bijection(d, pot, pot, n, 0, used, 1, steps);
}

/* 6 matches, p == q: a perfect matching of the pot (no side yet). */
static int matching(Draw *d, const int *pot, int n, int *done, int *steps)
{
    int i = 0;
    while (i < n && done[i]) i++;
    if (i == n) return 1;
    if (++*steps > SEARCH_STEPS || d->r.stop) return 0;
    int order[6], m = 0;
    for (int k = i + 1; k < n; k++)
        if (!done[k]) order[m++] = k;
    shuffle(d, order, m);
    done[i] = 1;
    for (int k = 0; k < m; k++) {
        int j = order[k];
        if (!allowed(d, pot[i], pot[j])) continue;
        done[j] = 1;
        add(d, pot[i], pot[j], -1);
        if (matching(d, pot, n, done, steps)) return 1;
        drop(d, pot[i], pot[j]);
        done[j] = 0;
    }
    done[i] = 0;
    return 0;
}

/* The pot of club c with pots of size s. */
static int pot_of(int c, int s) { return c / s; }

/* 6 matches: the home sides. For each two pot pairs (A, B), each club has two matches against the clubs of the
 * other pair (or of its own), so these matches form cycles; each cycle is turned one way at random. */
static void orient(Draw *d)
{
    for (int a = 0; a < N; a++)
        for (int i = 0; i < d->n_opp[a]; i++)
            if (d->home[a][i] < 0) {
                /* Walk the cycle of a's match i within the pair-of-pairs class of (pair(a), pair(opp)). */
                int pa = pot_of(a, 6) / 2, pb = pot_of(d->opp[a][i], 6) / 2;
                int turn = rnd(d, 2), cur = a, k = i;
                for (int guard = 0; guard < 2 * N && d->home[cur][k] < 0; guard++) {
                    int nxt = d->opp[cur][k];
                    d->home[cur][k] = turn;
                    for (int j = 0; j < d->n_opp[nxt]; j++)
                        if (d->opp[nxt][j] == cur) d->home[nxt][j] = !turn;
                    /* The other match of nxt in this class. */
                    int pn = pot_of(nxt, 6) / 2, want = pn == pa ? pb : pa, next_k = -1;
                    for (int j = 0; j < d->n_opp[nxt]; j++) {
                        int o = d->opp[nxt][j];
                        if (o != cur && pot_of(o, 6) / 2 == want && d->home[nxt][j] < 0) next_k = j;
                    }
                    if (next_k < 0) break;
                    cur = nxt, k = next_k;
                }
            }
}

static int draw_opponents(Draw *d, int days)
{
    memset(d->n_opp, 0, sizeof d->n_opp);
    int s = days == 8 ? 9 : 6, pots = N / s, steps = 0;
    int pot[6][9];
    for (int p = 0; p < pots; p++)
        for (int k = 0; k < s; k++) pot[p][k] = p * s + k;
    /* The blocks in random order, so no pot always comes last. */
    int blocks[21][2], nb = 0;
    for (int p = 0; p < pots; p++)
        for (int q = p; q < pots; q++) blocks[nb][0] = p, blocks[nb++][1] = q;
    for (int i = nb - 1; i > 0; i--) {
        int j = rnd(d, i + 1), t0 = blocks[i][0], t1 = blocks[i][1];
        blocks[i][0] = blocks[j][0], blocks[i][1] = blocks[j][1];
        blocks[j][0] = t0, blocks[j][1] = t1;
    }
    for (int b = 0; b < nb && !d->r.stop; b++) {
        int p = blocks[b][0], q = blocks[b][1];
        steps = 0;
        if (days == 8 && p == q) {
            if (!own_pot(d, pot[p], s, &steps)) return 0;
        } else if (days == 8) {
            int u1[9] = {0}, u2[9] = {0};
            if (!bijection(d, pot[p], pot[q], s, 0, u1, 1, &steps)) return 0;
            steps = 0;
            if (!bijection(d, pot[q], pot[p], s, 0, u2, 1, &steps)) return 0;
        } else if (p == q) {
            int done[6] = {0};
            if (!matching(d, pot[p], s, done, &steps)) return 0;
        } else {
            int u[9] = {0};
            if (!bijection(d, pot[p], pot[q], s, 0, u, -1, &steps)) return 0;
        }
    }
    if (d->r.stop) return 0;
    if (days == 6) orient(d);
    for (int a = 0; a < N; a++) {
        int h = 0;
        if (d->n_opp[a] != days) return 0;
        for (int i = 0; i < days; i++) {
            if (d->home[a][i] < 0) return 0;
            h += d->home[a][i];
        }
        if (h != days / 2) return 0;
    }
    return 1;
}

/* ---- The match days ---- */

typedef struct {
    int placed[N][SEASON_LEAGUE_MAX_DAYS];  /* 1: match i of club a is in a day */
    int busy[N];
    int steps;
} Days;

static int fill_day(Draw *d, Days *s, int days, SeasonLeagueMatch *out, int n)
{
    int a = 0;
    while (a < N && s->busy[a]) a++;
    if (a == N) return 1;
    if (++s->steps > SEARCH_STEPS || d->r.stop) return 0;
    int order[SEASON_LEAGUE_MAX_DAYS];
    for (int i = 0; i < days; i++) order[i] = i;
    shuffle(d, order, days);
    for (int k = 0; k < days; k++) {
        int i = order[k], b = d->opp[a][i];
        if (s->placed[a][i] || s->busy[b]) continue;
        int j = 0;
        while (j < days && d->opp[b][j] != a) j++;
        s->placed[a][i] = s->placed[b][j] = 1;
        s->busy[a] = s->busy[b] = 1;
        out[n].home = (signed char)(d->home[a][i] ? a : b);
        out[n].away = (signed char)(d->home[a][i] ? b : a);
        if (fill_day(d, s, days, out, n + 1)) return 1;
        s->placed[a][i] = s->placed[b][j] = 0;
        s->busy[a] = s->busy[b] = 0;
    }
    return 0;
}

static int make_days(Draw *d, int days, SeasonLeagueDraw *out)
{
    for (int t = 0; t < DAY_TRIES && !d->r.stop; t++) {
        Days s;
        memset(&s, 0, sizeof s);
        int ok = 1;
        for (int day = 0; day < days && ok; day++) {
            memset(s.busy, 0, sizeof s.busy);
            s.steps = 0;
            ok = fill_day(d, &s, days, out->match[day], 0);
        }
        if (ok) return 1;
    }
    return 0;
}

void season_league_by_reputation(const int *reputation, int order[SEASON_LEAGUE_CLUBS])
{
    for (int i = 0; i < N; i++) {                           /* an insertion sort: equal clubs keep their order */
        int j = i;
        for (; j > 0 && reputation[order[j - 1]] < reputation[i]; j--) order[j] = order[j - 1];
        order[j] = i;
    }
}

/* The clubs of nation x in pot p of size s. */
static int pot_count(const int *nation, const int *order, int s, int p, int x)
{
    int k = 0;
    for (int i = p * s; i < (p + 1) * s; i++) k += nation[order[i]] == x;
    return k;
}

/* The most clubs of nation x that one pot can hold under the nation rules. Own pot: 4 of 9, or 3 of 6. Other pots:
 * each of the k clubs of x meets 2 clubs of each pot (8 matches), and a club meets at most 2 clubs of x, so a pot
 * needs k clubs of other nations: k <= 9 - in_pot. With 6 matches each meets 1, so k <= 2 * (6 - in_pot). */
static int pot_limit(const int *nation, int s, int x)
{
    int k = 0;
    for (int i = 0; i < N; i++) k += nation[i] == x;
    int limit = s == 9 ? (9 - k < 4 ? 9 - k : 4) : (6 - (k + 1) / 2 < 3 ? 6 - (k + 1) / 2 : 3);
    return limit < 1 ? 1 : limit;
}

/* The place of the club that takes the place of a surplus club of nation x from pot p: the nearest pot first, the
 * later pot before the earlier one, and in that pot the club nearest in seed. -1 when no pot can take it. */
static int swap_place(const int *nation, const int *order, int s, int p, int x)
{
    int pots = N / s, limit = pot_limit(nation, s, x);
    for (int dist = 1; dist < pots; dist++)
        for (int dir = 1; dir >= -1; dir -= 2) {
            int q = p + dir * dist;
            if (q < 0 || q >= pots || pot_count(nation, order, s, q, x) >= limit) continue;
            for (int k = 0; k < s; k++) {
                int j = dir > 0 ? q * s + k : (q + 1) * s - 1 - k, y = nation[order[j]];
                if (y != x && pot_count(nation, order, s, p, y) < pot_limit(nation, s, y)) return j;
            }
        }
    return -1;
}

void season_league_pots(int days, const int *nation, int order[SEASON_LEAGUE_CLUBS])
{
    int s = days == 8 ? 9 : 6;
    for (int i = 0; i < N; i++) order[i] = i;
    for (int p = 0; p < N / s; p++)
        for (int i = (p + 1) * s - 1; i >= p * s; i--) {    /* the lowest seed of a nation moves first */
            int x = nation[order[i]];
            if (pot_count(nation, order, s, p, x) <= pot_limit(nation, s, x)) continue;
            int j = swap_place(nation, order, s, p, x);
            if (j < 0) continue;                            /* no place: the draw's last rule decides */
            int t = order[i];
            order[i] = order[j], order[j] = t;
        }
}

int season_league_draw(int days, const int *nation, SeasonLeagueRandom rnd_fn, void *ctx, SeasonLeagueDraw *out)
{
    if ((days != 8 && days != 6) || !nation || !rnd_fn || !out) return 0;
    static Draw d;                          /* about 3 KB: kept off the game's small host stack */
    memset(&d, 0, sizeof d);
    d.r.rnd = rnd_fn, d.r.ctx = ctx, d.nation = nation;
    memset(out, 0, sizeof *out);
    out->days = days;
    for (int t = 0; t < 3 * TRIES && !d.r.stop; t++) {
        d.relax = t < TRIES ? 0 : t < 2 * TRIES ? 1 : 2;
        if (!draw_opponents(&d, days)) continue;
        if (make_days(&d, days, out)) {
            out->relax = d.relax;
            return 1;
        }
    }
    return 0;
}

int season_league_knockout(SeasonLeagueRandom rnd_fn, void *ctx, int po[16], int r16[8])
{
    Rng g = {rnd_fn, ctx, 0};
    /* Tie groups j = 0..3 of the bracket (see the header): seeded pair 15/16, 9/10, 13/14, 11/12 with the
     * unseeded pair 17/18, 23/24, 19/20, 21/22, and the round of 16 seeds 1/2, 7/8, 3/4, 5/6. Tie k and tie
     * k + 4 are the two ties of group k. */
    static const int seeded[4] = {15, 9, 13, 11}, unseeded[4] = {17, 23, 19, 21}, top[4] = {1, 7, 3, 5};
    for (int j = 0; j < 4; j++) {
        int a = rng(&g, 2), b = rng(&g, 2), c = rng(&g, 2);
        po[2 * j] = unseeded[j] + a, po[2 * j + 1] = seeded[j] + b;
        po[2 * (j + 4)] = unseeded[j] + !a, po[2 * (j + 4) + 1] = seeded[j] + !b;
        r16[j] = top[j] + c, r16[j + 4] = top[j] + !c;
    }
    return !g.stop;
}
