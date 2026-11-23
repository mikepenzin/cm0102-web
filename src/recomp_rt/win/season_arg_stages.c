/* The title stages of the Argentine Primera in its zone seasons (0.7.2 league sizes, gap row 48; research notes of
 * 15 November 2026: LPF reglamentos 2025 and 2026, RSSSF arg2016, arg2025, arg2026, es and en Wikipedia; code notes
 * docs/research/2026-11-15-arg-zone-stages.md).
 *
 * Real life:
 * - 2025 and 2026 (rows 2024 and 2025): each half has two zone tables of 15. The first 8 of each zone play a knockout
 *   of one match a round: 1A v 8B, 4B v 5A, 2B v 7A, 3A v 6B (one side of the bracket), 1B v 8A, 4A v 5B, 2A v 7B,
 *   3B v 6A (the other), a fixed bracket, the better zone place at home, the final at a neutral ground. A level match
 *   goes to extra time and penalties, except the Apertura 2025 before its final: penalties at once. The winner is the
 *   champion of the half.
 * - 2016 (row 2015): two zone tables of 15, then a final of the zone winners and a match of the runners-up, one match
 *   each at a neutral ground. The final's winner is the champion.
 *
 * How (the model is BelThird, 0x4216f0..0x4242e0):
 * - The zone tables are League_Stages of 15 clubs with no dates (League_Stage::League_Stage 0x68b100, as the period
 *   tables of BelThird::find_or_create_period_stage 0x422e8d..0x422ed4). ArgPrm::add_result 0x405c50 gives each league
 *   match of a half to the zone tables of that half (League::add_result 0x685d30 with match +0x42 the stage index and
 *   no stats, as 0x422ca6); a table updates only the row of its own club (0x685faf..0x6864cf).
 * - The knockouts are Cup_Stages (Cup_Stage::Cup_Stage 0x522e00, tree mode 1: Cup_Stage::map_fixture_tree 0x5241e0
 *   puts entrant i in tie i / 2, home first, and the winner of tie t in tie t / 2 of the next round). The round table
 *   is made here (0x68 bytes a round, the fields of docs/research/2026-10-17-cup-round-record.md): codes 20, 120, 130,
 *   150, flags 0x200 (fixed bracket) and 0x3 (extra time and penalties) or 0x1 (penalties), the home rule bit 0x1 of
 *   +0xb, the final at venue 4 (the nation's ground, 0x85c727). Cup_Stage +0x90 0x522360 decides the home club of a
 *   tie: here the better seed (the zone place).
 * - The stage array (+0xc, +0x2c stages of the season, +0x30 the last one made): [0] the Clausura, [1] the aggregate
 *   table, then the zone tables, then the knockouts. Save, load, the destructor and start_new_season walk 0..+0x30
 *   (League save 0x6891c0, load 0x687b10, 0x404500, 0x4057b5), so a stage is in the array before +0x30 counts it.
 *   League::update_for_date 0x6847c0 closes the season only when +0x30 = +0x2c - 1 and every stage is complete
 *   (0x6849aa..0x6849e3); before that it calls advance_stage, the empty 0x5a8f60, where the last stages are made.
 *   The Apertura's knockout is made after the update of the day its table is complete.
 * - The champion of each half: League::record_season_honours 0x684640 reads the clubs of rows 0, 1 and 2
 *   (0x68464b..0x684663); for the call they are the winner, the finalist and the row 2 club of the table.
 * The base game and RECOMP_LEAGUE_SIZES=off: no stage is added and every wrap passes to the original. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_sizes.h"
#include "season_sizes_arg.h"
#include "season_sizes_arg_second.h"
#include "season_league.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_LEAGUE_STAGE 0x0068b100u      /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_ADD_RESULT 0x00685d30u        /* League::add_result, thiscall (match, update_stats) */
#define F_SORT_TABLE 0x006827d0u        /* League::sort_table, thiscall (rows or 0) */
#define F_DRAW_DATE 0x00521e60u         /* cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u        /* cdecl (table, round, day, month, year+, weekday, a7, year, venue) */
#define NO_WEEKDAY 0xffffffffu

#define GAME_DATE 0x00ae2c90u           /* s16 day of the year, +2 s16 year */
#define ENTRIES 0x00b63d60u
#define ENTRY_SIZE 0x48u
#define ENTRY_COUNT 35
#define ENTRY_UPDATE_DAY 0x15u
#define COMPS_MAX 256

#define COMP_RECORD 4u
#define STAGES 0xcu
#define STAGE_COUNT 0x2cu               /* s32: the stages of the season */
#define STAGE_LAST 0x30u                /* s32: the last stage made */
#define COMP_MEETINGS 0x3cu
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define STAGE_TYPE 0x43u                /* 1 Cup_Stage, 2 League_Stage */
#define STAGE_INDEX 0x44u
#define COMP_MATCHES 0xa7u              /* League: s16 matches of a club; Cup_Stage: the team entries (6 bytes) */
#define COMP_DATES 0xa9u
#define COMP_TABLE 0xb1u
#define CUP_COMPLETE 0xb1u              /* Cup: set when the last round is played (Cup::check_complete 0x521e00) */
#define COMP_TEMPLATE 0xbau
#define ROW_SIZE 0x3bu
#define ROW_POSITION 4u
#define ROW_PLAYED 0x11u
#define MATCH_STAGE 0x42u               /* s8: the stage of a match, 0xff the league */
#define DATE_SIZE 0x41u
#define ROUND_SIZE 0x68u
#define CLUB_NAME 4u

#define FIRST_ZONE 2                    /* stages[2..]: the zone tables */
#define ZONE_MAX 15
#define TABLE_MAX 60
#define KO_CLUBS 16
#define NAME_APERTURA 0x434             /* "Opening Stage" (ArgPrm::create_schedule 0x4045af) */
#define NAME_CLAUSURA 0x435             /* "Closing Stage" (0x404d9f) */
#define NAME_ZONE_A1 0x43d              /* "Phase 1 Group A", parent 0x449 (comp_util_get_parent_stage_id 0x4b01d0) */
#define NAME_ZONE_A2 0x445              /* "Phase 2 Group A", parent 0x44a */
#define NAME_GROUP_A 0x41f              /* "Group A", parent 0x41c */
#define NAME_CHAMPIONSHIP 0x46b         /* "Championship" */
#define NAME_PROMOCION 0x45b            /* the name of the second division's promotion ties (0x407ad8) */
#define KEY_SECOND 0x9cf658u            /* the record id of the second division (0x40576b) */
#define CLUB_DIVISION 0x57u
#define COMP_UP 0xbeu                   /* s8: the direct promotion places */
#define PROMO_DAYS 9                    /* the draw tomorrow, the first legs in 3 days, the second legs 4 days later */
#define CODE_R16 20
#define CODE_QF 120
#define CODE_SF 130
#define CODE_FINAL 150
#define CODE_PLAYOFF 160
#define VENUE_HOME 0
#define VENUE_NEUTRAL 4

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    const char *v = getenv("RECOMP_SIZE_LOG");
    return v && *v && strcmp(v, "0");
}

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

/* ---- The plan of a season's stages ---- */

typedef struct {
    int kind, season;
    int count;                          /* the stages of the season */
    int zones;                          /* the zone tables (2 a half, or 2) */
    int ko_a, ko_c;                     /* the knockout of the Apertura and of the Clausura, -1 none */
    int final_at, third_at;             /* the final and the runners-up match (2016), -1 none */
} Plan;

static int plan_of(uint32_t obj, Plan *p)
{
    memset(p, 0, sizeof *p);
    p->ko_a = p->ko_c = p->final_at = p->third_at = -1;
    p->kind = season_arg_stage_kind(obj, &p->season);
    if (p->kind == SEASON_ARG_KO_ZONES) {
        p->zones = 4;
        p->ko_a = FIRST_ZONE + 4;
        p->ko_c = FIRST_ZONE + 5;
        p->count = FIRST_ZONE + 6;
    } else if (p->kind == SEASON_ARG_KO_FINAL) {
        p->zones = 2;
        p->final_at = FIRST_ZONE + 2;
        p->third_at = FIRST_ZONE + 3;
        p->count = FIRST_ZONE + 4;
    } else if (p->kind == SEASON_ARG_KO_TABLE) {
        p->ko_a = FIRST_ZONE;
        p->ko_c = FIRST_ZONE + 1;
        p->count = FIRST_ZONE + 2;
    } else if (p->kind == SEASON_ARG_KO_PROMO) {
        p->count = FIRST_ZONE + 2;          /* the two ties of the promoción */
    } else {
        p->count = FIRST_ZONE;
    }
    return p->kind != SEASON_ARG_KO_NONE;
}

static uint32_t stage_at(uint32_t prm, int i);

/* No zone tables: the 2027 form (two knockouts) or the promoción (two ties). The row's kind when the sizes give one
 * (with changing rules off a 2005 start keeps row 2005 in every edition, so not the year); else the stages made (a
 * game loaded with the sizes off); else, before any is made, the edition's year (edition Y plays row Y). */
static int no_zones_kind(uint32_t obj, int last)
{
    int k = season_arg_stage_kind(obj, NULL);
    if (k == SEASON_ARG_KO_PROMO || k == SEASON_ARG_KO_TABLE) return k;
    for (int i = FIRST_ZONE; i <= last; i++) {
        uint32_t s = stage_at(obj, i);
        int name = s && g_mem[s + STAGE_TYPE] == 1 ? (int16_t)rd16(g_mem, s + 0x3a) : 0;
        if (name == NAME_PROMOCION) return SEASON_ARG_KO_PROMO;
        if (name == NAME_APERTURA || name == NAME_CLAUSURA) return SEASON_ARG_KO_TABLE;
    }
    return (int16_t)rd16(g_mem, obj + COMP_YEAR) <= SEASON_ARG_PROMO_LAST ? SEASON_ARG_KO_PROMO : SEASON_ARG_KO_TABLE;
}

/* The plan of a Primera whose stages exist, read from the stages alone (a loaded game, also with the sizes off: only
 * this code sets +0x2c above 2): the League_Stages from index 2 are the zone tables (4: two a half, 2: one
 * tournament, 0: the 2027 form), the knockouts come after them. +0x2c is the stages still planned. */
static int plan_live(uint32_t obj, Plan *p)
{
    if (!readable(obj) || g_mem[obj + STAGE_INDEX] != 0xff) return 0;
    int count = (int32_t)rd32(g_mem, obj + STAGE_COUNT), last = (int32_t)rd32(g_mem, obj + STAGE_LAST);
    if (count <= FIRST_ZONE || count > 16 || strcmp(season_change_file(obj), "arg_prm")) return 0;
    memset(p, 0, sizeof *p);
    p->ko_a = p->ko_c = p->final_at = p->third_at = -1;
    while (FIRST_ZONE + p->zones <= last) {
        uint32_t s = stage_at(obj, FIRST_ZONE + p->zones);
        if (!s || g_mem[s + STAGE_TYPE] != 2) break;
        p->zones++;
    }
    p->kind = p->zones == 4 ? SEASON_ARG_KO_ZONES : p->zones == 2 ? SEASON_ARG_KO_FINAL : p->zones != 0 ?
              SEASON_ARG_KO_NONE : no_zones_kind(obj, last);
    p->count = count;
    return p->kind != SEASON_ARG_KO_NONE;
}

/* The knockout stage of a name id (the Apertura's, the Clausura's, the 2016 final), or 0. */
static uint32_t ko_named(uint32_t obj, const Plan *p, int name)
{
    int last = (int32_t)rd32(g_mem, obj + STAGE_LAST);
    for (int i = FIRST_ZONE + p->zones; i <= last; i++) {
        uint32_t s = stage_at(obj, i);
        if (s && g_mem[s + STAGE_TYPE] == 1 && (int16_t)rd16(g_mem, s + 0x3a) == name) return s;
    }
    return 0;
}

static uint32_t stage_at(uint32_t prm, int i)
{
    uint32_t stages = rd32(g_mem, prm + STAGES);
    if (!readable(stages) || i < 0 || i > (int32_t)rd32(g_mem, prm + STAGE_LAST)) return 0;
    uint32_t s = rd32(g_mem, stages + 4u * (uint32_t)i);
    return readable(s) ? s : 0;
}

/* ---- Zone tables ---- */

/* A League_Stage of the clubs of a zone with no dates: it takes the results that add_result gives it. */
static uint32_t zone_stage(uint32_t self, const uint32_t *club, int z, int index, int name, int matches)
{
    uint32_t list = season_sizes_game_malloc(4u * (uint32_t)z);
    if (!list) return 0;
    for (int i = 0; i < z; i++) wr32(g_mem, list + 4u * (uint32_t)i, club[i]);
    uint32_t sz[1] = {0xee};
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) {
        season_sizes_game_free(list);
        return 0;
    }
    /* As the period tables of BelThird (0x422e8d..0x422ed4), with the parent's rule bytes, tie-break codes and
     * places (+0xc2..+0xc4, +0xc5, +0xbe) and the matches of a zone club. */
    uint32_t a[22] = {self, (uint32_t)z, list, 1, rd32(g_mem, self + COMP_RECORD), 0, 0, g_mem[self + 0xc2],
                      g_mem[self + 0xc3], g_mem[self + 0xc4], self + 0xc5, self + 0xbe,
                      (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR), (uint32_t)index, (uint32_t)name, 0, 1, 0, 2,
                      (uint32_t)matches, 0, 2};
    call_this(F_LEAGUE_STAGE, stage, 22, a);
    season_sizes_game_free(list);
    return rt_unwinding ? 0 : stage;
}

void season_arg_stages_made(uint32_t self)
{
    Plan p;
    int any = plan_of(self, &p);
    uint32_t old = rd32(g_mem, self + STAGES);
    if (!readable(old)) return;
    wr32(g_mem, self + STAGE_COUNT, (uint32_t)p.count);
    if (!any) return;
    if (p.kind == SEASON_ARG_KO_TABLE || p.kind == SEASON_ARG_KO_PROMO) {
        /* No zone tables; the array holds the two knockouts or the two promoción ties (made later). */
        uint32_t arr = season_sizes_game_malloc(4u * (uint32_t)p.count);
        if (!arr) {
            wr32(g_mem, self + STAGE_COUNT, FIRST_ZONE);
            return;
        }
        for (int i = 0; i < p.count; i++)
            wr32(g_mem, arr + 4u * (uint32_t)i, i < FIRST_ZONE ? rd32(g_mem, old + 4u * (uint32_t)i) : 0);
        wr32(g_mem, self + STAGES, arr);
        season_sizes_game_free(old);
        if (logging())
            fprintf(stderr, "recomp: league sizes: arg_prm stages %d: row %d, 0 zone tables of 0 (%d matches), %d stages "
                            "in the season\n", (int16_t)rd16(g_mem, self + COMP_YEAR), p.season,
                    (int16_t)rd16(g_mem, self + COMP_MATCHES), p.count);
        return;
    }
    uint32_t a[ZONE_MAX], b[ZONE_MAX];
    int z = season_arg_zone_lists(self, a, b);
    if (z < 2 || z > ZONE_MAX) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: no zones for the zone tables; no title stages\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR));
        wr32(g_mem, self + STAGE_COUNT, FIRST_ZONE);
        return;
    }
    /* The array of the season's stages (the constructor's holds 2, 0x4042b2): the Clausura and the aggregate move. */
    uint32_t arr = season_sizes_game_malloc(4u * (uint32_t)p.count);
    if (!arr) {
        wr32(g_mem, self + STAGE_COUNT, FIRST_ZONE);
        return;
    }
    for (int i = 0; i < p.count; i++) wr32(g_mem, arr + 4u * (uint32_t)i, i < FIRST_ZONE ? rd32(g_mem, old + 4u * (uint32_t)i) : 0);
    wr32(g_mem, self + STAGES, arr);
    season_sizes_game_free(old);
    int matches = (int16_t)rd16(g_mem, self + COMP_MATCHES);
    for (int k = 0; k < p.zones; k++) {
        int half = k / 2, zone = k % 2, index = FIRST_ZONE + k;
        int name = p.kind == SEASON_ARG_KO_FINAL ? NAME_GROUP_A + zone : (half ? NAME_ZONE_A2 : NAME_ZONE_A1) + zone;
        uint32_t s = zone_stage(self, zone ? b : a, z, index, name, matches);
        if (rt_unwinding) return;
        if (!s) {
            /* No table: the season has the stages made so far and no title stages. */
            wr32(g_mem, self + STAGE_COUNT, (uint32_t)index);
            fprintf(stderr, "recomp: league sizes: arg_prm %d: zone table %d not made; no title stages\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR), index);
            return;
        }
        wr32(g_mem, arr + 4u * (uint32_t)index, s);
        wr32(g_mem, self + STAGE_LAST, (uint32_t)index);
    }
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_prm stages %d: row %d, %d zone tables of %d (%d matches), %d stages "
                        "in the season\n", (int16_t)rd16(g_mem, self + COMP_YEAR), p.season, p.zones, z, matches,
                p.count);
}

/* ArgPrm::add_result 0x405c50 (thiscall (match, update_stats), ret 8, returns 1): League::add_result for the match,
 * then again with match +0x42 = 1 and no stats for the aggregate table (0x405c65..0x405c8f). A knockout match goes to
 * its stage only; a league match of a zone season goes to the zone tables of its half too. */
void o_00405c50(void);
RT_WRAP(00405c50, arg_add_result)
{
    uint32_t self = cpu.ecx, match = rt_arg(0), stats = rt_arg(1);
    Plan p;
    if (!readable(match) || !plan_live(self, &p)) {
        o_00405c50();
        return;
    }
    int at = (int8_t)g_mem[match + MATCH_STAGE];
    int first_ko = FIRST_ZONE + p.zones;
    if (at >= first_ko) {
        if (logging()) {
            /* The test (league_size_run.py) checks the bracket and the home clubs from these lines. */
            uint32_t h = rd32(g_mem, match + 0x1c), w = rd32(g_mem, match + 0x20);
            fprintf(stderr, "recomp: league sizes: arg_prm knockout match %d: stage %d round %d: %u-%u\n",
                    (int16_t)rd16(g_mem, self + COMP_YEAR), at, (int16_t)rd16(g_mem, match + 0x30),
                    readable(h) ? rd32(g_mem, h) : 0u, readable(w) ? rd32(g_mem, w) : 0u);
        }
        uint32_t a[2] = {match, stats};
        uint32_t r = call_this(F_ADD_RESULT, self, 2, a);
        if (!rt_unwinding) rt_ret_stdcall(r, 2);
        return;
    }
    o_00405c50();
    if (rt_unwinding) return;
    g_mem[match + MATCH_STAGE] = (uint8_t)at;
    int half = at == -1 ? 0 : at == 0 ? 1 : -1;
    if (half < 0 || !p.zones || (p.kind == SEASON_ARG_KO_FINAL && half)) return;
    int last = (int32_t)rd32(g_mem, self + STAGE_LAST);
    Cpu saved = cpu;
    for (int zone = 0; zone < 2; zone++) {
        int index = FIRST_ZONE + 2 * half + zone;
        uint32_t s = stage_at(self, index);
        if (!s || index > last) continue;
        g_mem[match + MATCH_STAGE] = (uint8_t)index;
        uint32_t a[2] = {match, 0};
        call_this(F_ADD_RESULT, self, 2, a);
        if (rt_unwinding) return;
        uint32_t z[1] = {0};
        call_this(F_SORT_TABLE, s, 1, z);
        if (rt_unwinding) return;
    }
    g_mem[match + MATCH_STAGE] = (uint8_t)at;
    cpu = saved;
}

/* ---- Knockouts ---- */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
static int year_days(int y) { return leap(y) ? 366 : 365; }

static void day_month(int doy, int year, int *day, int *month)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11) {
        int n = len[m] + (m == 1 && leap(year));
        if (doy < n) break;
        doy -= n;
        m++;
    }
    *day = doy + 1;
    *month = m;
}

static void date_plus(int n, int *doy, int *year)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    d += n;
    while (d >= year_days(y)) d -= year_days(y), y++;
    *doy = d;
    *year = y;
}

static int days_until(int doy, int year)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2), n = 0;
    while (y < year) n += year_days(y) - d, d = 0, y++;
    return n + doy - d;
}

/* The days to the update day of the Primera's table entry (its season change), or -1. */
static int days_to_change(uint32_t obj)
{
    uint32_t buf[COMPS_MAX];
    for (int e = 0; e < ENTRY_COUNT; e++) {
        int n = season_change_entry_comps(e, buf, COMPS_MAX);
        for (int i = 0; i < n; i++) {
            if (buf[i] != obj) continue;
            int upd = (int16_t)rd16(g_mem, ENTRIES + (uint32_t)e * ENTRY_SIZE + ENTRY_UPDATE_DAY);
            int today = (int16_t)rd16(g_mem, GAME_DATE), y = (int16_t)rd16(g_mem, GAME_DATE + 2);
            int left = upd - today;
            if (left < 0) left += year_days(y);
            return left;
        }
    }
    return -1;
}

/* The days to the first date of a league's template, or -1. */
static int days_to_first_date(uint32_t league)
{
    uint32_t t = readable(league) ? rd32(g_mem, league + COMP_TEMPLATE) : 0;
    int n = readable(league) ? (int16_t)rd16(g_mem, league + COMP_DATES) : 0, best = -1;
    for (int i = 0; readable(t) && i < n; i++) {
        int doy = (int16_t)rd16(g_mem, t + (uint32_t)i * DATE_SIZE), y = (int16_t)rd16(g_mem, t + (uint32_t)i * DATE_SIZE + 2);
        int left = days_until(doy, y);
        if (left > 0 && (best < 0 || left < best)) best = left;
    }
    return best;
}

static void set_dates(uint32_t table, int round, int draw_in, int match_in, uint32_t venue, uint32_t year)
{
    int doy, y, day, month;
    date_plus(draw_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t d[7] = {table, (uint32_t)round, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY,
                     year};
    call_cdecl(F_DRAW_DATE, 7, d);
    if (rt_unwinding) return;
    date_plus(match_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t m[9] = {table, (uint32_t)round, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY,
                     1, year, venue};
    call_cdecl(F_MATCH_DATE, 9, m);
}

/* A knockout stage at index of n clubs (16 or 2) in the list order of the tree (club 2i at home to club 2i + 1 in
 * round 0), seeds the zone places, a round every step days from today. pens: penalties at once before the final. */
static uint32_t make_cup(uint32_t self, int index, const uint32_t *club, const uint8_t *seed, int n, int rounds,
                         uint32_t table, int name);

static uint32_t ko_stage(uint32_t self, int index, const uint32_t *club, const uint8_t *seed, int n, int code_one,
                         int name, int step, int pens, int neutral_from)
{
    int rounds = n == KO_CLUBS ? 4 : 1;
    static const int codes[4] = {CODE_R16, CODE_QF, CODE_SF, CODE_FINAL};
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t table = season_sizes_game_malloc(ROUND_SIZE * (uint32_t)rounds);
    if (!table) return 0;
    memset(g_mem + table, 0, ROUND_SIZE * (size_t)rounds);
    for (int r = 0; r < rounds; r++) {
        uint32_t t = table + ROUND_SIZE * (uint32_t)r;
        int last = r == rounds - 1, teams = n >> r, neutral = last || r >= neutral_from;
        wr16(g_mem, t + 7, (uint16_t)(rounds == 1 ? code_one : codes[r]));
        wr16(g_mem, t + 9, (uint16_t)r);
        wr16(g_mem, t + 0xb, neutral ? 0 : 1);                 /* the home rule: the better seed (0x51e8b2) */
        wr16(g_mem, t + 0xd, (uint16_t)(0x200 | (pens && !last ? 0x1 : 0x3)));
        g_mem[t + 0x17] = (uint8_t)(r == 0 && rounds > 1 ? 4 : 6);
        wr16(g_mem, t + 0x18, (uint16_t)teams);
        wr16(g_mem, t + 0x1a, (uint16_t)(teams / 2));
        wr16(g_mem, t + 0x1c, (uint16_t)(r == 0 ? n : 0));
        wr16(g_mem, t + 0x1e, 0);
        g_mem[t + 0x21] = 1;
        /* The draw the day after the round before, the matches step - 1 days later. */
        set_dates(table, r, r == 0 ? 1 : step * r, step * (r + 1) - 1, neutral ? VENUE_NEUTRAL : VENUE_HOME, year);
        if (rt_unwinding) return 0;
    }
    return make_cup(self, index, club, seed, n, rounds, table, name);
}

/* The Cup_Stage of a round table (which it frees) at index: its list, Cup_Stage::Cup_Stage, the first round. */
static uint32_t make_cup(uint32_t self, int index, const uint32_t *club, const uint8_t *seed, int n, int rounds,
                         uint32_t table, int name)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t list = season_sizes_game_malloc(4u * (uint32_t)n), seeds = season_sizes_game_malloc((uint32_t)n);
    uint32_t sz[1] = {0xb2};
    uint32_t stage = list && seeds ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return 0;
    if (!readable(stage)) {
        season_sizes_game_free(list);
        season_sizes_game_free(seeds);
        season_sizes_game_free(table);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        wr32(g_mem, list + 4u * (uint32_t)i, club[i]);
        g_mem[seeds + (uint32_t)i] = seed[i];
    }
    uint32_t a[15] = {self, (uint32_t)n, list, (uint32_t)rounds, rd32(g_mem, self + COMP_RECORD), table, year,
                      (uint32_t)index, 1, (uint32_t)name, 0x14, 1, 0, 0, seeds};
    call_this(F_CUP_STAGE, stage, 15, a);
    season_sizes_game_free(list);
    season_sizes_game_free(seeds);
    season_sizes_game_free(table);
    if (rt_unwinding) return 0;
    wr32(g_mem, rd32(g_mem, self + STAGES) + 4u * (uint32_t)index, stage);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return 0;
    wr32(g_mem, self + STAGE_LAST, (uint32_t)index);           /* only now: save reads stages[0..+0x30] */
    return stage;
}

/* The clubs of a zone table in the order of their places (rows by +4). */
static int zone_order(uint32_t s, uint32_t *club)
{
    if (!readable(s)) return 0;
    uint32_t z[1] = {0};
    call_this(F_SORT_TABLE, s, 1, z);
    if (rt_unwinding) return 0;
    uint32_t rows = rd32(g_mem, s + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, s + COMP_CLUBS);
    if (!readable(rows) || n < 1 || n > TABLE_MAX) return 0;
    int pos[TABLE_MAX];
    for (int i = 0; i < n; i++) {
        club[i] = rd32(g_mem, rows + (uint32_t)i * ROW_SIZE);
        pos[i] = g_mem[rows + (uint32_t)i * ROW_SIZE + ROW_POSITION];
    }
    for (int i = 1; i < n; i++) {
        uint32_t c = club[i];
        int v = pos[i], j = i;
        for (; j > 0 && pos[j - 1] > v; j--) club[j] = club[j - 1], pos[j] = pos[j - 1];
        club[j] = c;
        pos[j] = v;
    }
    return n;
}

/* Every row of a league or zone table has played its matches. */
static int table_done(uint32_t league)
{
    uint32_t rows = readable(league) ? rd32(g_mem, league + COMP_TABLE) : 0;
    int n = readable(league) ? (int16_t)rd16(g_mem, league + COMP_CLUBS) : 0;
    int want = readable(league) ? (int16_t)rd16(g_mem, league + COMP_MATCHES) : 0;
    if (!readable(rows) || n < 1 || want < 1) return 0;
    for (int i = 0; i < n; i++)
        if (g_mem[rows + (uint32_t)i * ROW_SIZE + ROW_PLAYED] < want) return 0;
    return 1;
}

static void log_ko(uint32_t self, const char *what, int index, const uint32_t *club, const uint8_t *seed, int n,
                   int step)
{
    if (!logging()) return;
    fprintf(stderr, "recomp: league sizes: arg_prm %s %d: stage %d drawn on day %d of %d, a round every %d days:",
            what, (int16_t)rd16(g_mem, self + COMP_YEAR), index, (int16_t)rd16(g_mem, GAME_DATE),
            (int16_t)rd16(g_mem, GAME_DATE + 2), step);
    for (int i = 0; i + 1 < n; i += 2)
        fprintf(stderr, " %u:%d-%u:%d", rd32(g_mem, club[i]), seed[i], rd32(g_mem, club[i + 1]), seed[i + 1]);
    fprintf(stderr, ";");
    for (int i = 0; i + 1 < n; i += 2)
        fprintf(stderr, " %.30s v %.30s,", win_str(club[i] + CLUB_NAME), win_str(club[i + 1] + CLUB_NAME));
    fprintf(stderr, "\n");
}

/* The knockout of a half from its two zone tables: 1A v 8B, 4B v 5A, 2B v 7A, 3A v 6B, 1B v 8A, 4A v 5B, 2A v 7B,
 * 3B v 6A (LPF reglamento 2025 and 2026: ties 1 to 8 and quarter-finals 1 v 8, 2 v 7, 3 v 6, 4 v 5, semi-finals of
 * the first and fourth against the second and third). limit: the days it may take. */
static int build_table_ko(uint32_t self, const Plan *p, int half, int limit);

/* Art. 14.1.2 of the LPF reglamento 2025 and 2026: a club in the relegation zone does not play the Clausura's
 * knockout, and the next club of its zone (or table) takes its place. The game knows the clubs that go down when the
 * Clausura's tables are complete (season_arg_relegated: the averages and the annual table at their end). The rule is
 * not stated for the Apertura, nor yet for 2027 on: those seasons keep it from 2026, the last known (lead decision).
 * Drops them from an ordered list of n clubs; returns the count left. */
static int bar_relegated(uint32_t self, uint32_t *club, int n, const char *what)
{
    uint32_t down[8];
    int nd = season_arg_relegated(self, down, 8);
    if (rt_unwinding) return 0;
    int k = 0;
    for (int i = 0; i < n; i++) {
        int out = 0;
        for (int j = 0; j < nd; j++) out |= club[i] == down[j];
        if (out) {
            if (logging())
                fprintf(stderr, "recomp: league sizes: arg_prm %d: %u %.30s of the %s goes down; not in the Clausura "
                        "knockout\n", (int16_t)rd16(g_mem, self + COMP_YEAR), rd32(g_mem, club[i]),
                        win_str(club[i] + CLUB_NAME), what);
            continue;
        }
        club[k++] = club[i];
    }
    return k;
}

static int build_ko(uint32_t self, const Plan *p, int half, int limit)
{
    if (p->kind == SEASON_ARG_KO_TABLE) return build_table_ko(self, p, half, limit);
    uint32_t a[TABLE_MAX], b[TABLE_MAX];
    int na = zone_order(stage_at(self, FIRST_ZONE + 2 * half), a);
    if (rt_unwinding) return 0;
    int nb = zone_order(stage_at(self, FIRST_ZONE + 2 * half + 1), b);
    if (rt_unwinding) return 0;
    if (half) {
        na = bar_relegated(self, a, na, "zone A");
        if (rt_unwinding) return 0;
        nb = bar_relegated(self, b, nb, "zone B");
        if (rt_unwinding) return 0;
    }
    int index = (int32_t)rd32(g_mem, self + STAGE_LAST) + 1;
    if (na < 8 || nb < 8) return 0;
    static const struct { char zone; int place; } order[KO_CLUBS] = {
        {'A', 1}, {'B', 8}, {'B', 4}, {'A', 5}, {'B', 2}, {'A', 7}, {'A', 3}, {'B', 6},
        {'B', 1}, {'A', 8}, {'A', 4}, {'B', 5}, {'A', 2}, {'B', 7}, {'B', 3}, {'A', 6}};
    uint32_t club[KO_CLUBS];
    uint8_t seed[KO_CLUBS];
    for (int i = 0; i < KO_CLUBS; i++) {
        club[i] = order[i].zone == 'A' ? a[order[i].place - 1] : b[order[i].place - 1];
        seed[i] = (uint8_t)order[i].place;
    }
    /* A round a week, as the real dates; closer when the next stage comes sooner (at least every 3 days). */
    int step = limit >= 0 && limit < 29 ? (limit - 1) / 4 : 7;
    if (step < 3) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: %d days for the knockout of the %s; it is not played\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), limit, half ? "Clausura" : "Apertura");
        return 0;
    }
    int pens = !half && season_sizes_row_season(self, "arg_prm") == 2024;   /* Apertura 2025: penalties first */
    uint32_t s = ko_stage(self, index, club, seed, KO_CLUBS, 0, half ? NAME_CLAUSURA : NAME_APERTURA, step, pens, 99);
    if (rt_unwinding || !s) return 0;
    log_ko(self, half ? "knockout clausura" : "knockout apertura", index, club, seed, KO_CLUBS, step);
    return 1;
}

/* 2027 on: the first 16 of the half's table, 1 v 16, 8 v 9, 4 v 13, 5 v 12, 2 v 15, 7 v 10, 3 v 14, 6 v 11 (the
 * sources give 1 v 16, 2 v 15 and so on, and no bracket: the bracket of the seeds), the better place at home in the
 * round of 16 and the quarter-finals, the semi-finals and the final at a neutral ground. */
static int build_table_ko(uint32_t self, const Plan *p, int half, int limit)
{
    uint32_t t[TABLE_MAX];
    int n = zone_order(half ? stage_at(self, 0) : self, t);
    if (rt_unwinding) return 0;
    if (half) n = bar_relegated(self, t, n, "table");
    if (rt_unwinding || n < KO_CLUBS) return 0;
    static const int place[KO_CLUBS] = {1, 16, 8, 9, 4, 13, 5, 12, 2, 15, 7, 10, 3, 14, 6, 11};
    uint32_t club[KO_CLUBS];
    uint8_t seed[KO_CLUBS];
    for (int i = 0; i < KO_CLUBS; i++) {
        club[i] = t[place[i] - 1];
        seed[i] = (uint8_t)place[i];
    }
    int step = limit >= 0 && limit < 29 ? (limit - 1) / 4 : 7;
    if (step < 3) {
        fprintf(stderr, "recomp: league sizes: arg_prm %d: %d days for the knockout of the %s; it is not played\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), limit, half ? "Clausura" : "Apertura");
        return 0;
    }
    int index = (int32_t)rd32(g_mem, self + STAGE_LAST) + 1;
    if (!ko_stage(self, index, club, seed, KO_CLUBS, 0, half ? NAME_CLAUSURA : NAME_APERTURA, step, 0, 2) || rt_unwinding)
        return 0;
    log_ko(self, half ? "knockout clausura" : "knockout apertura", index, club, seed, KO_CLUBS, step);
    return 1;
}

/* 2016: the final of the zone winners and the match of the runners-up, one match each at a neutral ground. */
static int build_final(uint32_t self, const Plan *p, int limit)
{
    uint32_t a[TABLE_MAX], b[TABLE_MAX];
    int na = zone_order(stage_at(self, FIRST_ZONE), a);
    if (rt_unwinding) return 0;
    int nb = zone_order(stage_at(self, FIRST_ZONE + 1), b);
    if (rt_unwinding || na < 2 || nb < 2) return 0;
    int step = limit >= 0 && limit < 8 ? limit - 1 : 7;
    if (step < 2) return 0;
    uint32_t fin[2] = {a[0], b[0]}, thr[2] = {a[1], b[1]};
    uint8_t s1[2] = {1, 1}, s2[2] = {2, 2};
    int at = (int32_t)rd32(g_mem, self + STAGE_LAST) + 1;
    if (!ko_stage(self, at, fin, s1, 2, CODE_FINAL, NAME_CHAMPIONSHIP, step, 0, 0)) return 0;
    log_ko(self, "final", at, fin, s1, 2, step);
    if (!ko_stage(self, at + 1, thr, s2, 2, CODE_PLAYOFF, 0, step, 0, 0)) return 0;
    log_ko(self, "runners-up match", at + 1, thr, s2, 2, step);
    return 1;
}

/* The season plans want stages fewer: the season closes once the stages made are complete (0x6849aa..0x6849e3). */
static void fewer(uint32_t self, int want, const char *why)
{
    fprintf(stderr, "recomp: league sizes: arg_prm %d: %s; the season has %d stages\n",
            (int16_t)rd16(g_mem, self + COMP_YEAR), why, want);
    wr32(g_mem, self + STAGE_COUNT, (uint32_t)want);
}

/* ---- The promoción (rows 2002 to 2011) ---- */

static int ko_winners(uint32_t s, uint32_t *win, uint32_t *second);

/* The second division's object: g_comp_object_table at the key [0x9cf658]. */
static uint32_t second_obj(void)
{
    uint32_t objects = rd32(g_mem, 0xadadfcu);
    int32_t key = (int32_t)rd32(g_mem, KEY_SECOND);
    uint32_t o = readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
    return readable(o) ? o : 0;
}

/* A tie of two legs as the second division's (ArgSecond +0x3c, 0x407b4f..0x407b84): fixed bracket 0x200, the second
 * leg 4 days after the first, and on a level aggregate the home club of the second leg goes through with no extra
 * time, penalties or away goals (+0xf 0x804: 0x800 home side through, 0x4 away goals off; OR'd into the second leg,
 * docs/research/2026-10-17-cup-round-record.md). The tree puts entry 0 at home in the first leg, so the second
 * division's club comes first and the Primera's club, which had the sporting advantage, plays the second leg at home
 * (EN 2010-11: "The Primera División team (Team 1) played the second leg at home"). */
static uint32_t promo_tie(uint32_t self, int index, uint32_t second_club, uint32_t prm_club)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t table = season_sizes_game_malloc(ROUND_SIZE);
    if (!table) return 0;
    memset(g_mem + table, 0, ROUND_SIZE);
    wr16(g_mem, table + 7, 0);
    wr16(g_mem, table + 9, 0);
    wr16(g_mem, table + 0xb, 0);
    wr16(g_mem, table + 0xd, 0x200);
    wr16(g_mem, table + 0xf, 0x804);
    g_mem[table + 0x17] = 6;
    wr16(g_mem, table + 0x18, 2);
    wr16(g_mem, table + 0x1a, 1);
    wr16(g_mem, table + 0x1c, 2);
    wr16(g_mem, table + 0x1e, 0);
    g_mem[table + 0x20] = 0;
    g_mem[table + 0x21] = 2;
    g_mem[table + 0x22] = 4;
    set_dates(table, 0, 1, 3, VENUE_HOME, year);
    if (rt_unwinding) return 0;
    uint32_t club[2] = {second_club, prm_club};
    uint8_t seed[2] = {2, 1};
    return make_cup(self, index, club, seed, 2, 1, table, NAME_PROMOCION);
}

/* The end of a season of rows 2002 to 2011: the 17th of the averages against the second division's first place after
 * its direct places, the 18th against the next (EN 2010-11: the 3rd and 4th of the Primera B Nacional; the pairs of
 * the earlier seasons are not stated, lead decision). It waits, a day at a time (League::update_for_date calls
 * advance_stage each day while a stage is to come, 0x6849d1..0x6849e3), for the second division's table. A second
 * division that plays its class's stages (the 2001 edition, or the sizes off) plays its own ties (0x408910). */
static int promo_advance(uint32_t self, int left)
{
    uint32_t second = second_obj();
    int year = (int16_t)rd16(g_mem, self + COMP_YEAR);
    if (!second || (int32_t)rd32(g_mem, second + STAGE_COUNT) != 0) {
        fewer(self, FIRST_ZONE, "the second division plays its own promotion ties; no promocion of the Primera");
        return 1;
    }
    if (!table_done(second)) {
        if (left >= 0 && left < PROMO_DAYS) {
            fewer(self, FIRST_ZONE, "the second division's table is not complete in time; no promocion");
            return 1;
        }
        return 1;                           /* the next day */
    }
    if (left >= 0 && left < PROMO_DAYS) {
        fewer(self, FIRST_ZONE, "no days for the promocion");
        return 1;
    }
    uint32_t prm2[2], t[TABLE_MAX];
    int nt = zone_order(second, t);
    if (rt_unwinding) return 1;
    int up = (int8_t)g_mem[second + COMP_UP];
    if (season_arg_promo_primera(self, prm2) != 2 || rt_unwinding || up < 0 || up + 2 > nt) {
        if (!rt_unwinding) fewer(self, FIRST_ZONE, "no clubs for the promocion");
        return 1;
    }
    for (int k = 0; k < 2; k++) {
        uint32_t s = promo_tie(self, FIRST_ZONE + k, t[up + k], prm2[k]);
        if (rt_unwinding) return 1;
        if (!s) {
            fewer(self, (int32_t)rd32(g_mem, self + STAGE_LAST) + 1, "a promocion tie not made");
            return 1;
        }
        if (logging())
            fprintf(stderr, "recomp: league sizes: arg_prm promocion %d: stage %d drawn on day %d of %d: %u:%d-%u:%d "
                            "(the %d of the second division v the %d of the averages); %.30s v %.30s\n", year,
                    FIRST_ZONE + k, (int16_t)rd16(g_mem, GAME_DATE), (int16_t)rd16(g_mem, GAME_DATE + 2),
                    rd32(g_mem, t[up + k]), up + k + 1, rd32(g_mem, prm2[k]), 17 + k, up + k + 1, 17 + k,
                    win_str(t[up + k] + CLUB_NAME), win_str(prm2[k] + CLUB_NAME));
    }
    return 1;
}

int season_arg_promo_ties(uint32_t prm, uint32_t *win, uint32_t *lose)
{
    Plan p;
    if (!readable(prm) || !plan_live(prm, &p) || p.kind != SEASON_ARG_KO_PROMO) return 0;
    int last = (int32_t)rd32(g_mem, prm + STAGE_LAST), n = 0;
    for (int i = FIRST_ZONE; i <= last && n < 2; i++) {
        uint32_t s = stage_at(prm, i);
        if (!s || g_mem[s + STAGE_TYPE] != 1 || (int16_t)rd16(g_mem, s + 0x3a) != NAME_PROMOCION) continue;
        if (ko_winners(s, &win[n], &lose[n])) n++;
    }
    return n;
}

int season_arg_promo_results(uint32_t prm, uint32_t *down, uint32_t *up)
{
    uint32_t win[2], lose[2], rec = readable(prm) ? rd32(g_mem, prm + COMP_RECORD) : 0;
    int t = season_arg_promo_ties(prm, win, lose), n = 0;
    for (int i = 0; i < t; i++)
        if (rd32(g_mem, lose[i] + CLUB_DIVISION) == rec && rd32(g_mem, win[i] + CLUB_DIVISION) != rec) {
            down[n] = lose[i];
            up[n++] = win[i];
        }
    return n;
}

/* ---- The Torneo Reducido of the second division (item 291) ---- */

#define NAME_REDUCIDO 0x45a             /* the class's name of its Reducido (ArgSecond::create_schedule 0x4078a7) */
#define COMP_UP_PLAYOFF 0xbfu
#define ROW_STATUS 0x37u
#define REDUCIDO_MAX 16
#define COMP_DOWN_PLACES 0xc1u
#define VT_ARG_SECOND 0x009673d8u       /* ArgSecond's vtable (0x406e5e) */
#define CLASS_STAGES_SECOND 6           /* ArgSecond::ArgSecond 0x406c61 */
#define DAYS_ONE_MATCH 5                /* the draw tomorrow, the match in 3 days, the next draw after it */
#define DAYS_TWO_LEGS 9                 /* as PROMO_DAYS and one day for the next draw */

static int reducido_spec(uint32_t obj, SeasonArgReducido *s)
{
    if (!season_sizes_on() || !season_changing_rules() || !readable(obj) || !season_arg_second_one_table(obj)) return 0;
    int row = season_sizes_row_season(obj, "arg_second");
    const SeasonLeagueRule *l = row >= 0 ? season_league_row("arg_second", row) : NULL;
    if (!l || !season_arg_reducido_of(row, s) || season_arg_second_row_up(row) != 2) return 0;
    /* A zone row names its Reducido in the text only. An edition with no room for its dates plays the class's. */
    return s->zone_last ? l->zones == 2 && !season_sizes_kept(obj) : l->po_up == s->places;
}

/* The class always holds 6 stages (0x406c61) and a table with no stage 0, so any other count of an ArgSecond object is an
 * edition with a Reducido. Read from the object, not from the rules: a save made during a Reducido can load with the sizes
 * off (the same reason as plan_live for the Primera). */
int season_arg_reducido_marked(uint32_t obj)
{
    if (!readable(obj) || rd32(g_mem, obj) != VT_ARG_SECOND) return 0;
    int32_t n = (int32_t)rd32(g_mem, obj + STAGE_COUNT);
    return n >= 1 && n != CLASS_STAGES_SECOND;
}

/* The champion goes up directly (+0xbe 1) and the Reducido's winner takes the row's second place. The class's array holds
 * 6 stages (0x406c55), so an edition gets one slot for each tie. Only before any stage of the edition is made. */
void season_arg_reducido_rules(uint32_t obj)
{
    SeasonArgReducido s;
    if (!reducido_spec(obj, &s) || (int32_t)rd32(g_mem, obj + STAGE_LAST) >= 0) return;
    uint32_t block = season_sizes_game_malloc(4u * (uint32_t)s.stages);
    if (rt_unwinding || !block) return;
    memset(g_mem + block, 0, 4u * (size_t)s.stages);
    season_sizes_game_free(rd32(g_mem, obj + STAGES));
    wr32(g_mem, obj + STAGES, block);
    wr32(g_mem, obj + STAGE_COUNT, (uint32_t)s.stages);
    /* Zone rows: no place of the table moves by itself. The final and the Reducido send their winners up and
     * season_arg_reducido_advance marks the clubs that go down, zone by zone. */
    g_mem[obj + COMP_UP] = s.zone_last ? 0 : 1;
    g_mem[obj + COMP_UP_PLAYOFF] = s.zone_last ? 0 : (uint8_t)s.places;
    if (s.zone_last) g_mem[obj + COMP_DOWN_PLACES] = 0;
}

/* ---- Two zones in one table (item 291) ---- */

/* No source gives the clubs of each zone for every season, so the zones come from the club ids: the same clubs always make
 * the same zones, also after a load. */
int season_arg_second_zones(uint32_t obj, int n, int *a, int *b)
{
    SeasonArgReducido s;
    if (!reducido_spec(obj, &s) || !s.zone_last || n < 4) return 0;
    const SeasonLeagueRule *l = season_league_row("arg_second", season_sizes_row_season(obj, "arg_second"));
    *a = l && l->zone_size[0] + l->zone_size[1] == n ? l->zone_size[0] : n - n / 2;
    *b = n - *a;
    return 1;
}

/* The edition that is being played has its zones: its stage count says so, and that count is in the save. A save of 0.7.2
 * in a zone row, or an edition with no room for its dates, plays one table. */
int season_arg_second_zones_live(uint32_t obj, int n, int *a, int *b)
{
    return season_arg_reducido_marked(obj) && season_arg_second_zones(obj, n, a, b);
}

/* The zone (0 or 1) of each of n clubs. */
static void zones_of(const uint32_t *club, int n, int a, int *zone)
{
    int order[TABLE_MAX], sorted[TABLE_MAX];
    for (int i = 0; i < n; i++) {
        int j = i;
        for (; j > 0 && rd32(g_mem, club[order[j - 1]]) > rd32(g_mem, club[i]); j--) order[j] = order[j - 1];
        order[j] = i;
    }
    season_arg_second_zone_split(n, a, sorted);
    for (int i = 0; i < n; i++) zone[order[i]] = sorted[i];
}

static int zone_list_on;

uint32_t season_arg_second_zone_fixtures(uint32_t league)
{
    int n = readable(league) ? (int16_t)rd16(g_mem, league + COMP_CLUBS) : 0, a, b;
    if (!readable(league) || rd32(g_mem, league) != VT_ARG_SECOND || n > TABLE_MAX ||
        !season_arg_second_zones(league, n, &a, &b))
        return 0;
    static int round[SEASON_ARG_ZONE_ROUNDS][SEASON_ARG_ZONE_TIES][2];
    int rounds = season_arg_second_zone_plan(a, b, round), per = n / 2, dates = (int16_t)rd16(g_mem, league + COMP_DATES);
    uint32_t rows = rd32(g_mem, league + COMP_TABLE), club[TABLE_MAX];
    if (!rounds || rounds != dates || !readable(rows)) {
        fprintf(stderr, "recomp: league sizes: arg_second zones %d: %d dates for %d rounds: the class's fixtures\n",
                (int16_t)rd16(g_mem, league + COMP_YEAR), dates, rounds);
        return 0;
    }
    for (int i = 0; i < n; i++) {
        club[i] = rd32(g_mem, rows + (uint32_t)i * ROW_SIZE);
        if (!readable(club[i])) return 0;
    }
    int zone[TABLE_MAX], at[TABLE_MAX], ia = 0, ib = a;
    zones_of(club, n, a, zone);
    for (int i = 0; i < n; i++) at[zone[i] ? ib++ : ia++] = i;
    uint32_t sz[1] = {4u + 0x10u * (uint32_t)(rounds * per)};
    uint32_t block = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(block)) return 0;
    memset(g_mem + block, 0, sz[0]);
    wr32(g_mem, block, (uint32_t)(rounds * per));
    uint32_t list = block + 4;
    for (int r = 0; r < rounds; r++)
        for (int k = 0; k < per; k++) {
            uint32_t e = list + 0x10u * (uint32_t)(r * per + k);
            int h = round[r][k][0], w = round[r][k][1];
            uint32_t home = rd32(g_mem, club[at[h < 0 ? 0 : h]]), away = rd32(g_mem, club[at[w < 0 ? 0 : w]]);
            wr16(g_mem, e + 2, rd16(g_mem, league + COMP_YEAR));
            wr32(g_mem, e + 8, home);
            wr32(g_mem, e + 0xc, away);
        }
    zone_list_on = 1;
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second zones %d: %d and %d clubs, %d rounds\n",
                (int16_t)rd16(g_mem, league + COMP_YEAR), a, b, rounds);
    return list;
}

void season_arg_second_zone_fixtures_end(void) { zone_list_on = 0; }

/* An entry of the list with no match names one club twice (League::create_fixtures 0x683903 reads (clubs / 2) entries a
 * round): FIXTURE_MANAGER::add_fixture 0x5aa680 does not add it. The fixture holds the club ids at +0xc and +0x10
 * (0x6839b2, 0x6839c7). */
int season_arg_second_drop_fixture(uint32_t fixture)
{
    return zone_list_on && readable(fixture) && rd32(g_mem, fixture + 0xc) == rd32(g_mem, fixture + 0x10);
}

/* League::check_complete 0x689ad0 (thiscall (a, b), ret 8) counts one number of matches for every club (+0xa7,
 * 0x689b47..0x689b58). Two zones of different sizes play different numbers, so the table is complete only when each club
 * has played its own zone's. */
void o_00689ad0(void);
RT_WRAP(00689ad0, league_check_complete)
{
    uint32_t self = cpu.ecx;
    int n = readable(self) && rd32(g_mem, self) == VT_ARG_SECOND ? (int16_t)rd16(g_mem, self + COMP_CLUBS) : 0, a, b;
    if (n >= 4 && n <= TABLE_MAX && season_arg_second_zones_live(self, n, &a, &b)) {
        uint32_t rows = rd32(g_mem, self + COMP_TABLE), club[TABLE_MAX];
        int zone[TABLE_MAX];
        for (int i = 0; readable(rows) && i < n; i++) club[i] = rd32(g_mem, rows + (uint32_t)i * ROW_SIZE);
        if (readable(rows)) {
            zones_of(club, n, a, zone);
            for (int i = 0; i < n; i++)
                if (g_mem[rows + (uint32_t)i * ROW_SIZE + ROW_PLAYED] < 2 * ((zone[i] ? b : a) - 1)) {
                    rt_ret_stdcall(0, 2);
                    return;
                }
        }
    }
    o_00689ad0();
}

/* One tie at index. One match: the better placed club at home (first in the tree), and it goes through on a level score
 * (+0xd 0x800: Cup::create_tie_fixtures 0x51cd91..0x51cdce gives the fixture 0x800, home through, when the home seed
 * number is the lower; 2017-18 and 2022 sources). Two legs:
 * the better placed club plays the second leg at home and goes through on a level aggregate (+0xf 0x804, as promo_tie;
 * 2018-19 and 2022 sources), except in a final that goes to penalties (+0xf 7: extra time and penalties, no away goals;
 * 2015 and 2018-19 finals). */
static uint32_t reducido_tie(uint32_t self, int index, uint32_t better, uint32_t worse, int legs, int code, int pens,
                             int neutral)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t table = season_sizes_game_malloc(ROUND_SIZE);
    if (!table) return 0;
    memset(g_mem + table, 0, ROUND_SIZE);
    wr16(g_mem, table + 7, (uint16_t)code);
    /* A one-match final or relegation tie: extra time and penalties (2021, 2023 and 2024 sources). */
    wr16(g_mem, table + 0xd, legs == 2 ? 0x200 : pens ? 0x203 : 0xa00);
    wr16(g_mem, table + 0xf, legs == 2 ? (pens ? 0x7 : 0x804) : 0);
    g_mem[table + 0x17] = 6;
    wr16(g_mem, table + 0x18, 2);
    wr16(g_mem, table + 0x1a, 1);
    wr16(g_mem, table + 0x1c, 2);
    g_mem[table + 0x21] = (uint8_t)legs;
    g_mem[table + 0x22] = legs == 2 ? 4 : 0;
    set_dates(table, 0, 1, 3, neutral ? VENUE_NEUTRAL : VENUE_HOME, year);
    if (rt_unwinding) return 0;
    uint32_t club[2] = {legs == 2 ? worse : better, legs == 2 ? better : worse};
    uint8_t seed[2] = {legs == 2 ? 2 : 1, legs == 2 ? 1 : 2};
    return make_cup(self, index, club, seed, 2, 1, table, NAME_REDUCIDO);
}

/* A club goes down, as ArgSecond::create_promotion_cup_stage does for the worst averages (0x4086ad..0x4086ed). */
static void reducido_send_down(uint32_t self, uint32_t club)
{
    uint32_t rows = rd32(g_mem, self + COMP_TABLE), vt = rd32(g_mem, self);
    int n = (int16_t)rd16(g_mem, self + COMP_CLUBS);
    for (int r = 0; readable(rows) && r < n; r++)
        if (rd32(g_mem, rows + (uint32_t)r * ROW_SIZE) == club) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = 3;
    if (logging())
        fprintf(stderr, "recomp: league sizes: arg_second zones %d: club %u goes down\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), rd32(g_mem, club));
    uint32_t s2[5] = {club, 0xfffffffeu, 0x1e, 0x1e, 0x1e}, s1[5] = {club, 3, 0xffffffffu, 0, 0};
    call_this(rd32(g_mem, vt + 0x68), self, 5, s2);
    if (rt_unwinding) return;
    call_this(rd32(g_mem, vt + 0x40), self, 5, s1);
}

/* No Reducido this edition: the next club of the table takes the second place (status 0 goes up, 0x406839). */
static void reducido_none(uint32_t self, const uint32_t *table, int n, const char *why)
{
    uint32_t rows = rd32(g_mem, self + COMP_TABLE);
    SeasonArgReducido s;
    int a, b;
    /* Two zones and no stage made: the winner of each zone goes up and the zones send their clubs down. */
    if (reducido_spec(self, &s) && s.zone_last && (int32_t)rd32(g_mem, self + STAGE_LAST) < 0 && n <= TABLE_MAX &&
        season_arg_second_zones(self, n, &a, &b)) {
        int zone[TABLE_MAX], cnt[2] = {0, 0};
        uint32_t in[2][TABLE_MAX];
        zones_of(table, n, a, zone);
        for (int i = 0; i < n; i++) in[zone[i]][cnt[zone[i]]++] = table[i];
        for (int z = 0; z < 2; z++) {
            for (int r = 0; readable(rows) && r < n; r++)
                if (rd32(g_mem, rows + (uint32_t)r * ROW_SIZE) == in[z][0]) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = 0;
            for (int d = 0; d < s.down_each && cnt[z] - 1 - d >= 1; d++) {
                reducido_send_down(self, in[z][cnt[z] - 1 - d]);
                if (rt_unwinding) return;
            }
        }
        wr32(g_mem, self + STAGE_COUNT, 0);
        fprintf(stderr, "recomp: league sizes: arg_second reducido %d: %s; the zone winners go up\n",
                (int16_t)rd16(g_mem, self + COMP_YEAR), why);
        return;
    }
    for (int r = 0; readable(rows) && n > 1 && r < n; r++)
        if (rd32(g_mem, rows + (uint32_t)r * ROW_SIZE) == table[1]) g_mem[rows + (uint32_t)r * ROW_SIZE + ROW_STATUS] = 0;
    wr32(g_mem, self + STAGE_COUNT, (uint32_t)((int32_t)rd32(g_mem, self + STAGE_LAST) + 1));
    fprintf(stderr, "recomp: league sizes: arg_second reducido %d: %s; the next club of the table goes up\n",
            (int16_t)rd16(g_mem, self + COMP_YEAR), why);
}

void season_arg_reducido_advance(uint32_t self)
{
    SeasonArgReducido s;
    if (!reducido_spec(self, &s)) {
        /* Loaded with the sizes off: the ties made end the season, and no new stage is made. */
        wr32(g_mem, self + STAGE_COUNT, (uint32_t)((int32_t)rd32(g_mem, self + STAGE_LAST) + 1));
        return;
    }
    uint32_t t[TABLE_MAX];
    int nt = zone_order(self, t), made = (int32_t)rd32(g_mem, self + STAGE_LAST) + 1;
    if (rt_unwinding) return;
    if (made >= s.stages) return;
    if (nt < s.places + 1 + (s.zone_last ? 2 * (s.down_each + s.rel_tie) : 0)) {
        reducido_none(self, t, nt, "too few clubs");
        return;
    }
    if (!made) {
        int span = 0, left = days_to_change(self);
        for (int r = 0; r < s.rounds; r++) span += s.round[r].legs == 2 ? DAYS_TWO_LEGS : DAYS_ONE_MATCH;
        if (s.zone_last) span += DAYS_ONE_MATCH;
        if (left >= 0 && left < span) {
            reducido_none(self, t, nt, "no days before the season change");
            return;
        }
    }
    /* The clubs of the Reducido, best first: the places of the table, or for zones the zone places in turn. */
    uint32_t field[REDUCIDO_MAX + 4];
    int head = s.zone_last ? 1 + s.rel_tie : 0;
    if (s.zone_last) {
        int a, b, zone[TABLE_MAX];
        if (!season_arg_second_zones(self, nt, &a, &b)) return;
        zones_of(t, nt, a, zone);
        uint32_t in[2][TABLE_MAX];
        int cnt[2] = {0, 0}, k = 0;
        for (int i = 0; i < nt; i++) in[zone[i]][cnt[zone[i]]++] = t[i];
        /* Each zone holds the clubs of the Reducido and, below them, the clubs that go down. */
        if (cnt[0] < s.zone_last + s.down_each + s.rel_tie || cnt[1] < s.zone_last + s.down_each + s.rel_tie) {
            reducido_none(self, t, nt, "too few clubs in a zone");
            return;
        }
        if (!made) {
            for (int z = 0; z < 2; z++)
                for (int d = 0; d < s.down_each; d++) {
                    reducido_send_down(self, in[z][cnt[z] - 1 - d]);
                    if (rt_unwinding) return;
                }
            uint32_t st = reducido_tie(self, 0, in[0][0], in[1][0], 1, CODE_FINAL, 1, 1);
            if (rt_unwinding) return;
            if (st && s.rel_tie) st = reducido_tie(self, 1, in[0][cnt[0] - 1 - s.down_each], in[1][cnt[1] - 1 - s.down_each], 1,
                                                  CODE_PLAYOFF, 1, 1);
            if (rt_unwinding) return;
            if (!st) reducido_none(self, t, nt, "the zone winners' final not made");
            return;
        }
        for (int p = 1; p < s.zone_last; p++)
            for (int z = 0; z < 2; z++) field[k++] = in[z][p];
        /* The same zone place: the better place of the table first. */
        for (int p = 0; p + 1 < k; p += 2) {
            int x = -1, y = -1;
            for (int i = 0; i < nt; i++) x = t[i] == field[p] ? i : x, y = t[i] == field[p + 1] ? i : y;
            if (x > y) {
                uint32_t c = field[p];
                field[p] = field[p + 1];
                field[p + 1] = c;
            }
        }
    } else {
        for (int i = 0; i < s.places; i++) field[i] = t[1 + i];
    }
    /* The round whose ties are still to be made, and the places that entered before it. */
    int r = 0, before = head, entered = 0;
    while (r < s.rounds && before + s.round[r].ties <= made) {
        before += s.round[r].ties;
        entered += s.round[r].fresh;
        r++;
    }
    if (r >= s.rounds || before != made) return;
    /* The survivors as indices of field (-1: the final's loser, the best place): the winners, then the next places. */
    int place[REDUCIDO_MAX + 4], n = 0;
    for (int k = 0; r > 0 && k < s.round[r - 1].ties; k++) {
        uint32_t win = 0, lose = 0;
        if (!ko_winners(stage_at(self, made - 1 - k), &win, &lose)) return;     /* the next day */
        for (int i = 0; i < s.places; i++)
            if (field[i] == win) place[n++] = i;
        if (s.loser_round >= 0 && r > s.loser_round) {
            uint32_t fw = 0, fl = 0;
            if (ko_winners(stage_at(self, 0), &fw, &fl) && fl == win) place[n++] = -1;
        }
    }
    for (int k = 0; k < s.round[r].fresh; k++) place[n++] = s.places - 1 - entered - k;
    if (r == s.loser_round) place[n++] = -1;
    for (int i = 1; i < n; i++) {
        int v = place[i], j = i;
        for (; j > 0 && place[j - 1] > v; j--) place[j] = place[j - 1];
        place[j] = v;
    }
    if (n != 2 * s.round[r].ties) {
        reducido_none(self, t, nt, "the clubs of a round do not match its ties");
        return;
    }
    uint32_t loser = 0, final_win = 0;
    if (s.loser_round >= 0 && !ko_winners(stage_at(self, 0), &final_win, &loser)) return;
    int ties[REDUCIDO_MAX + 4];
    season_arg_reducido_pairs(place, n, ties);
    int last = r == s.rounds - 1, code = last ? CODE_FINAL : r == s.rounds - 2 ? CODE_SF : CODE_QF;
    for (int k = 0; k < s.round[r].ties; k++) {
        uint32_t a = ties[2 * k] < 0 ? loser : field[ties[2 * k]], b = ties[2 * k + 1] < 0 ? loser : field[ties[2 * k + 1]];
        int pens = (last && s.final_pens) || (s.round[r].legs == 2 && s.level_pens);
        uint32_t st = reducido_tie(self, made + k, a, b, s.round[r].legs, code, pens, last && s.final_one);
        if (rt_unwinding) return;
        if (!st) {
            reducido_none(self, t, nt, "a tie not made");
            return;
        }
        if (logging())
            fprintf(stderr, "recomp: league sizes: arg_second reducido %d: round %d tie %d, stage %d: %u (seed %d) v %u "
                            "(seed %d), %d legs, day %d\n", (int16_t)rd16(g_mem, self + COMP_YEAR), r + 1, k + 1, made + k,
                    rd32(g_mem, a), ties[2 * k] + 2, rd32(g_mem, b), ties[2 * k + 1] + 2, s.round[r].legs,
                    (int16_t)rd16(g_mem, GAME_DATE));
    }
}

/* Before the Primera's chain (0x4067f0 moves the rows of status 0 or 5 up, 0x406839): League::update_positions_and_status
 * 0x684cd0 gives the table's leader status 5, but in a season of two zones only the winners go up (status 0). A leader that
 * won nothing takes 0xfe, the mark of a club that stays (0x408e45). */
void season_arg_reducido_chain(uint32_t second)
{
    SeasonArgReducido s;
    if (!season_arg_reducido_marked(second) || !reducido_spec(second, &s) || !s.zone_last) return;
    uint32_t w0 = 0, l0 = 0, w1 = 0, l1 = 0;
    ko_winners(stage_at(second, 0), &w0, &l0);
    ko_winners(stage_at(second, (int32_t)rd32(g_mem, second + STAGE_LAST)), &w1, &l1);
    uint32_t rows = rd32(g_mem, second + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, second + COMP_CLUBS);
    for (int r = 0; readable(rows) && r < n; r++) {
        uint32_t row = rows + (uint32_t)r * ROW_SIZE, c = rd32(g_mem, row);
        int st = g_mem[row + ROW_STATUS];
        if (st == 5 && c != w0 && c != w1) g_mem[row + ROW_STATUS] = 0xfe;
    }
}

int season_arg_reducido_result(uint32_t self, int stage, int result, uint32_t club)
{
    SeasonArgReducido s;
    int count = (int32_t)rd32(g_mem, self + STAGE_COUNT);
    int zones = reducido_spec(self, &s) && s.zone_last;
    if (zones && stage == 0) return result == 1;                /* the zone winners' final: the winner goes up */
    if (zones && s.rel_tie && stage == 1) {
        if (result == 2) reducido_send_down(self, club);        /* the relegation tie: the loser goes down */
        return 0;
    }
    return stage + 1 == count && result == 1;
}

/* The Apertura's knockout, the day its table is complete: only while both knockouts are still to come (+0x2c holds
 * them both), so a knockout that cannot be made is not tried again; the Clausura's still comes. */
void season_arg_update(uint32_t obj)
{
    Plan p;
    if (!plan_live(obj, &p) || (p.kind != SEASON_ARG_KO_ZONES && p.kind != SEASON_ARG_KO_TABLE)) return;
    int first = FIRST_ZONE + p.zones;
    if ((int32_t)rd32(g_mem, obj + STAGE_LAST) != first - 1 || p.count != first + 2 || !table_done(obj)) return;
    int limit = days_to_first_date(stage_at(obj, 0));
    if (!build_ko(obj, &p, 0, limit) && !rt_unwinding) fewer(obj, first + 1, "no knockout of the Apertura");
}

/* The end of the season (every stage made is complete): the Clausura's knockout, or the 2016 final and runners-up
 * match, in the next slots; else the season closes with the stages it has. */
int season_arg_advance(uint32_t obj)
{
    Plan p;
    if (!plan_live(obj, &p)) return 0;
    int last = (int32_t)rd32(g_mem, obj + STAGE_LAST), left = days_to_change(obj);
    if (last + 1 >= p.count) return 0;
    if (p.kind == SEASON_ARG_KO_PROMO) return promo_advance(obj, left);
    if (p.kind == SEASON_ARG_KO_FINAL) {
        if (!build_final(obj, &p, left) && !rt_unwinding)
            fewer(obj, (int32_t)rd32(g_mem, obj + STAGE_LAST) + 1, "no final");
        return 1;
    }
    if (ko_named(obj, &p, NAME_CLAUSURA)) {
        fewer(obj, last + 1, "the knockouts are made");
        return 1;
    }
    if (!build_ko(obj, &p, 1, left) && !rt_unwinding) fewer(obj, last + 1, "no knockout of the Clausura");
    else if (!rt_unwinding) wr32(g_mem, obj + STAGE_COUNT, (uint32_t)((int32_t)rd32(g_mem, obj + STAGE_LAST) + 1));
    return 1;
}

static int is_ko(uint32_t obj, int stage)
{
    Plan p;
    return plan_live(obj, &p) && stage >= FIRST_ZONE + p.zones && stage <= (int32_t)rd32(g_mem, obj + STAGE_LAST);
}

int season_arg_ko_result(uint32_t obj, int stage)
{
    return stage >= FIRST_ZONE && is_ko(obj, stage);
}

/* The winner (result 1) and the finalist (2) of a complete knockout stage. */
static int ko_winners(uint32_t s, uint32_t *win, uint32_t *second)
{
    if (!readable(s) || g_mem[s + STAGE_TYPE] != 1 || !g_mem[s + CUP_COMPLETE]) return 0;
    uint32_t e = rd32(g_mem, s + COMP_MATCHES);
    int n = (int16_t)rd16(g_mem, s + COMP_CLUBS);
    *win = *second = 0;
    for (int i = 0; readable(e) && i < n && i < KO_CLUBS; i++) {
        uint8_t res = g_mem[e + 6u * (uint32_t)i + 5];
        if (res == 1) *win = rd32(g_mem, e + 6u * (uint32_t)i);
        if (res == 2) *second = rd32(g_mem, e + 6u * (uint32_t)i);
    }
    return readable(*win) && readable(*second);
}

/* The Primera's object: g_comp_object_table [0xadadfc] at the key [0x9cf654] (0x405765..0x405775). */
static uint32_t the_prm(void)
{
    uint32_t objects = rd32(g_mem, 0xadadfcu);
    int32_t key = (int32_t)rd32(g_mem, 0x9cf654u);
    uint32_t o = readable(objects) && key >= 0 ? rd32(g_mem, objects + 4u * (uint32_t)key) : 0;
    return readable(o) ? o : 0;
}

int season_arg_honours_swap(uint32_t league, uint32_t *saved)
{
    if (!readable(league)) return 0;
    uint32_t win, second;
    Plan p;
    int half = 0, zones = rd32(g_mem, league) == VT_ARG_SECOND;
    if (zones) {
        /* Item 291: the champion of a season of two zones is the winner of the zone winners' final (stage 0). */
        SeasonArgReducido s;
        if (!reducido_spec(league, &s) || !s.zone_last || !ko_winners(stage_at(league, 0), &win, &second)) return 0;
    } else {
        /* The Apertura (the league) or the Clausura (stages[0] of the Primera). */
        uint32_t prm = the_prm();
        if (!prm) return 0;
        if (league == prm) half = 0;
        else if (league == stage_at(prm, 0)) half = 1;
        else return 0;
        if (!plan_live(prm, &p)) return 0;
        int name = p.kind == SEASON_ARG_KO_FINAL ? (half ? -1 : NAME_CHAMPIONSHIP) : half ? NAME_CLAUSURA : NAME_APERTURA;
        if (name < 0 || !ko_winners(ko_named(prm, &p, name), &win, &second)) return 0;
    }
    uint32_t rows = rd32(g_mem, league + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, league + COMP_CLUBS);
    if (!readable(rows) || n < 2 || n > SEASON_ARG_HONOURS_ROWS) return 0;
    for (int i = 0; i < n; i++) saved[i] = rd32(g_mem, rows + (uint32_t)i * ROW_SIZE);
    /* Swap the clubs' pointers (not the rows): the winner to row 0, the finalist to row 1; each club stays once. */
    for (int k = 0; k < 2; k++) {
        uint32_t want = k ? second : win;
        for (int i = k; i < n; i++)
            if (rd32(g_mem, rows + (uint32_t)i * ROW_SIZE) == want) {
                uint32_t there = rd32(g_mem, rows + (uint32_t)k * ROW_SIZE);
                wr32(g_mem, rows + (uint32_t)k * ROW_SIZE, want);
                wr32(g_mem, rows + (uint32_t)i * ROW_SIZE, there);
                break;
            }
    }
    if (logging() && zones)
        fprintf(stderr, "recomp: league sizes: arg_second champion %d: %.30s, runner-up %.30s (the final)\n",
                (int16_t)rd16(g_mem, league + COMP_YEAR), win_str(win + CLUB_NAME), win_str(second + CLUB_NAME));
    else if (logging())
        fprintf(stderr, "recomp: league sizes: arg_prm champion %d %s: %.30s, runner-up %.30s (the %s)\n",
                (int16_t)rd16(g_mem, league + COMP_YEAR), half ? "clausura" : "apertura", win_str(win + CLUB_NAME),
                win_str(second + CLUB_NAME), p.kind == SEASON_ARG_KO_FINAL ? "final" : "knockout");
    return 1;
}

void season_arg_honours_restore(uint32_t league, const uint32_t *saved)
{
    uint32_t rows = rd32(g_mem, league + COMP_TABLE);
    int n = (int16_t)rd16(g_mem, league + COMP_CLUBS);
    if (!readable(rows) || n < 2 || n > SEASON_ARG_HONOURS_ROWS) return;
    for (int i = 0; i < n; i++) wr32(g_mem, rows + (uint32_t)i * ROW_SIZE, saved[i]);
}

/* Cup_Stage +0x90 0x522360 (thiscall (home club, away club), ret 8): the club of the lower division record, else 0;
 * Cup::set_home_teams_for_round 0x51e870 puts the club it returns at home. In a knockout of the Primera: the better
 * seed (the zone place). */
void o_00522360(void);
RT_WRAP(00522360, arg_ko_home)
{
    uint32_t s = cpu.ecx, h = rt_arg(0), w = rt_arg(1);
    uint32_t prm = season_sizes_on() ? the_prm() : 0;
    int at = readable(s) ? (int8_t)g_mem[s + STAGE_INDEX] : -1;
    if (!prm || !is_ko(prm, at) || stage_at(prm, at) != s) {
        o_00522360();
        return;
    }
    uint32_t e = rd32(g_mem, s + COMP_MATCHES);
    int n = (int16_t)rd16(g_mem, s + COMP_CLUBS), sh = 99, sw = 99;
    for (int i = 0; readable(e) && i < n && i < KO_CLUBS; i++) {
        uint32_t c = rd32(g_mem, e + 6u * (uint32_t)i);
        if (c == h) sh = g_mem[e + 6u * (uint32_t)i + 4];
        if (c == w) sw = g_mem[e + 6u * (uint32_t)i + 4];
    }
    rt_ret_stdcall(sh < sw ? h : sw < sh ? w : 0, 2);
}

/* The names of the zone tables (season_round_names_game.c): the original has no "Zone A". */
const char *season_arg_stage_name(uint32_t context, int id)
{
    if (!readable(context) || !season_sizes_on() || strcmp(season_change_file(context), "arg_prm")) return NULL;
    switch (id) {
    case NAME_ZONE_A1: return "Apertura Zone A";
    case NAME_ZONE_A1 + 1: return "Apertura Zone B";
    case NAME_ZONE_A2: return "Clausura Zone A";
    case NAME_ZONE_A2 + 1: return "Clausura Zone B";
    case 0x449: return "Apertura Zones";
    case 0x44a: return "Clausura Zones";
    case NAME_GROUP_A: return "Zone A";
    case NAME_GROUP_A + 1: return "Zone B";
    case 0x41c: return "Zones";
    default: return NULL;
    }
}
