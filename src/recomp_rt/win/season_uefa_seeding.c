/* The UEFA places of each club from the access list by association rank (0.6.0 slice 23, Q369: from 2009/10;
 * season_uefa.h). Evidence: docs/research/2026-10-18-uefa-2009-2020.md.
 *
 * The original. Uefa_Seeding::update_season 0x9035a0 (thiscall, this at [0xde1f58]) runs once a year, at the
 * first of the three UEFA cups that makes or starts its season (their constructors 0x583881, 0x63304d,
 * 0x902377 and start_new_season 0x584e56, 0x6339df, 0x902fac; this+0x439 holds the year). It sorts the nations
 * by coefficient (roll_coefficients 0x903b50: the 0x12-byte entries at this+0, count this+0x438, entry index =
 * rank, 0 the top), makes a 12-byte record of each European club (collect_european_clubs 0x904040, club +0x1db
 * set to -1, sorted by nation, division and league position, 0x905ce0), then gives the places with three
 * functions, each thiscall (records, short count) ret 8 with one caller: assign_champions_league_places
 * 0x904210 (0x90375d), assign_uefa_cup_places 0x904580 (0x90376a) and assign_intertoto_places 0x904e30
 * (0x903777). They write club +0x1db (the record id of the cup: [0x9cf6e8], [0x9cf6f0], [0x9cf6f8]) and +0x1df
 * (the entry code) and count each club in entry +0x11 (the coefficient divisor, 0x903c00). Their places come
 * from rank brackets in the code, not from entry +0xc and +0xd. Those two are read only by get_nation_places
 * 0x905c60 (the league table marks 0x6853f5, the Serie A play-offs 0x641af7, the board 0x6a3a32).
 *
 * This file. In a season of the access list (season_uefa_access_places gives places for the row of the season),
 * the wrap of 0x904210 gives every Champions League and Europa League place, and the wraps of 0x904580 and
 * 0x904e30 give none (the Intertoto Cup is not held: no club gets its id, and the English volunteer list at
 * this+0x43b is freed as 0x904fc0..0x904fe1 does). Other seasons, the base game and RECOMP_SEASON_CODE=off call
 * the original's functions.
 *
 * The places, in this order:
 *   1. the Champions League places of each nation by league position (the p-th club of its records,
 *      0x9050d0 with skip placed 0, as 0x904465), with the entry round of the access list.
 *   2. the Champions League title holder (the history winner of the cup, 0x4afd80 as 0x9042e3) in the group
 *      stage, when it has no place yet. Else its place is free (the cascade of step 6 fills it).
 *   3. the Europa League title holder: from 2015/16 a Champions League place ("X" rows), before a Europa League
 *      group place ("H" rows), when it has no place yet.
 *   4. the Europa League places of each nation: the first the domestic cup winner (the cup of the league
 *      table entry, 0xb63d7e, as 0x9047e2: the winner, else the runner-up), the last the league cup winner for the
 *      nations of the "L" rows (0xb63d82, as 0x904ad8), the others the first club with no place (0x9050d0 with
 *      skip placed 1).
 *   5. the fair play places (to 2015/16): three nations at random (0x90cfc0, as 0x904670) get one Europa League
 *      place more.
 *   6. the cascade: the number of new clubs of each entry round must be the number the round sizes of the row
 *      give (season_uefa_new_clubs). From the group stage down, a round with too many clubs moves its
 *      lowest-ranked club down one round of its path, and a round with too few takes the best-ranked club of
 *      the rounds below in its path (the group stage first from the Champions Path: the access lists give the
 *      free place to the best-ranked champion). The lowest round gives up its extra clubs, and fills a gap with
 *      the first club with no place of the best-ranked nations (as the UEFA Cup's fillers of 0x902cb0).
 *   Slice 24, part b (2021/22 on): after step 4 the Conference League places of each nation ("k" rows): the
 *      first the cup winner when the nation has no Europa League place, the last the league cup winner for the
 *      nations of the "M" rows, the others the first club with no place. The Conference League title holder
 *      gets a Europa League group place ("Y" rows, 2022/23 on) or a Conference League place ("K" rows) when it
 *      has no place yet. Step 6 runs for the Conference League too, on one path. These places come only when the
 *      game has the Conference League record (season_uefa_ecl_id), and its id is the club's +0x1db.
 * Each club gets +0x1db and the entry code of season_uefa.h (SEASON_UEFA_CL_*, SEASON_UEFA_EL_*,
 * SEASON_UEFA_ECL_*), and its nation entry +0x11 one more. Entry +0xc and +0xd get the nation's league places of the list (the table
 * marks), +0xe 0. */
#include "win.h"
#include "season.h"
#include "season_uefa.h"
#include "season_nation.h"

#include <stdio.h>
#include <string.h>

#define KEY_CL 0x9cf6e8u
#define KEY_UC 0x9cf6f0u
#define COMP_RECORDS 0xae23d0u          /* comp records, 0x6b bytes each */
#define LEAGUE_TABLE 0xb63d60u          /* league table entries, 0x48 bytes: +0 nation, +0x1e cup, +0x22 league cup */
#define LEAGUE_ENGLAND 0xb64150u        /* the England entry's nation (key_nation_build_table 0x667150) */
#define LEAGUE_FRANCE 0xb641e0u
/* The club table and its count, as collect_european_clubs 0x904040 walks them (count [0xae235c] at 0x9040df,
 * table [0xae23b4] at 0x9040e5, 0x245 bytes a club at 0x9041b9). */
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u

#define F_PICK 0x009050d0u              /* thiscall (records, short count, nation id, char n, skip placed, skip human) */
#define F_HISTORY_CLUB 0x004afd80u      /* cdecl (comp record, k): k 0 the last winner, 1 the runner-up */
#define F_NATION_INDEX 0x00669340u      /* key_nation_index_of_nation, cdecl (nation): the league table index */
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (range) */
#define F_FREE 0x009452cau

#define MAX_PLACED 400
#define MAX_NATIONS 60                  /* the entries at this+0 (0x438 / 0x12) */

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* ---- The placed clubs ---- */

typedef struct {
    uint32_t club;
    char comp;          /* 'c', 'e' or 'k' (slice 24: the Conference League) */
    int code;
    int rank;           /* 1 is the top association */
    int champion;       /* a Champions League club that is its nation's champion (the Champions Path) */
    int order;          /* the order of the places, for ties of rank */
} Placed;

static Placed placed[MAX_PLACED];
static int n_placed;

static int has_place(uint32_t club) { return readable(club) && (int32_t)rd32(g_mem, club + 0x1db) != -1; }

static int32_t ecl_id = -1;             /* slice 24: the Conference League record id of assign_all, -1 for none */

static void add(uint32_t club, char comp, int code, int rank, int champion)
{
    if (!readable(club) || has_place(club) || !readable(rd32(g_mem, club + 0x53)) || n_placed >= MAX_PLACED) return;
    if (comp == 'k' && ecl_id < 0) return;
    /* The mark of a place, as 0x90430e. */
    wr32(g_mem, club + 0x1db, comp == 'k' ? (uint32_t)ecl_id : rd32(g_mem, comp == 'c' ? KEY_CL : KEY_UC));
    placed[n_placed] = (Placed){club, comp, code, rank, champion, n_placed};
    n_placed++;
}

static uint32_t pick(uint32_t self, uint32_t records, int count, uint32_t nation_id, int n, int skip_placed)
{
    uint32_t a[6] = {records, (uint32_t)count, nation_id, (uint32_t)n, (uint32_t)skip_placed, 0};
    uint32_t c = call_this(F_PICK, self, 6, a);
    return rt_unwinding ? 0 : c;
}

static uint32_t history_club_of(int32_t id, int k)
{
    uint32_t recs = rd32(g_mem, COMP_RECORDS);
    if (id < 0 || !readable(recs)) return 0;
    uint32_t a[2] = {recs + 0x6bu * (uint32_t)id, (uint32_t)k};
    uint32_t c = call_cdecl(F_HISTORY_CLUB, 2, a);
    return rt_unwinding || !readable(c) ? 0 : c;
}

static uint32_t history_club(uint32_t key, int k) { return history_club_of((int32_t)rd32(g_mem, key), k); }

/* The winner (k 0) or runner-up (k 1) of the cup at offset off (0x1e cup, 0x22 league cup) of the nation's
 * league table entry, or 0. */
static uint32_t cup_club(uint32_t nation, uint32_t off, int k)
{
    uint32_t a[1] = {nation};
    int idx = (int8_t)call_cdecl(F_NATION_INDEX, 1, a);
    if (rt_unwinding || idx < 0 || idx >= 35) return 0;
    uint32_t entry = LEAGUE_TABLE + 0x48u * (uint32_t)idx;
    uint32_t rec = rd32(g_mem, entry + off);
    if (rd32(g_mem, entry) != nation || !readable(rec)) return 0;
    uint32_t b[2] = {rec, (uint32_t)k};
    uint32_t c = call_cdecl(F_HISTORY_CLUB, 2, b);
    return rt_unwinding || !readable(c) ? 0 : c;
}

static int rank_of_nation(uint32_t self, int entries, uint32_t nation)
{
    for (int i = 0; i < entries; i++)
        if (rd32(g_mem, self + 0x12u * (uint32_t)i + 4) == nation) return i + 1;
    return 99;
}

/* Slice 24 (Q293, Q365): 1 when the nation (a nation record) is suspended in row r: its clubs get no place,
 * the title holders included, and no filler comes from it. The nation's record id is in the key global of the
 * nations table (season_nation.h), as season_kind_nation.c reads it. */
static int nation_suspended(const SeasonUefa *r, uint32_t nation)
{
    if (!r || !r->suspended || !*r->suspended || !readable(nation)) return 0;
    for (int i = 0; i < season_nation_count; i++) {
        int32_t id = (int32_t)rd32(g_mem, season_nations[i].key_global);
        if (id >= 0 && (int32_t)rd32(g_mem, nation) == id) return season_uefa_suspended(r, season_nations[i].name);
    }
    return 0;
}

static int club_suspended(const SeasonUefa *r, uint32_t club)
{
    return readable(club) && nation_suspended(r, rd32(g_mem, club + 0x53));
}

static int count_tokens(const char *s)
{
    int n = 0;
    for (const char *p = s; p && *p;) {
        while (*p == ' ') p++;
        if (!*p) break;
        n++;
        while (*p && *p != ' ') p++;
    }
    return n;
}

/* The k-th token of s as a code, 0 when s has fewer. */
static int token_code(char comp, const char *s, int k)
{
    for (const char *p = s; p && *p;) {
        while (*p == ' ') p++;
        const char *e = p;
        while (*e && *e != ' ') e++;
        if (e == p) break;
        if (k-- == 0) return season_uefa_code(comp, p, (int)(e - p));
        p = e;
    }
    return 0;
}

/* ---- The cascade ---- */

/* The paths of the entry codes, from the group stage down. The Champions League's group stage is in both. */
static const int CL_CHAMPIONS[] = {SEASON_UEFA_CL_G, SEASON_UEFA_CL_PO_CP, SEASON_UEFA_CL_Q3_CP, SEASON_UEFA_CL_Q2_CP,
                                   SEASON_UEFA_CL_Q1, SEASON_UEFA_CL_PR};
static const int CL_LEAGUE[] = {SEASON_UEFA_CL_G, SEASON_UEFA_CL_PO_LP, SEASON_UEFA_CL_Q3_LP, SEASON_UEFA_CL_Q2_LP};
static const int EL_MAIN[] = {SEASON_UEFA_EL_G, SEASON_UEFA_EL_PO, SEASON_UEFA_EL_Q3, SEASON_UEFA_EL_Q2,
                              SEASON_UEFA_EL_Q1, SEASON_UEFA_EL_PR};
static const int ECL_MAIN[] = {SEASON_UEFA_ECL_G, SEASON_UEFA_ECL_PO, SEASON_UEFA_ECL_Q3, SEASON_UEFA_ECL_Q2,
                               SEASON_UEFA_ECL_Q1};

static int count_code(char comp, int code)
{
    int n = 0;
    for (int i = 0; i < n_placed; i++) n += placed[i].comp == comp && placed[i].code == code && placed[i].club;
    return n;
}

static int better(const Placed *a, const Placed *b)
{
    return a->rank != b->rank ? a->rank < b->rank : a->order < b->order;
}

/* The index of path p (of n codes) that holds code, or -1. */
static int path_index(const int *p, int n, int code)
{
    for (int i = 0; i < n; i++)
        if (p[i] == code) return i;
    return -1;
}

/* The path of a placed club. */
static void path_of(const Placed *c, const int **p, int *n)
{
    if (c->comp == 'e') *p = EL_MAIN, *n = 6;
    else if (c->comp == 'k') *p = ECL_MAIN, *n = 5;
    else if (path_index(CL_LEAGUE, 4, c->code) > 0 || (c->code == SEASON_UEFA_CL_G && !c->champion))
        *p = CL_LEAGUE, *n = 4;
    else *p = CL_CHAMPIONS, *n = 6;
}

/* The lowest round of a path that the row plays (a target of new clubs above 0, or a round with clubs). */
static int lowest(char comp, const int *p, int n, const int *target)
{
    int low = 0;
    for (int i = 0; i < n; i++)
        if (target[p[i]] > 0 || count_code(comp, p[i])) low = i;
    return low;
}

/* Moves the club i one round down its path. At the lowest round of the path it loses its place. */
static void move_down(int i, const int *target)
{
    const int *p;
    int n;
    path_of(&placed[i], &p, &n);
    int at = path_index(p, n, placed[i].code), low = lowest(placed[i].comp, p, n, target);
    if (at >= 0 && at < low) {
        placed[i].code = p[at + 1];
        return;
    }
    wr32(g_mem, placed[i].club + 0x1db, 0xffffffffu);
    placed[i].club = 0;
}

static const SeasonUefa *cascade_row;   /* the row of assign_all, for the fillers of the cascade */

/* Slice 24: the filler of a Europa League gap from 2021/22. The access list then gives Europa League places only
 * to the best-ranked associations (ranks 16 and below have places "", their clubs go to the Conference League).
 * A vacancy (the title holder places, a suspended nation) moves the best-ranked cup winners up one round, and the
 * gap at the bottom goes to the cup winner of the best-ranked association with no Europa League place (the notes
 * of the access list rows: in 2021/22 Cyprus, rank 16; in 2022/23 Serbia and Czech Republic, 16 and 17). The cup
 * winner as in step 4 (0x9047e2: the winner, else the runner-up), else the first club with no place. Returns 0
 * in a season where every rank has places (to 2020/21): the original's filler then applies. */
static uint32_t el_cup_filler(uint32_t self, uint32_t records, int count, int entries, int *rank)
{
    const SeasonUefa *r = cascade_row;
    for (int e = 0; r && e < entries && !rt_unwinding; e++) {
        uint32_t ent = self + 0x12u * (uint32_t)e, nation = rd32(g_mem, ent + 4);
        const char *pl = season_uefa_access_places(r->season, 'e', e + 1);
        if (!pl || *pl || nation_suspended(r, nation)) continue;
        uint32_t club = cup_club(nation, 0x1e, 0);
        if (!club || has_place(club)) club = cup_club(nation, 0x1e, 1);
        if (!club || has_place(club)) club = pick(self, records, count, rd32(g_mem, ent), 1, 1);
        if (club && !has_place(club)) {
            *rank = e + 1;
            return club;
        }
    }
    return 0;
}

static void cascade(char comp, const int *p, int n, const int *target, uint32_t self, uint32_t records, int count,
                    int entries)
{
    for (int at = 0; at < n && !rt_unwinding; at++) {
        int code = p[at];
        for (int guard = 0; guard < MAX_PLACED && count_code(comp, code) > target[code]; guard++) {
            int worst = -1;
            for (int i = 0; i < n_placed; i++)
                if (placed[i].club && placed[i].comp == comp && placed[i].code == code &&
                    (worst < 0 || better(&placed[worst], &placed[i])))
                    worst = i;
            if (worst < 0) break;
            move_down(worst, target);
        }
        for (int guard = 0; guard < MAX_PLACED && count_code(comp, code) < target[code]; guard++) {
            /* The best-ranked club of the rounds below: of the Champions Path first for the group stage. */
            int best = -1;
            for (int pass = 0; pass < 2 && best < 0; pass++) {
                const int *q = p;
                int m = n;
                if (comp == 'c' && code == SEASON_UEFA_CL_G) q = pass ? CL_LEAGUE : CL_CHAMPIONS, m = pass ? 4 : 6;
                else if (pass) break;
                for (int i = 0; i < n_placed; i++) {
                    if (!placed[i].club || placed[i].comp != comp) continue;
                    int k = path_index(q, m, placed[i].code);
                    if (k > path_index(q, m, code) && (best < 0 || better(&placed[i], &placed[best]))) best = i;
                }
            }
            if (best >= 0) {
                placed[best].code = code;
                continue;
            }
            /* No club below: the first club with no place of the best-ranked nations. */
            uint32_t club = 0;
            int rank = 0;
            if (comp == 'e') club = el_cup_filler(self, records, count, entries, &rank);
            for (int e = 0; e < entries && !club && !rt_unwinding; e++) {
                if (nation_suspended(cascade_row, rd32(g_mem, self + 0x12u * (uint32_t)e + 4))) continue;
                club = pick(self, records, count, rd32(g_mem, self + 0x12u * (uint32_t)e), 1, 1);
                rank = e + 1;
            }
            if (!club) break;
            int before = n_placed;
            add(club, comp, code, rank, 0);
            if (n_placed == before) break;
        }
    }
}

/* ---- The wraps ---- */

void o_00904210(void);
void o_00904580(void);
void o_00904e30(void);

/* The row of the seeding year, when it has an access list. */
static const SeasonUefa *access_row(uint32_t self)
{
    const SeasonUefa *r = season_uefa_row((int16_t)rd16(g_mem, self + 0x439));
    return r && season_uefa_access_places(r->season, 'c', 1) ? r : NULL;
}

static void assign_all(uint32_t self, uint32_t records, int count, const SeasonUefa *r)
{
    int s = r->season, entries = (int8_t)g_mem[self + 0x438];
    if (entries > MAX_NATIONS) entries = MAX_NATIONS;
    n_placed = 0;
    cascade_row = r;
    /* 1. Champions League places by league position. */
    for (int e = 0; e < entries && !rt_unwinding; e++) {
        uint32_t ent = self + 0x12u * (uint32_t)e;
        const char *pl = season_uefa_access_places(s, 'c', e + 1);
        int n = nation_suspended(r, rd32(g_mem, ent + 4)) ? 0 : count_tokens(pl);
        g_mem[ent + 0xc] = (uint8_t)n;
        g_mem[ent + 0xe] = 0;
        for (int k = 0; k < n && !rt_unwinding; k++)
            add(pick(self, records, count, rd32(g_mem, ent), k + 1, 0), 'c', token_code('c', pl, k), e + 1, k == 0);
    }
    /* 2. The Champions League title holder. */
    uint32_t holder = rt_unwinding ? 0 : history_club(KEY_CL, 0);
    const char *h = season_uefa_access_rule(s, 'h');
    if (holder && h && !rt_unwinding && !club_suspended(r, holder))
        add(holder, 'c', token_code('c', h, 0), rank_of_nation(self, entries, rd32(g_mem, holder + 0x53)), 1);
    /* 3. The Europa League title holder. */
    uint32_t el_holder = rt_unwinding ? 0 : history_club(KEY_UC, 0);
    const char *x = season_uefa_access_rule(s, 'X'), *hh = season_uefa_access_rule(s, 'H');
    if (el_holder && !rt_unwinding && !club_suspended(r, el_holder)) {
        int rank = rank_of_nation(self, entries, rd32(g_mem, el_holder + 0x53));
        if (x) add(el_holder, 'c', token_code('c', x, 0), rank, 0);
        else if (hh) add(el_holder, 'e', token_code('e', hh, 0), rank, 0);
    }
    /* Slice 24: the Conference League title holder (the history winner of its record), before the league places
     * as the other holders: a Europa League group place from 2022/23 ("Y" rows) or a Conference League place
     * ("K"). With a place already (a Champions League place) it keeps that one. */
    ecl_id = r->ecl_round_count > 0 ? season_uefa_ecl_id() : -1;
    uint32_t ecl_holder = ecl_id >= 0 && !rt_unwinding ? history_club_of(ecl_id, 0) : 0;
    const char *y = season_uefa_access_rule(s, 'Y'), *kk = season_uefa_access_rule(s, 'K');
    if (ecl_holder && (y || kk) && !rt_unwinding && !club_suspended(r, ecl_holder)) {
        int rank = rank_of_nation(self, entries, rd32(g_mem, ecl_holder + 0x53)), had = has_place(ecl_holder);
        if (y) add(ecl_holder, 'e', token_code('e', y, 0), rank, 0);
        else add(ecl_holder, 'k', token_code('k', kk, 0), rank, 0);
        fprintf(stderr, "recomp: uefa: Conference League holder %.50s: %s\n", win_str(ecl_holder + 4),
                had ? "has a place already" : y ? "a Europa League place" : "a Conference League place");
    }
    /* Slice 25 (Q540, 2024/25 on): the European Performance Spots. The two nations with the best season
     * coefficient of the season before (nation record +0x108, a double: roll_coefficients 0x903b50 writes the
     * points of that season divided by the clubs entered there, before these places) each get one more league
     * phase place, for their best-placed club with no place yet. At a new game in a moved season the shift runs
     * too (0x903b6f tests the year 2001) with no clubs entered, so +0x108 is 0 for every nation (0x903c2a) and
     * the season before is the database's last value, now at +0x100 (0x903be8..0x903bee). It
     * comes after the title holders and before the Europa League places, so the clubs below move down one place
     * (CL regulations 2024/25, Art. 3.07 and 3.08). */
    if (r->cl_league && !rt_unwinding) {
        int best[2] = {-1, -1};
        double val[2] = {0, 0};
        uint32_t field = 0x100;
        for (int e = 0; e < entries; e++) {
            uint32_t rec = rd32(g_mem, self + 0x12u * (uint32_t)e + 4);
            double v = 0;
            if (readable(rec)) memcpy(&v, g_mem + rec + 0x108, sizeof v);
            if (v > 0) field = 0x108;
        }
        for (int e = 0; e < entries; e++) {
            uint32_t ent = self + 0x12u * (uint32_t)e, rec = rd32(g_mem, ent + 4);
            if (!readable(rec) || nation_suspended(r, rd32(g_mem, ent + 4))) continue;
            double v;
            memcpy(&v, g_mem + rec + field, sizeof v);
            if (best[0] < 0 || v > val[0]) best[1] = best[0], val[1] = val[0], best[0] = e, val[0] = v;
            else if (best[1] < 0 || v > val[1]) best[1] = e, val[1] = v;
        }
        for (int k = 0; k < 2 && best[k] >= 0 && !rt_unwinding; k++) {
            uint32_t ent = self + 0x12u * (uint32_t)best[k];
            uint32_t club = pick(self, records, count, rd32(g_mem, ent), 1, 1);
            if (rt_unwinding || !readable(club)) continue;
            add(club, 'c', SEASON_UEFA_CL_G, best[k] + 1, 0);
            g_mem[ent + 0xc]++;
            fprintf(stderr, "recomp: uefa: European Performance Spot %d/%02d: rank %d (%.3f at +0x%x), %.40s\n", s,
                    (s + 1) % 100, best[k] + 1, val[k], field, win_str(club + 4));
        }
    }
    /* 4. Europa League places: the cup winner, the league places, the league cup winner. 5. Fair play. */
    int fair[3] = {-1, -1, -1};
    if (season_uefa_access_rule(s, 'f') && entries > 3)
        for (int k = 0; k < 3 && !rt_unwinding; k++) {
            uint32_t a[1] = {(uint32_t)entries};
            int v = (int)call_cdecl(F_RANDOM, 1, a);
            if (v < 0 || v >= entries || v == fair[0] || v == fair[1]) k--;
            else fair[k] = v;
        }
    for (int e = 0; e < entries && !rt_unwinding; e++) {
        uint32_t ent = self + 0x12u * (uint32_t)e, nation = rd32(g_mem, ent + 4);
        const char *pl = season_uefa_access_places(s, 'e', e + 1);
        if (nation_suspended(r, nation)) {
            g_mem[ent + 0xd] = 0;
            continue;
        }
        int n = count_tokens(pl), extra = e == fair[0] || e == fair[1] || e == fair[2];
        g_mem[ent + 0xd] = (uint8_t)n;
        int league_cup = (nation == rd32(g_mem, LEAGUE_ENGLAND) && season_uefa_access_league_cup(s, "England")) ||
                         (nation == rd32(g_mem, LEAGUE_FRANCE) && season_uefa_access_league_cup(s, "France"));
        for (int k = 0; k < n + extra && !rt_unwinding; k++) {
            int code = k < n ? token_code('e', pl, k) : SEASON_UEFA_EL_Q1;
            uint32_t club = 0;
            if (k == 0) {
                club = cup_club(nation, 0x1e, 0);
                if (has_place(club)) club = cup_club(nation, 0x1e, 1);
            } else if (k == n - 1 && league_cup) {
                club = cup_club(nation, 0x22, 0);
            }
            if (!club || has_place(club)) club = pick(self, records, count, rd32(g_mem, ent), 1, 1);
            add(club, 'e', code, e + 1, 0);
        }
    }
    if (rt_unwinding) return;
    /* Slice 24, part b: the Conference League places (its title holder is placed with the other holders). */
    for (int e = 0; ecl_id >= 0 && e < entries && !rt_unwinding; e++) {
        uint32_t ent = self + 0x12u * (uint32_t)e, nation = rd32(g_mem, ent + 4);
        const char *pl = season_uefa_access_places(s, 'k', e + 1);
        if (nation_suspended(r, nation)) continue;
        int n = count_tokens(pl), el = count_tokens(season_uefa_access_places(s, 'e', e + 1));
        int league_cup = nation == rd32(g_mem, LEAGUE_ENGLAND) && season_uefa_access_ecl_league_cup(s, "England");
        for (int k = 0; k < n && !rt_unwinding; k++) {
            uint32_t club = 0;
            if (k == 0 && el == 0) {
                club = cup_club(nation, 0x1e, 0);
                if (has_place(club)) club = cup_club(nation, 0x1e, 1);
            } else if (k == n - 1 && league_cup) {
                club = cup_club(nation, 0x22, 0);
            }
            if (!club || has_place(club)) club = pick(self, records, count, rd32(g_mem, ent), 1, 1);
            add(club, 'k', token_code('k', pl, k), e + 1, 0);
        }
    }
    if (rt_unwinding) return;
    /* 6. The cascade to the round sizes of the row. */
    int target[SEASON_UEFA_CODE_SLOTS];
    season_uefa_new_clubs(r, 'c', target);
    cascade('c', CL_CHAMPIONS, 6, target, self, records, count, entries);
    cascade('c', CL_LEAGUE, 4, target, self, records, count, entries);
    season_uefa_new_clubs(r, 'e', target);
    cascade('e', EL_MAIN, 6, target, self, records, count, entries);
    if (ecl_id >= 0 && !rt_unwinding) {
        season_uefa_new_clubs(r, 'k', target);
        cascade('k', ECL_MAIN, 5, target, self, records, count, entries);
    }
    if (rt_unwinding) return;
    int n_cl = 0, n_el = 0, n_ecl = 0;
    for (int i = 0; i < n_placed; i++) {
        uint32_t c = placed[i].club;
        if (!c) continue;
        g_mem[c + 0x1df] = (uint8_t)placed[i].code;
        int rank = rank_of_nation(self, entries, rd32(g_mem, c + 0x53));
        if (rank <= entries) g_mem[self + 0x12u * (uint32_t)(rank - 1) + 0x11]++;
        if (placed[i].comp == 'c') n_cl++;
        else if (placed[i].comp == 'k') n_ecl++;
        else n_el++;
    }
    /* Slice 24: the clubs of the club table with a UEFA place from a suspended nation (0 when the rule holds). */
    int n_susp = 0;
    uint32_t clubs = rd32(g_mem, CLUBS);
    int n_clubs = (int32_t)rd32(g_mem, CLUB_COUNT);
    for (int i = 0; r->suspended && *r->suspended && readable(clubs) && i < n_clubs; i++) {
        uint32_t c = clubs + CLUB_SIZE * (uint32_t)i, id = rd32(g_mem, c + 0x1db);
        if ((id == rd32(g_mem, KEY_CL) || id == rd32(g_mem, KEY_UC) || (ecl_id >= 0 && id == (uint32_t)ecl_id)) &&
            club_suspended(r, c))
            n_susp++;
    }
    fprintf(stderr, "recomp: uefa: access list %d/%02d: %d Champions League and %d Europa League places\n", s,
            (s + 1) % 100, n_cl, n_el);
    if (ecl_id >= 0)
        fprintf(stderr, "recomp: uefa: access list %d/%02d: %d Conference League places\n", s, (s + 1) % 100, n_ecl);
    if (r->suspended && *r->suspended)
        fprintf(stderr, "recomp: uefa: suspended %s: %d clubs with a place\n", r->suspended, n_susp);
    /* For the tests: England's places (the England entry's nation, LEAGUE_ENGLAND). */
    int en_cl = 0, en_g = 0, en_el = 0;
    for (int i = 0; i < n_placed; i++)
        if (placed[i].club && rd32(g_mem, placed[i].club + 0x53) == rd32(g_mem, LEAGUE_ENGLAND)) {
            if (placed[i].comp == 'c') en_cl++, en_g += placed[i].code == SEASON_UEFA_CL_G;
            else if (placed[i].comp == 'e') en_el++;
        }
    fprintf(stderr, "recomp: uefa: England %d/%02d: %d Champions League places, %d in the group stage, %d Europa "
            "League places\n", s, (s + 1) % 100, en_cl, en_g, en_el);
}

/* Uefa_Seeding::assign_champions_league_places, thiscall (records, short count) ret 8. */
RT_WRAP(00904210, uefa_assign_cl)
{
    uint32_t self = cpu.ecx;
    const SeasonUefa *r = access_row(self);
    if (!r) {
        o_00904210();
        return;
    }
    uint32_t records = rt_arg(0);
    int count = (int16_t)rt_arg(1);
    assign_all(self, records, count, r);
    if (!rt_unwinding) rt_ret_stdcall(0, 2);
}

/* Uefa_Seeding::assign_uefa_cup_places, thiscall (records, short count) ret 8: given by the wrap above. */
RT_WRAP(00904580, uefa_assign_uc)
{
    if (!access_row(cpu.ecx)) {
        o_00904580();
        return;
    }
    rt_ret_stdcall(0, 2);
}

/* Uefa_Seeding::assign_intertoto_places, thiscall (records, short count) ret 8: none. The volunteer list goes
 * as 0x904fc0..0x904fe1 frees it. */
RT_WRAP(00904e30, uefa_assign_uic)
{
    uint32_t self = cpu.ecx;
    if (!access_row(self)) {
        o_00904e30();
        return;
    }
    uint32_t list = rd32(g_mem, self + 0x43b);
    if (readable(list)) {
        uint32_t a[1] = {list};
        call_cdecl(F_FREE, 1, a);
        if (rt_unwinding) return;
    }
    wr32(g_mem, self + 0x43b, 0);
    g_mem[self + 0x43f] = 0;
    rt_ret_stdcall(0, 2);
}
