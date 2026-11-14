/* The 2022 World Cup UEFA qualifying with Nations League paths (0.6.0 slice 28, season_nl.h). Research and
 * evidence: docs/research/2026-10-18-wc-europe-paths.md ("wc doc") and 2026-10-18-nations-league.md.
 *
 * The real 2022 qualifying played ten groups from March to November 2021 (group winners qualify), then three
 * paths in March 2022: the ten runners-up and the two best 2020-21 Nations League group winners that were not
 * in the top two of their group, single-match semi-finals and finals; the path winners qualify. The class
 * wc_europe_league (vtable 0x970cc4) holds the campaign year 2020 for it (wc doc 1) and plays its groups from
 * September 2020, which would meet the Nations League matchdays, and a two-leg play-off of runners-up in
 * November 2021 (slice 27's shape, E1 to E3). In a season game this file gives the 2020 campaign:
 *   - the real 2021 matchdays: a wrap of the template 0x9185c0 (wc doc 2), as season_nl_euro.c does for the
 *     Euro qualifying;
 *   - the paths: a wrap of create_play_off_stage 0x919910 (wc doc 4) with a Cup_Stage of 12, link mode 1, two
 *     single-match rounds on 24 and 29 March 2022. Only the three final winners get status 1 (0x51af95), so only
 *     they reach the World Cup, through the class's slot 17 (add_qualified_team 0x92e740 at 0x918a84). Slice 27's
 *     date shift puts the World Cup seeding in late May 2022 (wc doc 5), after the path finals;
 *   - no European runner-up for the Asian play-off: 0x91def0 reads stage[+0x101], which does not exist yet on
 *     the Asian stage 12 day (about 27 October 2021) when the groups end in November (wc doc 0, point 3). */
#include "win.h"
#include "season.h"
#include "season_comps.h"
#include "season_intl.h"
#include "season_nl.h"
#include "season_wcq26.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WCQ_VTABLE 0x00970cc4u          /* wc_europe_league (written at 0x91823a) */
#define CAMPAIGN 2020                   /* +0x40 for the 2022 World Cup (wc doc 1) */
#define GAME_MALLOC 0x00944e46u
#define GAME_FREE 0x009452cau
#define GAME_NEW 0x00944cf1u
#define TABLE_SORT 0x006827d0u
#define CUP_STAGE_NEW 0x00522e00u       /* as at 0x919d53 */
#define CUP_START 0x0051c800u           /* as at 0x919dfd */
#define F_RECORD 0x4u
#define F_STAGES 0xcu
#define F_STAGE 0x30u
#define F_YEAR 0x40u
#define F_TABLE 0xb1u
#define F_TEAMS 0x3eu
#define F_LAST_GROUP 0xffu              /* 8: ten groups (wc doc 3) */
#define F_RUNNERS_UP 0x100u             /* the runners-up table stage (0x91a140) */
#define F_PLAYOFF 0x101u
#define ROW_SIZE 0x3bu
#define ROW_POINTS 0x19u
#define LEAGUE_ROUND 0x41u
#define ROUND_SIZE 0x68u
#define ROUNDS 10
#define MAX_GROUPS 16
#define PATHS 3
#define TEAMS (4 * PATHS)

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_NL_LOG");
        on = v && *v == '1';
    }
    return on;
}

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(GAME_MALLOC, 1, a);
    if (readable(p)) memset(g_mem + p, 0, size);
    return readable(p) ? p : 0;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (p) call_cdecl(GAME_FREE, 1, a);
}

/* The matchdays and paths of the 2022 campaign, or NULL: a season game with the date shift of slice 27 (wc doc
 * 5: without it the World Cup seeds in December 2021, before the paths), the class, the campaign 2020. */
static const SeasonNlQualifying *moved(uint32_t self)
{
    if (!season_comps_on() || !season_intl_on() || !readable(self) || rd32(g_mem, self) != WCQ_VTABLE ||
        (int16_t)rd16(g_mem, self + F_YEAR) != CAMPAIGN)
        return NULL;
    for (int i = 0; i < season_nl_qualifying_count; i++)
        if (season_nl_qualifying[i].world_cup && season_nl_qualifying[i].euro == CAMPAIGN + 2)
            return &season_nl_qualifying[i];
    return NULL;
}

static uint32_t stage(uint32_t self, int i)
{
    if (i < 0) return self;
    uint32_t stages = rd32(g_mem, self + F_STAGES);
    return readable(stages) ? rd32(g_mem, stages + 4u * (uint32_t)i) : 0;
}

/* The club of place `pos` (from 0) of a sorted table, or 0. */
static uint32_t table_club(uint32_t st, int pos)
{
    if (!readable(st)) return 0;
    uint32_t table = rd32(g_mem, st + F_TABLE);
    int teams = (int16_t)rd16(g_mem, st + F_TEAMS);
    return readable(table) && pos < teams ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

void o_009185c0(void);

/* create_schedule, thiscall (stage as a byte, short *rounds, short *code, int *is_league), ret 0x10 (wc doc 2):
 * the group stages (up to +0xff) on the real 2021 matchdays, the play-off (+0x101) as two single-match rounds.
 * Other campaigns and objects: the original (with slice 27's E1 site inside it). */
RT_WRAP(009185c0, nl_wc_template)
{
    if (season_wcq26_europe_template()) return; /* slice 29: the 2026 cycle (season_wcq26.c) */
    uint32_t self = cpu.ecx, sp = cpu.esp;
    const SeasonNlQualifying *q = moved(self);
    int st = (int8_t)rd8(g_mem, sp + 4u);
    int group = q && st <= (int8_t)rd8(g_mem, self + F_LAST_GROUP), playoff = q && st == (int8_t)rd8(g_mem, self + F_PLAYOFF);
    if (!group && !playoff) {
        o_009185c0();
        return;
    }
    uint32_t out_rounds = rd32(g_mem, sp + 8u), out_code = rd32(g_mem, sp + 0xcu), out_league = rd32(g_mem, sp + 0x10u);
    if (readable(out_league)) wr32(g_mem, out_league, group ? 1u : 0u);
    if (readable(out_rounds)) wr16(g_mem, out_rounds, group ? ROUNDS : 2u);
    if (readable(out_code)) wr16(g_mem, out_code, group ? (uint32_t)(0x3e9 + st) : 0u);
    uint32_t rec = game_malloc(group ? ROUNDS * LEAGUE_ROUND : 2u * ROUND_SIZE);
    if (rt_unwinding) return;
    if (rec && group) {
        for (int i = 0; i < ROUNDS && !rt_unwinding; i++) season_nl_league_round(rec, i, q->md[i], CAMPAIGN);
        if (logging() && st == -1 && !rt_unwinding)
            fprintf(stderr, "recomp: season nl: world cup %d qualifying groups from %d March %d\n", q->euro,
                    q->md[0].day, CAMPAIGN + q->md[0].year);
    } else if (rec) {
        season_nl_path_round(rec, 0, q->po_sf, CAMPAIGN, TEAMS);
        if (!rt_unwinding) season_nl_path_round(rec, 1, q->po_final, CAMPAIGN, TEAMS / 2);
    }
    if (rt_unwinding) return;
    rt_ret_stdcall(rec, 4);
}

/* The paths. Seeds: the six best runners-up of the runners-up table stage[+0x100] (0x91a140), at home. The
 * others: the four other runners-up and the two best 2020-21 Nations League group winners (by the overall
 * ranking) that are not in the top two of a group; without the Nations League tables, the two best thirds by
 * points. Path p: seed 2p v the unseeded 5 - 2p, seed 2p + 1 v the unseeded 4 - 2p (the real draw of the
 * unseeded is not made: decision for Max). */
static void build_paths(uint32_t self, const SeasonNlQualifying *q)
{
    int groups = (int8_t)rd8(g_mem, self + F_LAST_GROUP) + 2, nations = (int)rd32(g_mem, 0xae2350u);
    if (groups > MAX_GROUPS || nations <= 0 || nations > 4096) return;
    char *top2 = calloc((size_t)nations, 1);
    if (!top2) return;
    uint32_t third[MAX_GROUPS] = {0};
    int third_points[MAX_GROUPS] = {0};
    for (int g = 0; g < groups; g++) {
        uint32_t st = stage(self, g - 1), z[1] = {0};
        if (!readable(st)) continue;
        call_this(TABLE_SORT, st, 1, z);
        if (rt_unwinding) { free(top2); return; }
        for (int r = 0; r < 2; r++) {
            int n = season_nl_nation_of_club(table_club(st, r));
            if (n >= 0 && n < nations) top2[n] = 1;
        }
        third[g] = table_club(st, 2);
        uint32_t table = rd32(g_mem, st + F_TABLE);
        third_points[g] = readable(table) ? (int16_t)rd16(g_mem, table + 2u * ROW_SIZE + ROW_POINTS) : 0;
    }
    /* The runners-up in the order of their table (sorted). */
    uint32_t ru_stage = stage(self, (int8_t)rd8(g_mem, self + F_RUNNERS_UP)), z[1] = {0};
    uint32_t ru[MAX_GROUPS] = {0};
    int n_ru = 0;
    if (readable(ru_stage)) {
        call_this(TABLE_SORT, ru_stage, 1, z);
        if (rt_unwinding) { free(top2); return; }
        for (int r = 0; r < groups && n_ru < 10; r++) {
            uint32_t c = table_club(ru_stage, r);
            if (c) ru[n_ru++] = c;
        }
    }
    if (n_ru < 10) {                                     /* no table: row 1 of each group */
        n_ru = 0;
        for (int g = 0; g < groups && n_ru < 10; g++) {
            uint32_t c = table_club(stage(self, g - 1), 1);
            if (c) ru[n_ru++] = c;
        }
    }
    uint32_t extra[2] = {0};
    int n_extra = 0;
    SeasonNlRow rows[NL_MAX_NATIONS];
    int n = season_nl_held_results(CAMPAIGN, rows);
    if (rt_unwinding) { free(top2); return; }
    for (int i = 0; i < n && n_extra < 2; i++) {
        int id = rows[i].nation;
        if (rows[i].pos != 1 || id < 0 || id >= nations || top2[id]) continue;
        uint32_t t = season_nl_nation_team(id);
        if (rt_unwinding) { free(top2); return; }
        if (t) extra[n_extra++] = t;
    }
    for (int k = n_extra; k < 2; k++) {                  /* the best thirds by points */
        int best = -1;
        for (int g = 0; g < groups; g++)
            if (third[g] && (best < 0 || third_points[g] > third_points[best])) best = g;
        if (best < 0) break;
        extra[n_extra++] = third[best];
        third[best] = 0;
    }
    free(top2);
    if (n_ru < 10 || n_extra < 2) {
        fprintf(stderr, "recomp: season nl: world cup %d: no paths (%d runners-up, %d more)\n", q->euro, n_ru, n_extra);
        return;
    }
    uint32_t unseeded[6] = {ru[6], ru[7], ru[8], ru[9], extra[0], extra[1]};
    uint32_t list = game_malloc(4u * TEAMS), out = game_malloc(8);
    if (rt_unwinding || !list || !out) return;
    for (int p = 0; p < PATHS; p++) {
        uint32_t pair[4] = {ru[2 * p], unseeded[5 - 2 * p], ru[2 * p + 1], unseeded[4 - 2 * p]};
        for (int k = 0; k < 4; k++) wr32(g_mem, list + 4u * (uint32_t)(4 * p + k), pair[k]);
        if (logging()) {
            fprintf(stderr, "recomp: season nl: world cup %d path %c:", q->euro, 'A' + p);
            for (int k = 0; k < 4; k++) {
                int id = season_nl_nation_of_club(pair[k]);
                fprintf(stderr, " %s;", id >= 0 ? win_str(rd32(g_mem, 0xae23a8u) + (uint32_t)id * 0x122u + 4u) : "none");
            }
            fputc('\n', stderr);
        }
    }
    int po = (int8_t)rd8(g_mem, self + F_PLAYOFF);
    uint32_t xa[4] = {(uint32_t)po, out, out + 4u, 0};
    uint32_t tmpl = call_this(rd32(g_mem, rd32(g_mem, self) + 0x3cu), self, 4, xa);
    if (rt_unwinding) return;
    uint32_t na[1] = {0xb2};
    uint32_t cup = tmpl ? call_cdecl(GAME_NEW, 1, na) : 0;
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, TEAMS, list, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + F_RECORD), tmpl,
                           CAMPAIGN, (uint32_t)po, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 1, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    game_free(list);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding || !cup) return;
    wr32(g_mem, rd32(g_mem, self + F_STAGES) + 4u * (uint32_t)po, cup);
    call_this(CUP_START, cup, 1, z);
    if (rt_unwinding) return;
    wr32(g_mem, self + F_STAGE, (uint32_t)po);
}

void o_00919910(void);

/* create_play_off_stage, thiscall (), from advance_stage 0x9196b5 when the groups end (wc doc 4). */
RT_WRAP(00919910, nl_wc_paths)
{
    if (season_wcq26_europe_paths()) return; /* slice 29: the 2026 cycle (season_wcq26.c) */
    uint32_t self = cpu.ecx;
    const SeasonNlQualifying *q = moved(self);
    if (!q) {
        o_00919910();
        return;
    }
    build_paths(self, q);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

void o_0091def0(void);

/* get_asia_play_off_team, thiscall (), returns a team (ret at 0x91df6c, 0x91df8d), from slice 27's site at
 * 0x913e42: no European team for the 2020 campaign (the real 2022 cycle had none; slice 27's partner code
 * takes the CONMEBOL team or its fallback). */
RT_WRAP(0091def0, nl_wc_asia_partner)
{
    if (season_wcq26_europe_no_asia()) return; /* slice 29: the 2026 cycle (season_wcq26.c) */
    if (!moved(cpu.ecx)) {
        o_0091def0();
        return;
    }
    rt_ret_stdcall(0, 0);
}
