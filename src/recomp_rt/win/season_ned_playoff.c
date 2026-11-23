/* The Dutch nacompetitie as a knockout from 2005/06 (issue 150). The plan of each season is season_ned_playoff_plan.c.
 *
 * The original (3.9.68). HolFirst::advance_stage 0x5f6650 waits for the Eredivisie (check_complete at 0x5f66e3), moves
 * to the next stage and at stage 4 calls create_play_off_groups 0x5f6720. That function tops the rows of status 1 up to
 * six from the rows of 0xff in table order (0x5f677e..0x5f67cd, news through vtable +0x40 at 0x5f67b2 and +0x68 at
 * 0x5f67cd), adds the Eredivisie rows of status 2 (0x5f68a3..0x5f68ce) and makes two League_Stages of four, stages 4
 * and 5 (0x5f6962, 0x5f6a09). The result handler 0x5f6ee0 (vtable +0x44) settles them: an Eerste Divisie club of code 0
 * gets status 0 and goes up (0x5f707e), one of code 0xfe gets 0xfe (0x5f709d); an Eredivisie club of code 0 gets 0xfe
 * and stays (0x5f70b8), one of code 0xfe gets status 3 and goes down (0x5f70f6). Each group holds one club of each
 * outcome pair, so the chain 0x689c20 moves as many clubs up as down.
 *
 * Here, in a season game with changing rules on and a season of a knockout form:
 * - The wrap of 0x5f6720 (season_ned_second.c) calls season_ned_playoff_build in place of the original. It picks the
 *   clubs as the original does and makes one Cup_Stage (0x522e00) per bracket in the group slots, stage numbers as the
 *   groups have them (rt_hol_playoff_stage, Cup_Stage +0x44 from its eighth argument at 0x522e5b). Each bracket holds
 *   one Eredivisie club and plays for one place, so the chain stays balanced.
 * - A form of one bracket, or no play-off in 2019/20, ends the season at an earlier stage (season_ned_playoff_last_stage).
 * - Cup::draw_round 0x51c410 is told the ties of each round (season_ned_playoff_draw_round, as season_playoffs_draw_round).
 * - The wrap of 0x5f6ee0 settles a bracket at its final: code 1 is the winner (as EngFirst's handler 0x576d71).
 * - The season change waits for the brackets (season_playoffs.c, entry_playoffs).
 * - From 2020/21 the Eredivisie relegates two clubs directly and only the 16th plays off (season_ned_playoff_rules):
 *   HolPrm::set_up_rules 0x5f8560 gives one direct place and two play-off places, which the rules step keeps because
 *   it reads the row's 2 as one direct place and one play-off winner (season_sizes.c set_places).
 * - 2019/20 was abandoned and no club moved: before the chain 0x689c20 the rows lose the statuses that it moves (the
 *   title 5 of 0x684cd0 still goes up with no promotion place).
 * The stages save and load as the Cup_Stages of the other leagues (League::save 0x6891c0, load 0x687b10). A save made
 * during the groups of an older build keeps its League_Stages and the original's handler.
 *
 * Switch: RECOMP_NED_PLAYOFF=off (tests) keeps the groups in every season. */
#include "win.h"
#include "season.h"
#include "season_names.h"
#include "season_ned_playoff.h"
#include "season_ned_playoff_plan.h"
#include "season_playoffs.h"
#include "season_second_teams.h"
#include "season_sizes.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_PREMIER 0x9cf568u           /* the Eredivisie (0x5f6882) */
#define KEY_FIRST 0x9cf56cu             /* the Eerste Divisie (0x5f6fe6) */
#define COMP_RECORD 4u
#define COMP_STAGES 0x0cu
#define COMP_STAGE_NOW 0x30u
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_STAGE_BYTE 0x44u           /* pushed for the news at 0x5f6799 */
#define CUP_VTABLE 0x00969468u          /* Cup_Stage (0x522e80) */
#define CUP_ROUNDS 0xa3u
#define CUP_LIST 0xa7u                  /* 6 bytes a slot, the club at +0 */
#define CUP_TIES 0xadu
#define TIE_SIZE 0x35u
#define TIE_BUILT 0x34u
#define ROUND_SIZE 0x68u
#define STATUS_PLAYOFF_UP 1             /* 0x5f676a */
#define STATUS_PLAYOFF_DOWN 2           /* 0x5f68ad */
#define STATUS_OUT 0xfeu                /* 0x5f709d */
#define PLACES_DOWN_PLAYOFF 0xc0u       /* League: relegation play-off places */
#define PLACES_DOWN 0xc1u               /* League: direct relegation places */
#define NEWS_OBJECT 0x00dd7f00u
#define F_NEWS_UP 0x00869480u           /* thiscall (club, record, 0x32), as 0x5f7060 */
#define F_NEWS_DOWN 0x0086a1c0u         /* thiscall (club, record), as 0x5f70d5 */
#define F_ENG_ROUNDS 0x00571980u        /* EngFirst +0x3c (stage, &rounds, &name, flag): reads only the year */
#define F_CUP_STAGE 0x00522e00u
#define F_SET_UP_ROUND 0x0051c800u
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_NEW 0x00944cf1u

static uint32_t call_cdecl(uint32_t fn, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !st_readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    uint32_t a[1] = {p};
    if (st_readable(p) && !rt_unwinding) call_cdecl(F_FREE, 1, a);
}

static int season_of(uint32_t self) { return season_sizes_row_season(self, "hol_first"); }

static NedPoForm form_now(uint32_t self)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_NED_PLAYOFF", "off");
    if (off || !season_sizes_on() || !season_game_changes() || !season_changing_rules()) return NED_PO_GROUPS;
    int season = season_of(self);
    return season < 0 ? NED_PO_GROUPS : ned_po_form(season);
}

static const char *club_name(uint32_t club) { return st_readable(club) ? win_str(club + ST_CLUB_NAME) : "?"; }

/* The row index of a club in a league table, or -1. */
static int place_in(uint32_t league, uint32_t club)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, table + (uint32_t)i * ST_ROW_SIZE) == club) return i;
    return -1;
}

/* The bracket of a stage object of this season's play-off, or -1. */
static int bracket_of(uint32_t self, uint32_t stage)
{
    /* HolFirst::start_new_season deletes the stages and keeps the pointers (0x5f6436..0x5f644a), and another cup can
     * take the freed block: a bracket is a Cup_Stage of this league and this edition. */
    if (!st_readable(self) || !st_readable(stage) || rd32(g_mem, stage) != CUP_VTABLE ||
        rd32(g_mem, stage + COMP_RECORD) != rd32(g_mem, self + COMP_RECORD) ||
        rd16(g_mem, stage + COMP_YEAR) != rd16(g_mem, self + COMP_YEAR))
        return -1;
    NedPoForm form = form_now(self);
    uint32_t stages = rd32(g_mem, self + COMP_STAGES), first = rt_hol_playoff_stage(self, 4u);
    for (int b = 0; b < ned_po_brackets(form); b++)
        if (rd32(g_mem, stages + 4u * (first + (uint32_t)b)) == stage) return b;
    return -1;
}

/* ---- The clubs ---- */

/* The Eerste Divisie entrants in table order: the rows of status 1, topped up from the rows of 0xff as 0x5f677e..
 * 0x5f67cd does. Returns the count. */
static int pick_lower(uint32_t self, int want, uint32_t out[8])
{
    int n, have = 0, got = 0;
    uint32_t table = st_table_of(self, &n);
    for (int i = 0; i < n; i++) have += g_mem[table + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS] == STATUS_PLAYOFF_UP;
    uint32_t vt = rd32(g_mem, self), stage = g_mem[self + COMP_STAGE_BYTE];
    for (int i = 0; i < n && have < want && !rt_unwinding; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE, club = rd32(g_mem, row);
        if (g_mem[row + ST_ROW_STATUS] != ST_NONE) continue;
        g_mem[row + ST_ROW_STATUS] = STATUS_PLAYOFF_UP;
        have++;
        uint32_t news[5] = {club, 1, stage, 0, 0}, progress[5] = {club, stage, 2, 2, 7};
        st_call_this(rd32(g_mem, vt + 0x40u), self, 5, news);
        if (!rt_unwinding) st_call_this(rd32(g_mem, vt + 0x68u), self, 5, progress);
    }
    for (int i = 0; i < n && got < want; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] == STATUS_PLAYOFF_UP) out[got++] = rd32(g_mem, row);
    }
    return got;
}

/* The Eredivisie rows of status 2 in table order (0x5f68a3..0x5f68ce). Returns the count. */
static int pick_upper(int want, uint32_t out[2])
{
    int n, got = 0;
    uint32_t table = st_table_of(st_league_of(KEY_PREMIER), &n);
    for (int i = 0; i < n && got < want; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] == STATUS_PLAYOFF_DOWN) out[got++] = rd32(g_mem, row);
    }
    return got;
}

/* ---- The stages ---- */

/* The round table of a bracket: EngFirst's semi-final (two legs, 4 days apart, 0x572af3..0x572b20) or final (one match
 * with extra time and penalties, 0x572b78) in each round. */
static uint32_t round_table(uint32_t self, NedPoForm form, int legs)
{
    uint32_t outs = game_malloc(8);
    if (!outs) return 0;
    uint32_t ra[4] = {0, outs, outs + 4, 0};
    uint32_t eng = st_call_this(F_ENG_ROUNDS, self, 4, ra);
    int eng_rounds = (int16_t)rd16(g_mem, outs);
    game_free(outs);
    if (rt_unwinding) return 0;
    int rounds = ned_po_rounds(form);
    uint32_t table = st_readable(eng) && eng_rounds == 2 ? game_malloc((uint32_t)rounds * ROUND_SIZE) : 0;
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    for (int r = 0; table && r < rounds; r++) {
        NedPoRound o;
        ned_po_round(form, r, &o);
        uint32_t t = table + (uint32_t)r * ROUND_SIZE;
        memcpy(g_mem + t, g_mem + eng + (legs == 2 ? 0u : ROUND_SIZE), ROUND_SIZE);
        wr16(g_mem, t + 0x07, (uint16_t)o.id);
        wr16(g_mem, t + 0x09, (uint16_t)r);
        wr16(g_mem, t + 0x18, (uint16_t)o.clubs);
        wr16(g_mem, t + 0x1a, (uint16_t)o.ties);
        wr16(g_mem, t + 0x1c, (uint16_t)o.add);
        wr16(g_mem, t + 0x1e, (uint16_t)o.first);
        /* Fixed pairs (no draw, 0x51c5e4..0x51c5f1) and no home swap (0x51e8b7..0x51ea3d), as the play-off of six. */
        wr16(g_mem, t + 0x0d, rd16(g_mem, t + 0x0d) | 0x200u);
        g_mem[t + 0x0b] &= (uint8_t)~0x09u;
        /* Two legs 3 days apart (EngFirst's semi-final has 4, +0x22 at 0x572b20), so that a day is free between the
         * second leg and the next draw. A clash move of a second leg can go to that day (fixture_season_end.c, the
         * bound of a tie with fixed pairs). With 4 days the second leg was on the day before the draw and a move had
         * no day left: in sp2021 (2022/23) round 0 was drawn on day 133 of 2023, its first legs played on day 136
         * and its second legs on day 140, round 1 was drawn on day 141, and add_fixture's clash move took FC
         * Volendam v Sparta Rotterdam from day 140 to day 143. Round 1 was drawn without its winner. */
        if (g_mem[t + 0x21] == 2) g_mem[t + 0x22] = 3;
    }
    game_free(eng);
    for (int r = 0; table && r < rounds && !rt_unwinding; r++)
        season_playoffs_round_dates(table, r, 1 + 7 * r, 3 + 7 * r, r == rounds - 1 ? 2u : 1u, 0, year);
    return rt_unwinding ? 0 : table;
}

static int build_bracket(uint32_t self, NedPoForm form, int legs, int bracket, uint32_t slot, const uint32_t lower[8],
                         const uint32_t upper[2])
{
    int codes[NED_PO_MAX_LIST], n = ned_po_entrants(form, bracket, codes);
    uint32_t table = round_table(self, form, legs), list = table ? game_malloc(4u * (uint32_t)n) : 0;
    if (!list) {
        game_free(table);
        return 0;
    }
    for (int i = 0; i < n; i++)
        wr32(g_mem, list + 4u * (uint32_t)i, codes[i] >= NED_PO_UPPER ? upper[codes[i] - NED_PO_UPPER] : lower[codes[i]]);
    uint32_t size[1] = {0xb2};
    uint32_t stage = call_cdecl(F_NEW, 1, size);
    if (rt_unwinding || !st_readable(stage)) {
        game_free(list);
        game_free(table);
        return 0;
    }
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, self + COMP_YEAR);
    uint32_t args[15] = {self, (uint32_t)n, list, (uint32_t)ned_po_rounds(form), rd32(g_mem, self + COMP_RECORD), table,
                         year, slot, 1, 0xa0, 0x14, 0, 0, 0, 0};    /* as build_stage of season_playoffs.c */
    st_call_this(F_CUP_STAGE, stage, 15, args);
    if (rt_unwinding) return 0;
    wr32(g_mem, rd32(g_mem, self + COMP_STAGES) + 4u * slot, stage);
    game_free(list);
    game_free(table);
    uint32_t r0[1] = {0};
    st_call_this(F_SET_UP_ROUND, stage, 1, r0);
    return !rt_unwinding;
}

int season_ned_playoff_build(uint32_t self)
{
    NedPoForm form = form_now(self);
    int brackets = ned_po_brackets(form), season = season_of(self);
    if (!brackets) return 0;
    uint32_t lower[8], upper[2];
    int want_low = ned_po_lower_clubs(form), want_up = ned_po_upper_clubs(form);
    /* The Eredivisie clubs first: the groups of the fallback pick their own clubs, and pick_lower sends news. */
    int up = pick_upper(want_up, upper);
    if (up != want_up) {
        fprintf(stderr, "recomp: ned playoff\tno knockout\t%d\t%d of %d upper clubs: the groups play\n", season, up,
                want_up);
        return 0;
    }
    int low = pick_lower(self, want_low, lower);
    if (rt_unwinding) return 1;
    if (low != want_low) {
        fprintf(stderr, "recomp: ned playoff\tno knockout\t%d\t%d of %d lower clubs: the groups play\n", season, low,
                want_low);
        return 0;
    }
    uint32_t first = rt_hol_playoff_stage(self, 4u);
    int legs = ned_po_legs(season);
    for (int b = 0; b < brackets; b++)
        wr32(g_mem, rd32(g_mem, self + COMP_STAGES) + 4u * (first + (uint32_t)b), 0);   /* no pointer of last season */
    for (int b = 0; b < brackets; b++) {
        if (!build_bracket(self, form, legs, b, first + (uint32_t)b, lower, upper)) {
            /* Out of memory: the game cannot go on, as at the box of 0x5f6a48. */
            if (!rt_unwinding) fprintf(stderr, "recomp: ned playoff\tfailed\t%d\tbracket %d\n", season, b);
            return 1;
        }
        wr32(g_mem, self + COMP_STAGE_NOW, first + (uint32_t)b);
        int codes[NED_PO_MAX_LIST], n = ned_po_entrants(form, b, codes);
        fprintf(stderr, "recomp: ned playoff\tdrawn\t%d\tbracket %d\t%d legs\tday %d", season, b, legs,
                (int16_t)rd16(g_mem, 0x00ae2c90u));
        for (int i = 0; i < n; i++)
            fprintf(stderr, "\t%.51s", club_name(codes[i] >= NED_PO_UPPER ? upper[codes[i] - NED_PO_UPPER]
                                                                          : lower[codes[i]]));
        fputc('\n', stderr);
    }
    return 1;
}

/* ---- The ties of a round ---- */

/* The list index of the plan of a club of the bracket: the Eredivisie club by its code, an Eerste Divisie club by its
 * rank in table order among the bracket's clubs. The stage does not keep the list order (season_playoffs.c, item 273).
 * -1 when not found. */
static int list_index(uint32_t self, NedPoForm form, int bracket, uint32_t cup, uint32_t club)
{
    int codes[NED_PO_MAX_LIST], n = ned_po_entrants(form, bracket, codes), place = place_in(self, club);
    if (place < 0) {
        for (int i = 0; i < n; i++)
            if (codes[i] >= NED_PO_UPPER) return i;
        return -1;
    }
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int count = (int16_t)rd16(g_mem, cup + COMP_CLUBS), rank = 0;
    for (int s = 0; s < count && s < NED_PO_MAX_LIST; s++) {
        int p = place_in(self, rd32(g_mem, list + 6u * (uint32_t)s));
        if (p >= 0 && p < place) rank++;
    }
    for (int seed = 0; seed < NED_PO_UPPER; seed++)
        for (int i = 0; i < n; i++)
            if (codes[i] == seed && rank-- == 0) return i;
    return -1;
}

typedef struct { int16_t slot, tie, round; } Side;

static void side_get(uint32_t tie, int k, Side *v)
{
    v->slot = (int16_t)rd16(g_mem, tie + 2u * (uint32_t)k);
    v->tie = (int16_t)rd16(g_mem, tie + 0x0au + 2u * (uint32_t)k);
    v->round = (int16_t)rd16(g_mem, tie + 0x0eu + 2u * (uint32_t)k);
}

static void side_set(uint32_t tie, int k, const Side *v)
{
    wr16(g_mem, tie + 2u * (uint32_t)k, (uint16_t)v->slot);
    wr16(g_mem, tie + 0x0au + 2u * (uint32_t)k, (uint16_t)v->tie);
    wr16(g_mem, tie + 0x0eu + 2u * (uint32_t)k, (uint16_t)v->round);
}

/* A tie of the round before with no winner on the draw day. Cup::draw_round 0x51c410 waits only for a side whose
 * list slot holds no club yet (0x51c47c..0x51c4b1, then 0x51c4e9 moves the draw dates on). A side that waits for the
 * winner of a tie has slot -1, which that test skips (0x51c47f, 0x51c49b), so the round is built without the club and
 * its tie is never played. The round dates (round_table) and the bound of a moved leg (fixture_season_end.c) keep each
 * leg before the next draw, so this is only a log of the case that they cannot cover: a moved leg with no free day
 * before the draw. */
static void log_missing(uint32_t self, int bracket, int round, uint32_t base, int ties)
{
    for (int t = 0; round > 0 && t < ties; t++)
        for (int k = 0; k < 2; k++) {
            Side s;
            side_get(base + (uint32_t)t * TIE_SIZE, k, &s);
            if (s.slot < 0 && s.tie >= 0)
                fprintf(stderr, "recomp: ned playoff\twaits\t%d\tbracket %d\tround %d\ttie %d of the round before has no "
                        "winner on the draw day\n", season_of(self), bracket, round, s.tie);
        }
}

void season_ned_playoff_draw_round(uint32_t cup, int round)
{
    uint32_t self = st_league_of(KEY_FIRST);
    int bracket = self ? bracket_of(self, cup) : -1;
    if (bracket < 0) return;
    NedPoForm form = form_now(self);
    NedPoRound o;
    uint32_t ties = rd32(g_mem, cup + CUP_TIES), list = rd32(g_mem, cup + CUP_LIST);
    if (!ned_po_round(form, round, &o) || !st_readable(ties) || !st_readable(list)) return;
    uint32_t base = rd32(g_mem, ties + 4u * (uint32_t)round);
    if (!st_readable(base)) return;
    for (int t = 0; t < o.ties; t++)
        if (g_mem[base + (uint32_t)t * TIE_SIZE + TIE_BUILT]) return;     /* a round already built */
    log_missing(self, bracket, round, base, o.ties);
    Side have[2 * NED_PO_MAX_TIES], want[2 * NED_PO_MAX_TIES];
    int index[2 * NED_PO_MAX_TIES], sides = 2 * o.ties;
    for (int t = 0; t < o.ties; t++) {
        uint32_t tie = base + (uint32_t)t * TIE_SIZE;
        for (int k = 0; k < 2; k++) {
            Side *s = &have[2 * t + k];
            side_get(tie, k, s);
            int from_tie = round > 0 && s->tie >= 0;   /* a winner of the round before */
            if (s->slot < 0 && !from_tie) return;
            index[2 * t + k] = from_tie ? -(s->tie + 1)
                                        : list_index(self, form, bracket, cup, rd32(g_mem, list + 6u * (uint32_t)s->slot));
        }
    }
    for (int t = 0; t < o.ties; t++) {
        int plan[2];
        if (!ned_po_tie(form, round, t, &plan[0], &plan[1])) return;
        for (int k = 0; k < 2; k++) {
            int found = -1;
            for (int i = 0; i < sides && found < 0; i++)
                if (index[i] == plan[k]) found = i;
            if (found < 0) {
                fprintf(stderr, "recomp: ned playoff\tno side\tbracket %d\tround %d\ttie %d\n", bracket, round, t);
                return;
            }
            want[2 * t + k] = have[found];
        }
    }
    /* The better placed club plays the second leg at home, an Eredivisie club above every Eerste Divisie club (RSSSF
     * 2011/12, 2018/19, 2022/23). A single match (2020/21) is at the better placed club's ground: side 0 is at home
     * (season_playoffs.c, the play-off of six). */
    int legs = ned_po_legs(season_of(self));
    for (int t = 0; t < o.ties; t++) {
        if (want[2 * t].slot >= 0 && want[2 * t + 1].slot >= 0) {
            int a = place_in(self, rd32(g_mem, list + 6u * (uint32_t)want[2 * t].slot));
            int b = place_in(self, rd32(g_mem, list + 6u * (uint32_t)want[2 * t + 1].slot));
            int first_better = a < 0 ? 1 : b < 0 ? 0 : a < b;     /* -1: the Eredivisie club */
            if (first_better == (legs == 2)) {
                Side x = want[2 * t];
                want[2 * t] = want[2 * t + 1];
                want[2 * t + 1] = x;
            }
        }
        for (int k = 0; k < 2; k++) side_set(base + (uint32_t)t * TIE_SIZE, k, &want[2 * t + k]);
        /* tests/seasons/ned_playoff_run.py compares these lines with the plan. */
        fprintf(stderr, "recomp: ned playoff\ttie\t%d\tbracket %d\tround %d\ttie %d\t%.51s\t%.51s\n", season_of(self),
                bracket, round, t, want[2 * t].slot >= 0 ? club_name(rd32(g_mem, list + 6u * (uint32_t)want[2 * t].slot)) : "?",
                want[2 * t + 1].slot >= 0 ? club_name(rd32(g_mem, list + 6u * (uint32_t)want[2 * t + 1].slot)) : "?");
    }
}

/* ---- The result of a bracket ---- */

/* The Eredivisie club of a bracket, or 0. */
static uint32_t bracket_upper(uint32_t self, uint32_t cup)
{
    uint32_t list = rd32(g_mem, cup + CUP_LIST);
    int count = (int16_t)rd16(g_mem, cup + COMP_CLUBS);
    for (int s = 0; st_readable(list) && s < count && s < NED_PO_MAX_LIST; s++) {
        uint32_t club = rd32(g_mem, list + 6u * (uint32_t)s);
        if (place_in(self, club) < 0) return club;
    }
    return 0;
}

static void set_status(uint32_t league, uint32_t club, uint8_t status)
{
    int place = place_in(league, club), n;
    uint32_t table = st_table_of(league, &n);
    if (place >= 0) g_mem[table + (uint32_t)place * ST_ROW_SIZE + ST_ROW_STATUS] = status;
}

/* The final's winner: as 0x5f7053..0x5f707e and 0x5f70ca..0x5f70f6 for the clubs that move. */
static void settle(uint32_t self, uint32_t cup, int bracket, uint32_t winner)
{
    uint32_t premier = st_league_of(KEY_PREMIER), upper = bracket_upper(self, cup), record = rd32(g_mem, self + COMP_RECORD);
    uint32_t list = rd32(g_mem, cup + CUP_LIST), news = rd32(g_mem, NEWS_OBJECT);
    int count = (int16_t)rd16(g_mem, cup + COMP_CLUBS), lower_won = place_in(self, winner) >= 0;
    for (int s = 0; s < count && s < NED_PO_MAX_LIST; s++) {
        uint32_t club = rd32(g_mem, list + 6u * (uint32_t)s);
        if (club != winner && club != upper) set_status(self, club, STATUS_OUT);
    }
    if (lower_won) {
        set_status(self, winner, ST_UP);
        uint32_t a[3] = {winner, record, 0x32};
        st_call_this(F_NEWS_UP, news, 3, a);
        if (rt_unwinding) return;
        if (upper) {
            set_status(premier, upper, ST_DOWN);
            uint32_t b[2] = {upper, record};
            st_call_this(F_NEWS_DOWN, news, 2, b);
            if (rt_unwinding) return;
        }
    } else {
        set_status(premier, winner, STATUS_OUT);
    }
    fprintf(stderr, "recomp: ned playoff\twinner\t%d\tbracket %d\t%.51s\t%s\t%.51s\t%s\n", season_of(self), bracket,
            club_name(winner), lower_won ? "goes up" : "stays up", club_name(upper), lower_won ? "goes down" : "stays up");
}

/* HolFirst +0x44 (club, code, stage, &status, fixture, arg), thiscall, ret 0x18. */
void o_005f6ee0(void);
RT_WRAP(005f6ee0, ned_playoff_result)
{
    uint32_t self = cpu.ecx;
    int stage = (int8_t)g_mem[cpu.esp + 0xc];
    uint32_t stages = rd32(g_mem, self + COMP_STAGES);
    uint32_t cup = stage >= 0 && stage < 8 && st_readable(stages) ? rd32(g_mem, stages + 4u * (uint32_t)stage) : 0;
    int bracket = cup ? bracket_of(self, cup) : -1;
    if (bracket < 0) {
        o_005f6ee0();
        return;
    }
    if ((int8_t)g_mem[cpu.esp + 8] == 1) {
        uint32_t club = rd32(g_mem, cpu.esp + 4), status = rd32(g_mem, cpu.esp + 0x10);
        settle(self, cup, bracket, club);
        if (rt_unwinding) return;
        if (st_readable(status)) g_mem[status] = 1;      /* as EngFirst's handler 0x576da4 */
    }
    rt_ret_stdcall(0, 6);
}

/* ---- The season ---- */

uint32_t season_ned_playoff_last_stage(uint32_t obj, uint32_t last)
{
    NedPoForm form = form_now(obj);
    uint32_t first = rt_hol_playoff_stage(obj, 4u);
    /* The groups of a save of an older build play on to their own last stage. */
    if ((int32_t)rd32(g_mem, obj + COMP_STAGE_NOW) > (int32_t)first) return last;
    return form == NED_PO_NONE ? first - 1u : ned_po_brackets(form) == 1 ? first : last;
}

void season_ned_playoff_rules(uint32_t obj)
{
    uint32_t first = st_league_of(KEY_FIRST);
    if (!st_readable(obj) || !first || obj != st_league_of(KEY_PREMIER)) return;
    NedPoForm form = form_now(first);
    if (form != NED_PO_SEVEN && form != NED_PO_LADDER) return;
    g_mem[obj + PLACES_DOWN] = 2;
    g_mem[obj + PLACES_DOWN_PLAYOFF] = 1;
}

/* Before the chain of HolPrm::start_new_season (0x5f8623), while both divisions are in the old season. */
static void ned_playoff_before_chain(uint32_t league)
{
    uint32_t first = st_league_of(KEY_FIRST);
    if (!first || league != st_league_of(KEY_PREMIER) || form_now(first) != NED_PO_NONE) return;
    uint32_t both[2] = {league, first};
    for (int d = 0; d < 2; d++) {
        int n;
        uint32_t table = st_table_of(both[d], &n);
        for (int i = 0; i < n; i++) {
            uint8_t *s = &g_mem[table + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS];
            if (*s == ST_UP || *s == ST_TITLE || *s == ST_DOWN) *s = ST_NONE;
        }
    }
    fprintf(stderr, "recomp: ned playoff\tabandoned\t%d\tno club moves\n", season_of(first));
}

static const SecondTeamHooks ned_playoff_hooks = {NULL, ned_playoff_before_chain, NULL, NULL};
SEASON_SECOND_HOOKS(ned_playoff_hooks)

int season_ned_playoff_pending(uint32_t obj)
{
    return st_readable(obj) && obj == st_league_of(KEY_FIRST) && ned_po_brackets(form_now(obj)) > 0;
}

/* The match news 0x5f7110 tells the groups by their stage numbers (0x5f7117, 0x5f711d). A bracket takes the base news
 * 0x48c6d0, as the Cup_Stages of the other leagues. */
uint32_t rt_hol_match_news_stage(uint32_t obj, uint32_t original)
{
    uint32_t stage = rt_hol_playoff_stage(obj, original), stages = rd32(g_mem, obj + COMP_STAGES);
    return st_readable(stages) && bracket_of(obj, rd32(g_mem, stages + 4u * stage)) >= 0 ? 0x7fu : stage;
}
