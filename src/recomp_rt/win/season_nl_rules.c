/* The Nations League rules (0.6.0 slice 28, season_nl.h). This file reads no game memory, so the unit test
 * tests/seasons/test_season_nl.c builds it alone. Research and evidence: docs/research/2026-10-18-nations-league.md.
 */
#include "season_nl.h"

#include <stdlib.h>
#include <string.h>

int season_nl_weekday(int day, int month, int year)
{
    static const int t[12] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (month < 1 || month > 12) return 0;
    if (month < 3) year--;
    int sunday0 = (year + year / 4 - year / 100 + year / 400 + t[month - 1] + day) % 7;
    return (sunday0 + 6) % 7;
}

const SeasonNlEdition *season_nl_first(const char *confed, int s)
{
    for (int i = 0; i < season_nl_edition_count; i++)
        if (!strcmp(season_nl_editions[i].confed, confed) && season_nl_editions[i].season >= s)
            return &season_nl_editions[i];
    return NULL;
}

/* Item 209 (season_nl.h). */
int season_nl_held_after(const char *confed, int last, int year, int (*wc)(int))
{
    if (!confed || year <= last) return 0;
    if (!strcmp(confed, "CONCACAF") && wc) return !wc(year + 1) && !wc(year + 2);
    return (year - last) % 2 == 0;
}

int season_nl_next_after(const char *confed, int last, int year, int (*wc)(int))
{
    for (int y = (year < last ? last : year) + 1; y <= year + 8; y++)
        if (season_nl_held_after(confed, last, y, wc)) return y;
    return 0;
}

const SeasonNlEdition *season_nl_draw_of(const char *confed, int year, int (*wc)(int))
{
    const SeasonNlEdition *last = NULL, *same = NULL;
    for (int i = 0; confed && i < season_nl_edition_count; i++) {
        const SeasonNlEdition *e = &season_nl_editions[i];
        if (strcmp(e->confed, confed)) continue;
        last = e;
        if (wc && !wc(e->year) == !wc(year)) same = e;
    }
    return confed && !strcmp(confed, "CONCACAF") && same ? same : last;
}

/* Days from 1 January 1970 of a date (the civil calendar), for the gaps below. */
static long nl_days(int y, int m, int d)
{
    y -= m <= 2;
    long era = (y >= 0 ? y : y - 399) / 400;
    long yoe = y - era * 400, doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    return era * 146097 + yoe * 365 + yoe / 4 - yoe / 100 + doy - 719468;
}

/* Item 215 (season_nl.h). */
int season_nl_late_window(const char *confed, int y, int m, int d)
{
    const SeasonNlEdition *prev = NULL;
    long day = nl_days(y, m, d);
    for (int i = 0; confed && i < season_nl_edition_count; i++) {
        const SeasonNlEdition *e = &season_nl_editions[i];
        if (strcmp(e->confed, confed)) continue;
        long draw = nl_days(e->year + e->draw.year, e->draw.month, e->draw.day);
        if (prev) {
            long after = nl_days(prev->year + prev->final.year, prev->final.month, prev->final.day) + 1;
            if (after > draw) draw = after;
        }
        prev = e;
        if (draw < day - NL_FRIENDLY_AHEAD) continue;
        for (int k = 0; k < 6; k++) {
            long md = nl_days(e->year + e->md[k].year, e->md[k].month, e->md[k].day);
            if (e->md[k].month && labs(md - day) <= NL_FRIENDLY_NEAR) return 1;
        }
    }
    return 0;
}

/* The table is sorted by confederation, then season (make_nl.py). */
const SeasonNlEdition *season_nl_after(const SeasonNlEdition *e)
{
    if (!e) return NULL;
    return season_nl_first(e->confed, e->season + 1);
}

/* The overall ranking (season_nl.h). */
/* A new nation (pos 0) ranks after the nations of its league. */
static int place_key(const SeasonNlRow *r) { return r->pos > 0 ? r->pos : 1000; }

static int rank_cmp(const void *a, const void *b)
{
    const SeasonNlRow *x = a, *y = b;
    if (x->league != y->league) return x->league - y->league;
    if (x->pos != y->pos) return place_key(x) - place_key(y);
    if (x->points != y->points) return y->points - x->points;
    if (x->gd != y->gd) return y->gd - x->gd;
    if (x->gf != y->gf) return y->gf - x->gf;
    return x->nation - y->nation;
}

void season_nl_rank(SeasonNlRow *rows, int n)
{
    if (rows && n > 1) qsort(rows, (size_t)n, sizeof *rows, rank_cmp);
}

/* The last place of a row's group: the largest place of that league and group. */
static int group_size(const SeasonNlRow *rows, int n, int league, int group)
{
    int last = 0;
    for (int i = 0; i < n; i++)
        if (rows[i].league == league && rows[i].group == group && rows[i].pos > last) last = rows[i].pos;
    return last;
}

int season_nl_next_leagues(const SeasonNlEdition *from, const SeasonNlEdition *to, SeasonNlRow *rows, int n,
                           int *next)
{
    if (!from || !to || !rows || !next || n <= 0 || n > NL_MAX_NATIONS) return 0;
    season_nl_rank(rows, n);

    /* 1. The moves that the edition gives: each nation's league after promotion and relegation. */
    int moved[NL_MAX_NATIONS];
    int lowest_left[NL_MAX_LEAGUES];
    for (int l = 0; l < NL_MAX_LEAGUES; l++) lowest_left[l] = from->down_lowest[l];
    for (int i = n - 1; i >= 0; i--) {             /* from the bottom of the ranking: the lowest first */
        const SeasonNlRow *r = &rows[i];
        int l = r->league;
        moved[i] = l;
        if (l < 0 || l >= from->leagues || r->pos <= 0) continue;     /* a new nation has no place to move by */
        if (from->up_winners && l > 0 && r->pos == 1) moved[i] = l - 1;
        else if (l + 1 < from->leagues && from->down_last[l] && r->pos > 1 &&
                 r->pos == group_size(rows, n, l, r->group)) moved[i] = l + 1;
        else if (l + 1 < from->leagues && lowest_left[l] > 0 && r->pos > 1) {
            moved[i] = l + 1;
            lowest_left[l]--;
        }
    }

    /* The best runners-up that go up too (rows are ranked, so the first found are the best). */
    for (int l = 1; l < from->leagues && l < NL_MAX_LEAGUES; l++)
        for (int i = 0, left = from->up_second[l]; i < n && left > 0; i++)
            if (rows[i].league == l && rows[i].pos == 2 && moved[i] == l) {
                moved[i] = l - 1;
                left--;
            }

    /* 2. Fill the leagues of `to` in order: the nations of each moved league, in the ranking (rows are ranked
     * by their old league, so a nation that came down from above comes first, then those that stay, then those
     * that came up). A league with room takes the best of the next moved league. A full league passes the
     * lowest of its order down, which can be a nation that came up (no edition of the table does this: the
     * sizes of each edition fit its moves). The last league takes all that are left. */
    int order[NL_MAX_NATIONS], k = 0;
    for (int l = 0; l <= NL_MAX_LEAGUES; l++)
        for (int pass = 0; pass < 3; pass++)
            for (int i = 0; i < n; i++) {
                if (rows[i].pos <= 0) continue;
                if (moved[i] != l && !(l == NL_MAX_LEAGUES && (moved[i] < 0 || moved[i] >= NL_MAX_LEAGUES)))
                    continue;
                /* pass 0: came down, 1: stays, 2: came up */
                int kind = rows[i].league < moved[i] ? 0 : rows[i].league == moved[i] ? 1 : 2;
                if (kind == pass) order[k++] = i;
            }
    int league = 0, used = 0, size[NL_MAX_LEAGUES] = {0};
    for (int j = 0; j < k; j++) {
        while (league < to->leagues - 1 && used >= to->size[league]) {
            league++;
            used = 0;
        }
        next[order[j]] = league;
        size[league]++;
        used++;
    }

    /* 3. A nation new to the competition (pos 0, open item 183) takes the room that the nations of the edition
     * before leave: in the league of its row if that league has room, else in the first league below with room,
     * else in the last league. So it moves no nation of the edition before, and no league grows past its size
     * while a league below has room (a league of 17 in four groups gave a group of five). */
    for (int i = 0; i < n; i++) {
        if (rows[i].pos > 0) continue;
        int l = rows[i].league < 0 ? 0 : rows[i].league;
        if (l > to->leagues - 1) l = to->leagues - 1;
        while (l < to->leagues - 1 && size[l] >= to->size[l]) l++;
        next[i] = l;
        size[l]++;
    }
    return n;
}

int season_nl_paths(const SeasonNlRow *rows, int n, int paths, int per, int *out)
{
    if (!rows || !out || paths <= 0 || per <= 0 || paths > NL_MAX_LEAGUES || n > NL_MAX_NATIONS) return 0;
    char taken[NL_MAX_NATIONS] = {0};
    for (int i = 0; i < paths * per; i++) out[i] = -1;
    int placed = 0, have[NL_MAX_LEAGUES] = {0};
    /* 1. Each path takes its league's group winners, then its other nations, in the ranking. */
    for (int p = 0; p < paths; p++)
        for (int pass = 0; pass < 2; pass++)
            for (int i = 0; i < n && have[p] < per; i++) {
                const SeasonNlRow *r = &rows[i];
                if (taken[i] || r->qualified || r->league != p || (pass == 0) != (r->pos == 1)) continue;
                out[p * per + have[p]++] = r->nation;
                taken[i] = 1;
                placed++;
            }
    /* 2. The places left go to the best nations of the ranking that no path holds, path A first. */
    for (int p = 0; p < paths; p++)
        for (int i = 0; i < n && have[p] < per; i++) {
            if (taken[i] || rows[i].qualified) continue;
            out[p * per + have[p]++] = rows[i].nation;
            taken[i] = 1;
            placed++;
        }
    return placed;
}

void season_nl_path_pairs(const int *path, int *pairs)
{
    pairs[0] = path[0];
    pairs[1] = path[3];
    pairs[2] = path[1];
    pairs[3] = path[2];
}
