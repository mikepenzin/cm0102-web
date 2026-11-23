/* The MLS of real life from 2002 (0.7.2 gap row 7): the season table and the regular season plan. See
 * season_mls_plan.h. Sources: docs/research/2026-11-15-mls-seasons.tsv and its second-source check
 * docs/research/2026-11-15-mls-second-source.tsv. */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "season_mls_plan.h"

/* The regular seasons (season, own, other, own_to, games, unit). The research rows give these splits:
 * - 2002: 4 meetings in the conference, 2 with each club of the other, and one designated rival of the other
 *   conference 4 times (28). 2003 and 2004: two designated rivals (30).
 * - 2005, 2006: 4 in the conference and 2 with the other (32).
 * - 2007 to 2009: every club twice, then regional rivals to 30. 2010 (30) and 2011 (34): every club twice.
 * - 2012 to 2014: the conference 2 or 3 times and the other conference once (34). 2013: the 2012 form with home
 *   and away reversed (the second-source check, docs/research/2026-11-15-mls-second-source.tsv).
 * - 2015 to 2019: the conference twice plus extra conference matches, the other conference once (34).
 * - 2020: the restart after the suspension, 23 matches, nearly all in the conference (the real clubs played 18 to 23).
 * - 2021: the conference 2 or 3 times to 32, then 2 matches with the other conference (34).
 * - 2022: the conference twice (26) and 8 with the other (34). 2023 to 2026: the conference twice to 28 and 6 with
 *   the other (34); 2026 the same (mlssoccer.com, second-source check). */
static const MlsSchedule SCHEDULES[] = {
    {2002, 4, 2, 26, 28, 2}, {2003, 4, 2, 26, 30, 2}, {2004, 4, 2, 26, 30, 2}, {2005, 4, 2, 32, 32, 1},
    {2006, 4, 2, 32, 32, 1}, {2007, 2, 2, 30, 30, 1}, {2008, 2, 2, 30, 30, 1}, {2009, 2, 2, 30, 30, 1},
    {2010, 2, 2, 30, 30, 1}, {2011, 2, 2, 34, 34, 1}, {2012, 2, 1, 34, 34, 1}, {2013, 2, 1, 34, 34, 1},
    {2014, 2, 1, 34, 34, 1}, {2015, 2, 1, 34, 34, 1}, {2016, 2, 1, 34, 34, 1}, {2017, 2, 1, 34, 34, 1},
    {2018, 2, 1, 34, 34, 1}, {2019, 2, 1, 34, 34, 1}, {2020, 1, 0, 23, 23, 1}, {2021, 2, 0, 32, 34, 1},
    {2022, 2, 0, 26, 34, 1}, {2023, 2, 0, 28, 34, 1}, {2024, 2, 0, 28, 34, 1}, {2025, 2, 0, 28, 34, 1},
    {2026, 2, 0, 28, 34, 1},
};

/* The play-offs (season, east, west, wild, legs to 8, 4, 2, 1 clubs, final_neutral). legs 0: no such round.
 * - 2002: the two conference winners and 6 others by points; first round and conference finals were series of
 *   matches (first to 5 points), here two legs.
 * - 2003 to 2006: 4 a conference, two-leg conference semi-finals, one-match conference finals.
 * - 2007 to 2010: 2 a conference and 4 wild cards by points, the same rounds.
 * - 2011: 3 a conference and 4 wild cards, one-match wild card round.
 * - 2012 to 2014: 5 a conference, one-match knockout round (4 v 5), two-leg semi-finals and finals.
 * - 2015 to 2018: 6 a conference, one-match knockout round (3 v 6, 4 v 5), two legs after.
 * - 2019, 2021, 2022: 7 a conference, all one match, a bye for the first.
 * - 2020: 10 East (two play-in matches) and 8 West, all one match.
 * - 2023 to 2026: 9 a conference, the wild card match (8 v 9), round one (best of three, here two legs), then one
 *   match.
 * The MLS Cup was at a neutral ground chosen before the season to 2011, at the better club's ground from 2012. */
static const MlsPlayoff PLAYOFFS[] = {
    {2002, 1, 1, 6, {0, 0, 2, 2}, 1}, {2003, 4, 4, 0, {0, 0, 2, 1}, 1}, {2004, 4, 4, 0, {0, 0, 2, 1}, 1},
    {2005, 4, 4, 0, {0, 0, 2, 1}, 1}, {2006, 4, 4, 0, {0, 0, 2, 1}, 1}, {2007, 2, 2, 4, {0, 0, 2, 1}, 1},
    {2008, 2, 2, 4, {0, 0, 2, 1}, 1}, {2009, 2, 2, 4, {0, 0, 2, 1}, 1}, {2010, 2, 2, 4, {0, 0, 2, 1}, 1},
    {2011, 3, 3, 4, {0, 1, 2, 1}, 1}, {2012, 5, 5, 0, {0, 1, 2, 2}, 0}, {2013, 5, 5, 0, {0, 1, 2, 2}, 0},
    {2014, 5, 5, 0, {0, 1, 2, 2}, 0}, {2015, 6, 6, 0, {0, 1, 2, 2}, 0}, {2016, 6, 6, 0, {0, 1, 2, 2}, 0},
    {2017, 6, 6, 0, {0, 1, 2, 2}, 0}, {2018, 6, 6, 0, {0, 1, 2, 2}, 0}, {2019, 7, 7, 0, {0, 1, 1, 1}, 0},
    {2020, 10, 8, 0, {1, 1, 1, 1}, 0}, {2021, 7, 7, 0, {0, 1, 1, 1}, 0}, {2022, 7, 7, 0, {0, 1, 1, 1}, 0},
    {2023, 9, 9, 0, {1, 2, 1, 1}, 0}, {2024, 9, 9, 0, {1, 2, 1, 1}, 0}, {2025, 9, 9, 0, {1, 2, 1, 1}, 0},
    {2026, 9, 9, 0, {1, 2, 1, 1}, 0},
};

const MlsSchedule *season_mls_schedule(int season)
{
    if (season < MLS_FIRST_SEASON) return NULL;
    if (season > MLS_LAST_KNOWN) season = MLS_LAST_KNOWN;
    for (size_t k = 0; k < sizeof SCHEDULES / sizeof SCHEDULES[0]; k++)
        if (SCHEDULES[k].season == season) return &SCHEDULES[k];
    return NULL;
}

const MlsPlayoff *season_mls_playoff(int season)
{
    if (season < MLS_FIRST_SEASON) return NULL;
    if (season > MLS_LAST_KNOWN) season = MLS_LAST_KNOWN;
    for (size_t k = 0; k < sizeof PLAYOFFS / sizeof PLAYOFFS[0]; k++)
        if (PLAYOFFS[k].season == season) return &PLAYOFFS[k];
    return NULL;
}

/* The conference of a club by a word of its name, from the research's conference lists. The first row that
 * matches wins, so the longer names come first ("New York City" before "New York", "Galaxy" before "Los Angeles").
 * A club that moved has one row for each spell. */
static const struct {
    const char *word;
    int from, to, conf;
} WORDS[] = {
    {"red bull", 0, 9999, MLS_EAST},       {"metrostars", 0, 9999, MLS_EAST},  {"new york city", 0, 9999, MLS_EAST},
    {"nycfc", 0, 9999, MLS_EAST},          {"galaxy", 0, 9999, MLS_WEST},      {"chivas", 0, 9999, MLS_WEST},
    {"los angeles", 0, 9999, MLS_WEST},    {"lafc", 0, 9999, MLS_WEST},        {"chicago", 0, 9999, MLS_EAST},
    {"columbus", 0, 9999, MLS_EAST},       {"d.c.", 0, 9999, MLS_EAST},        {"dc united", 0, 9999, MLS_EAST},
    {"washington", 0, 9999, MLS_EAST},     {"new england", 0, 9999, MLS_EAST}, {"toronto", 0, 9999, MLS_EAST},
    {"montreal", 0, 9999, MLS_EAST},       {"montréal", 0, 9999, MLS_EAST},    {"philadelphia", 0, 9999, MLS_EAST},
    {"orlando", 0, 9999, MLS_EAST},        {"atlanta", 0, 9999, MLS_EAST},     {"cincinnati", 0, 9999, MLS_EAST},
    {"miami", 0, 9999, MLS_EAST},          {"charlotte", 0, 9999, MLS_EAST},   {"tampa", 0, 9999, MLS_EAST},
    {"kansas city", 0, 2004, MLS_WEST},    {"kansas city", 2005, 2014, MLS_EAST},
    {"kansas city", 2015, 9999, MLS_WEST}, {"houston", 0, 2010, MLS_WEST},     {"houston", 2011, 2014, MLS_EAST},
    {"houston", 2015, 9999, MLS_WEST},     {"nashville", 0, 2021, MLS_EAST},   {"nashville", 2022, 2022, MLS_WEST},
    {"nashville", 2023, 9999, MLS_EAST},   {"dallas", 0, 9999, MLS_WEST},      {"colorado", 0, 9999, MLS_WEST},
    {"san jose", 0, 9999, MLS_WEST},       {"salt lake", 0, 9999, MLS_WEST},   {"seattle", 0, 9999, MLS_WEST},
    {"portland", 0, 9999, MLS_WEST},       {"vancouver", 0, 9999, MLS_WEST},   {"minnesota", 0, 9999, MLS_WEST},
    {"austin", 0, 9999, MLS_WEST},         {"st. louis", 0, 9999, MLS_WEST},   {"st louis", 0, 9999, MLS_WEST},
    {"san diego", 0, 9999, MLS_WEST},
};

static int has_word(const char *name, const char *word)
{
    size_t n = strlen(name), w = strlen(word);
    for (size_t i = 0; i + w <= n; i++) {
        size_t k = 0;
        while (k < w && tolower((unsigned char)name[i + k]) == (unsigned char)word[k]) k++;
        if (k == w) return 1;
    }
    return 0;
}

int season_mls_conference(const char *name, int season)
{
    if (!name) return -1;
    for (size_t k = 0; k < sizeof WORDS / sizeof WORDS[0]; k++)
        if (season >= WORDS[k].from && season <= WORDS[k].to && has_word(name, WORDS[k].word)) return WORDS[k].conf;
    return -1;
}

int season_mls_playoff_rounds(const MlsPlayoff *p, int ne, int nw, int bracket[2], int targets[4])
{
    if (!p) return 0;
    int e = p->east < ne ? p->east : ne, w = p->west < nw ? p->west : nw;
    int wild = p->wild, rest = ne + nw - e - w;
    if (wild > rest) wild = rest;
    bracket[0] = e;
    bracket[1] = w;
    /* A wild club joins the bracket with fewer clubs (they fill to the same size), as the conference of each wild
     * club decides only which of two equal brackets it takes (season_mls.c). */
    for (int k = 0; k < wild; k++) bracket[bracket[1] < bracket[0] ? 1 : 0]++;
    int most = bracket[0] > bracket[1] ? bracket[0] : bracket[1], r = 0;
    static const int T[4] = {8, 4, 2, 1};
    for (int k = 0; k < 4; k++)
        if (T[k] < most) targets[r++] = T[k];
    return r;
}

int season_mls_round_pairs(int c, int t, int *pairs)
{
    int m = c - t;
    if (m <= 0) return 0;
    for (int k = 0; k < m; k++) {
        pairs[2 * k] = c - 2 * m + k;
        pairs[2 * k + 1] = c - 1 - k;
    }
    return m;
}

/* ---- The plan ---- */

enum { SAME, OTHER, ANY };

static int pair_kind_ok(const MlsPlan *p, int i, int j, int kind)
{
    if (kind == ANY) return 1;
    return (p->conf[i] == p->conf[j]) == (kind == SAME);
}

/* Adds matches of `unit` meetings until each club has `target` games: the club short of the most games meets the
 * club of the right kind short of the most, then the pair met the fewest times, then the turned order. A pair meets
 * at most `cap` times. */
static void fill(MlsPlan *p, const int *ord, int target, int kind, int unit, int cap)
{
    int stuck[MLS_MAX_CLUBS] = {0};
    for (;;) {
        int i = -1;
        for (int k = 0; k < p->n; k++) {
            int c = ord[k];
            if (!stuck[c] && target - p->games[c] >= unit && (i < 0 || p->games[c] < p->games[i])) i = c;
        }
        if (i < 0) return;
        int j = -1;
        for (int k = 0; k < p->n; k++) {
            int c = ord[k];
            if (c == i || !pair_kind_ok(p, i, c, kind) || target - p->games[c] < unit || p->meet[i][c] + unit > cap)
                continue;
            if (j < 0 || p->games[c] < p->games[j] || (p->games[c] == p->games[j] && p->meet[i][c] < p->meet[i][j]))
                j = c;
        }
        if (j < 0) {
            stuck[i] = 1;
            continue;
        }
        p->meet[i][j] += unit;
        p->meet[j][i] += unit;
        p->games[i] += unit;
        p->games[j] += unit;
    }
}

/* A club c two or more games short when no other club is short: one extra meeting a v b (above the base) becomes
 * c v a and c v b, so a and b keep their games and c gains two. */
static void switch_short(MlsPlan *p, const int *ord, int target, int own, int other, int cap)
{
    for (int k = 0; k < p->n; k++) {
        int c = ord[k];
        while (target - p->games[c] >= 2) {
            int done = 0;
            for (int x = 0; x < p->n && !done; x++)
                for (int y = x + 1; y < p->n && !done; y++) {
                    int a = ord[x], b = ord[y];
                    if (a == c || b == c) continue;
                    int base = p->conf[a] == p->conf[b] ? own : other;
                    if (p->meet[a][b] <= base || p->meet[c][a] + 1 > cap || p->meet[c][b] + 1 > cap) continue;
                    p->meet[a][b]--;
                    p->meet[b][a]--;
                    p->meet[c][a]++;
                    p->meet[a][c]++;
                    p->meet[c][b]++;
                    p->meet[b][c]++;
                    p->games[c] += 2;
                    done = 1;
                }
            if (!done) break;
        }
    }
}

/* The home side of each pair: a pair that meets k times plays k / 2 at each ground, and the meeting left over
 * when k is odd goes to the ground that an Euler walk over the odd pairs gives, so every club has at most one home
 * game more or fewer than away (a walk enters and leaves each club it passes). first[i][j] is the home club of the
 * pair's first meeting; the meetings alternate after it. */
static void orient(MlsPlan *p, const int *ord)
{
    static unsigned char left[MLS_MAX_CLUBS][MLS_MAX_CLUBS];
    int deg[MLS_MAX_CLUBS] = {0};
    for (int i = 0; i < p->n; i++)
        for (int j = 0; j < p->n; j++) {
            left[i][j] = (unsigned char)(i != j && (p->meet[i][j] & 1));
            deg[i] += left[i][j];
            p->first[i][j] = -1;
        }
    for (;;) {
        int start = -1;
        for (int k = 0; k < p->n && start < 0; k++)
            if (deg[ord[k]] & 1) start = ord[k];
        for (int k = 0; k < p->n && start < 0; k++)
            if (deg[ord[k]]) start = ord[k];
        if (start < 0) break;
        for (int at = start;;) {
            int to = -1;
            for (int k = 0; k < p->n && to < 0; k++)
                if (left[at][ord[k]]) to = ord[k];
            if (to < 0) break;
            left[at][to] = left[to][at] = 0;
            deg[at]--;
            deg[to]--;
            p->first[at][to] = p->first[to][at] = at;      /* the odd meeting at `at`'s ground */
            at = to;
        }
    }
    for (int i = 0; i < p->n; i++)
        for (int j = i + 1; j < p->n; j++)
            if (p->first[i][j] < 0) p->first[i][j] = p->first[j][i] = ((i + j) & 1) ? i : j;
}

int season_mls_plan(const MlsSchedule *s, int n, const int *conf, int turn, MlsPlan *p, char *why, int why_size)
{
    if (why_size > 0) why[0] = 0;
    if (!s || n < 2 || n > MLS_MAX_CLUBS) {
        snprintf(why, (size_t)why_size, "no schedule or %d clubs", n);
        return 0;
    }
    memset(p, 0, sizeof *p);
    p->n = n;
    int count[2] = {0, 0};
    for (int i = 0; i < n; i++)
        if (conf[i] == MLS_EAST || conf[i] == MLS_WEST) count[conf[i]]++;
    for (int i = 0; i < n; i++) {
        int c = conf[i];
        if (c != MLS_EAST && c != MLS_WEST) c = count[MLS_WEST] < count[MLS_EAST] ? MLS_WEST : MLS_EAST;
        if (conf[i] != c) count[c]++;
        p->conf[i] = c;
    }
    int ord[MLS_MAX_CLUBS];
    int t = turn % n;
    if (t < 0) t += n;
    for (int k = 0; k < n; k++) ord[k] = (k + t) % n;
    /* The base meetings. With more clubs than the real season (a club count that is not the row's), the base can
     * pass the games: the meetings step down, the larger first, until every club fits. */
    int own = s->own, other = s->other;
    for (;;) {
        int worst = 0;
        for (int c = 0; c < 2; c++)
            if (count[c]) {
                int g = (count[c] - 1) * own + count[1 - c] * other;
                if (g > worst) worst = g;
            }
        if (worst <= s->games || (own <= 1 && other <= 0)) break;
        if (other > 0 && (other >= own || own <= 1)) other--;
        else own--;
    }
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (i != j) {
                p->meet[i][j] = p->conf[i] == p->conf[j] ? own : other;
                p->games[i] += p->meet[i][j];
            }
    int most = own > other ? own : other;
    fill(p, ord, s->own_to, SAME, 1, own + 1);
    fill(p, ord, s->games, OTHER, s->unit, other + s->unit);
    fill(p, ord, s->games, ANY, 1, most + 2);
    switch_short(p, ord, s->games, own, other, most + 2);
    p->own = own;
    p->other = other;
    orient(p, ord);
    for (int i = 0; i < n; i++)
        if (p->games[i] > s->games) {
            snprintf(why, (size_t)why_size, "club %d has %d games of %d", i, p->games[i], s->games);
            return 0;
        }
    /* The matches: each pair's meetings in layers (every pair's first meeting, then the second, ...), a layer in
     * the order of the circle method on the turned clubs, so the first layer of a full league is a round robin. */
    int even = n + (n & 1), max_meet = 0;
    for (int i = 0; i < n; i++)
        for (int j = 0; j < n; j++)
            if (p->meet[i][j] > max_meet) max_meet = p->meet[i][j];
    int last_round[MLS_MAX_CLUBS][MLS_MAX_CLUBS], last_home[MLS_MAX_CLUBS][MLS_MAX_CLUBS];
    memset(last_round, -1, sizeof last_round);
    memset(last_home, -1, sizeof last_home);
    static unsigned char busy[MLS_MAX_ROUNDS][MLS_MAX_CLUBS];
    memset(busy, 0, sizeof busy);
    int rounds = 0;
    for (int layer = 0; layer < max_meet; layer++)
        for (int r = 0; r < even - 1; r++)
            for (int k = 0; k < even / 2; k++) {
                int a = k == 0 ? even - 1 : (r + k) % (even - 1), b = k == 0 ? r : (r - k + even - 1) % (even - 1);
                if (a >= n || b >= n) continue;
                int i = ord[a], j = ord[b];
                if (p->meet[i][j] <= layer) continue;
                if (p->matches >= MLS_MAX_MATCHES) {
                    snprintf(why, (size_t)why_size, "more than %d matches", MLS_MAX_MATCHES);
                    return 0;
                }
                int home = last_home[i][j] >= 0 ? (last_home[i][j] == i ? j : i) : p->first[i][j];
                int away = home == i ? j : i;
                int round = last_round[i][j] + 1;
                while (round < MLS_MAX_ROUNDS && (busy[round][i] || busy[round][j])) round++;
                if (round >= MLS_MAX_ROUNDS) {
                    snprintf(why, (size_t)why_size, "more than %d rounds", MLS_MAX_ROUNDS);
                    return 0;
                }
                busy[round][i] = busy[round][j] = 1;
                last_round[i][j] = last_round[j][i] = round;
                last_home[i][j] = last_home[j][i] = home;
                p->home[home]++;
                p->m[p->matches++] = (MlsMatch){home, away, round};
                if (round + 1 > rounds) rounds = round + 1;
            }
    p->rounds = rounds;
    return rounds;
}
