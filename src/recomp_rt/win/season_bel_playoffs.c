/* The Belgian play-offs after the Pro League's groups (issue 150). The rules by season are in season_bel_plan.c.
 *
 * The original. BelFirst (vtable 0x967940) is a plain league: advance_stage +0x28 is the empty 0x5a8f60, the result
 * handler +0x44 is the base 0x686940, and BelFirst::start_new_season 0x41ea90 frees no stage. League::update_for_date
 * 0x6847c0 closes a season when the table and the stages up to +0x2c - 1 are complete, and before that calls
 * advance_stage each day (0x6849aa..0x6849e3). So the stages are made here, in the array +0xc, as the Argentine Primera's
 * knockouts (season_arg_stages.c). A stage of an old season stays in the heap, as in season_playoffs.c drop_stage.
 *
 * The stages of the First Division:
 * - 2016/17 to 2018/19: [0] and [1] the two groups of Play-off II, League_Stages of six clubs, as BelSecond::
 *   create_playoff_group 0x420230 makes its group (0x68b100 at 0x420417). Second-division clubs cannot be rows of the
 *   parent's table, so the split gives these places no matches (encode_leagues.py). The groups wait for the Second
 *   Division's season, its final included, and take the dates of Play-off I.
 * - Each season: the Play-off II final, then the Europa League play-off, Cup_Stages of two clubs in tree mode 1
 *   (Cup_Stage::map_fixture_tree 0x5241e0: entrant 0 is at home in the first match).
 *
 * The European place. collect_european_clubs 0x904040 sorts the clubs by the place that League::record_final_positions
 * 0x68aa30 stores in club +0x5f (0x68aa6d). When the Play-off II club wins the Europa League play-off, the wrap of
 * 0x68aa30 gives it the place of the Play-off I club and the other way round. The table itself keeps its order.
 *
 * The second division's final round. BelSecond::create_playoff_group 0x420230 lists the rows of status 1 in table order,
 * at most four (0x42036b..0x420378). For the call, a free row at the bottom of the table holds each First Division
 * entrant with status 1, so the top-up (0x42028e..0x4202a6) stops at the right count. BelSecond::update_staff_records_
 * for_status 0x420790 gives status 0 to a winner that is a row of its table (0x420886) and nothing to another club. The
 * promotion chain 0x41ee80 then starts with season_bel_po_settle, which reads the group's table.
 *
 * The base game, RECOMP_LEAGUE_SIZES=off and RECOMP_BEL_PLAYOFFS=off (tests): no stage is made and each wrap passes to
 * the original. */
#include "win.h"
#include "season.h"
#include "season_bel_plan.h"
#include "season_bel_playoffs.h"
#include "season_change.h"
#include "season_names.h"
#include "season_sizes.h"
#include "season_split.h"
#include "season_template.h"
#include "season_uefa.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define KEY_FIRST 0x9cf59cu
#define KEY_SECOND 0x9cf5a0u
#define LEAGUES 0xadadfcu
#define GAME_DATE 0xae2c90u             /* s16 day of the year, +2 s16 year */
#define ENTRY_COMPS 256

#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_LEAGUE_STAGE 0x0068b100u      /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_CUP_STAGE 0x00522e00u         /* Cup_Stage::Cup_Stage, thiscall, 15 arguments */
#define F_SET_UP_ROUND 0x0051c800u      /* Cup::set_up_round, thiscall (round) */
#define F_SORT_TABLE 0x006827d0u        /* League::sort_table, thiscall (rows or 0) */
#define F_DRAW_DATE 0x00521e60u         /* cdecl (table, round, day, month, year+, weekday, year) */
#define F_MATCH_DATE 0x00521eb0u        /* cdecl (table, round, day, month, year+, weekday, a7, year, venue) */
#define NO_WEEKDAY 0xffffffffu

#define COMP_RECORD 4u
#define COMP_STAGES 0xcu
#define COMP_STAGE_COUNT 0x2cu          /* s32 */
#define COMP_STAGE_LAST 0x30u           /* s32: the last stage made, -1 for none */
#define COMP_MEETINGS 0x3cu             /* s16 */
#define COMP_CLUBS 0x3eu                /* s16 */
#define COMP_YEAR 0x40u                 /* s16 */
#define STAGE_TYPE 0x43u                /* 1 Cup_Stage, 2 League_Stage */
#define COMP_TABLE 0xb1u
#define CUP_ENTRIES 0xa7u               /* Cup_Stage: entries of 6 bytes, +0 the club, +5 the result */
#define CUP_COMPLETE 0xb1u              /* Cup::check_complete 0x521e00 */
#define ROW_SIZE 0x3bu
#define ROW_POSITION 4u                 /* the place of the row in a stage's table */
#define ROW_POSITIONS 9u                /* the row's place after each match (season_split.c) */
#define ROW_STATUS 0x37u
#define CLUB_NAME 4u
#define CLUB_NATION 0x53u
#define CLUB_DIVISION 0x57u
#define CLUB_LAST_PLACE 0x5fu           /* s8: stored by 0x68aa30 at 0x68aa6d */
#define DATE_SIZE 0x41u
#define ROUND_SIZE 0x68u

#define ST_TITLE 5
#define ST_UP 0
#define ST_PLAYOFF 1
#define ST_DOWN 3
#define ST_NONE 0xff

#define SPLIT_AFTER 30                  /* the matches before Play-off I, each season of the form */
#define PLAYOFF_ONE 6                   /* the clubs of Play-off I */
#define GROUP_CLUBS 6
#define GROUP_ROUNDS 10
#define NAME_PLAYOFF 0xa0               /* the name of a play-off stage: BelSecond's group 0x41f9a8, England's 0x57316b */
#define CODE_FINAL 150                  /* the round id of a final (0x572b6e) */
#define LEG_GAP 4                       /* days between two legs, as the English semi-finals (0x572b20) */
#define MAX_ROWS 64
#define CLASS_SECOND_STAGES 4           /* BelSecond: three periods and the group (0x41f0a5) */
#define TEST_SAVE_GROUP_DAYS 14
#define CUP_WAIT_LAST_DAY 165           /* the Europa match waits for the cup final until this day of the year */
#define B_ROUNDS 28                     /* First Division B: 8 clubs that meet four times */
#define WINTER_DAY 353                  /* 20 December, the day of the year */
#define WINTER_BREAK 21                 /* days */
#define FINAL_AFTER 4                   /* days from First Division B's table to the first leg of its final */

uint32_t rt_bel_playoff_stage(uint32_t obj, uint32_t original);    /* season_sizes_periods.c */

void testrun_save_in(int days);    /* testrun.c */

static int readable(uint32_t p) { return rt_readable(p); }

static int on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_BEL_PLAYOFFS") && !strcmp(getenv("RECOMP_BEL_PLAYOFFS"), "off");
    return !off && season_sizes_on() && season_game_changes();
}

/* Tests only (tests/seasons/bel_playoffs_run.py): RECOMP_TEST_BEL_SAVE=group or tie saves while that stage is played. */
static int test_save(const char *what)
{
    const char *v = getenv("RECOMP_TEST_BEL_SAVE");
    return v && !strcmp(v, what);
}

/* One log line of an edition, with the day of the year (tests/seasons/bel_playoffs_run.py reads them). */
static void say(const char *fmt, int year, ...)
{
    va_list ap;
    fprintf(stderr, "recomp: bel playoffs: %d: day %d: ", year, (int16_t)rd16(g_mem, GAME_DATE));
    va_start(ap, year);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
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

static uint32_t league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return readable(league) ? league : 0;
}

static const char *club_name(uint32_t club) { return readable(club) ? win_str(club + CLUB_NAME) : "?"; }

static int row_count(uint32_t league)
{
    int n = readable(league) ? (int16_t)rd16(g_mem, league + COMP_CLUBS) : 0;
    return n > 0 && n <= MAX_ROWS ? n : 0;
}

static uint32_t row_at(uint32_t league, int i) { return rd32(g_mem, league + COMP_TABLE) + (uint32_t)i * ROW_SIZE; }

/* The row season of the First Division, or -1. */
static int first_season(uint32_t first) { return readable(first) ? season_sizes_row_season(first, "bel_first") : -1; }

/* 1 and the form of a First Division whose stages of this season were set up by season_bel_po_rules. */
static int live(uint32_t obj, SeasonBelForm *f)
{
    if (!on() || !readable(obj) || obj != league_of(KEY_FIRST)) return 0;
    return season_bel_form(first_season(obj), f) && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) >= 1;
}

static int after_split(uint32_t first) { return season_split_meetings(first) != (int16_t)rd16(g_mem, first + COMP_MEETINGS); }

/* The place (from 1) of a row after the matches before the split, 0 when it has none. */
static int place_at_split(uint32_t row)
{
    uint32_t at = rd32(g_mem, row + ROW_POSITIONS);
    return readable(at) ? (int8_t)g_mem[at + SPLIT_AFTER - 1] + 1 : 0;
}

static int place_of_club(uint32_t league, uint32_t club)
{
    for (int i = 0; i < row_count(league); i++)
        if (rd32(g_mem, row_at(league, i)) == club) return place_at_split(row_at(league, i));
    return 0;
}

/* The season ends with the stages made so far. */
static void no_more(uint32_t first, const char *why)
{
    int last = (int32_t)rd32(g_mem, first + COMP_STAGE_LAST);
    wr32(g_mem, first + COMP_STAGE_COUNT, (uint32_t)(last + 1));
    say("no more stages: %s\n", (int16_t)rd16(g_mem, first + COMP_YEAR), why);
}

int season_bel_po_rules(uint32_t obj)
{
    if (!readable(obj) || strcmp(season_change_file(obj), "bel_first")) return 0;
    SeasonBelForm f;
    if (!on() || !season_bel_form(first_season(obj), &f)) {
        wr32(g_mem, obj + COMP_STAGE_COUNT, 0);
        wr32(g_mem, obj + COMP_STAGE_LAST, 0xffffffffu);
        return 1;
    }
    uint32_t arr = rd32(g_mem, obj + COMP_STAGES);
    if (!readable(arr) || (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) < f.stages) {
        arr = season_sizes_game_malloc(4u * (uint32_t)f.stages);
        if (!arr) return 1;
        wr32(g_mem, obj + COMP_STAGES, arr);
    }
    for (int i = 0; i < f.stages; i++) wr32(g_mem, arr + 4u * (uint32_t)i, 0);
    wr32(g_mem, obj + COMP_STAGE_COUNT, (uint32_t)f.stages);
    wr32(g_mem, obj + COMP_STAGE_LAST, 0xffffffffu);
    return 1;
}

/* ---- Dates ---- */

static int leap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }

static void date_plus(int n, int *doy, int *year)
{
    int d = (int16_t)rd16(g_mem, GAME_DATE) + n, y = (int16_t)rd16(g_mem, GAME_DATE + 2);
    while (d >= 365 + leap(y)) d -= 365 + leap(y), y++;
    *doy = d;
    *year = y;
}

static void day_month(int doy, int year, int *day, int *month)
{
    static const int len[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int m = 0;
    while (m < 11 && doy >= len[m] + (m == 1 && leap(year))) doy -= len[m] + (m == 1 && leap(year)), m++;
    *day = doy + 1;
    *month = m;
}

static void set_dates(uint32_t table, int draw_in, int match_in, uint32_t year)
{
    int doy, y, day, month;
    date_plus(draw_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t d[7] = {table, 0, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY, year};
    call_cdecl(F_DRAW_DATE, 7, d);
    if (rt_unwinding) return;
    date_plus(match_in, &doy, &y);
    day_month(doy, y, &day, &month);
    uint32_t m[9] = {table, 0, (uint32_t)day, (uint32_t)month, (uint32_t)(y - (int)year), NO_WEEKDAY, 1, year, 0};
    call_cdecl(F_MATCH_DATE, 9, m);
}

/* Dates of a template that lie on or before today move on together, so that the first is tomorrow. +0 is the s16 day of
 * the year; the dates stay in their year. */
static void move_dates_on(uint32_t dates, int days)
{
    int today = (int16_t)rd16(g_mem, GAME_DATE), first = (int16_t)rd16(g_mem, dates);
    int last = (int16_t)rd16(g_mem, dates + (uint32_t)(days - 1) * DATE_SIZE);
    int shift = first <= today ? today + 1 - first : 0;
    if (shift <= 0 || last + shift >= 365) return;
    for (int k = 0; k < days; k++) {
        uint32_t d = dates + (uint32_t)k * DATE_SIZE;
        wr16(g_mem, d, (uint16_t)((int16_t)rd16(g_mem, d) + shift));
    }
}

/* ---- The groups of Play-off II with second-division clubs ---- */

static uint32_t make_group(uint32_t first, int index, const uint32_t *club, uint32_t dates)
{
    uint32_t list = season_sizes_game_malloc(4u * GROUP_CLUBS), places = season_sizes_game_malloc(4);
    uint32_t tmpl = season_sizes_game_malloc(GROUP_ROUNDS * DATE_SIZE), sz[1] = {0xee};
    uint32_t stage = list && places && tmpl ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return 0;
    if (readable(stage)) {
        for (int k = 0; k < GROUP_CLUBS; k++) wr32(g_mem, list + 4u * (uint32_t)k, club[k]);
        g_mem[places] = 1;          /* one place at the top of the group, as 0x420259 */
        memcpy(g_mem + tmpl, g_mem + dates, GROUP_ROUNDS * DATE_SIZE);
        uint32_t a[22] = {first, GROUP_CLUBS, list, 2, rd32(g_mem, first + COMP_RECORD), tmpl, GROUP_ROUNDS,
                          g_mem[first + 0xc2u], g_mem[first + 0xc3u], g_mem[first + 0xc4u], first + 0xc5u, places,
                          (uint32_t)(int16_t)rd16(g_mem, first + COMP_YEAR), (uint32_t)index, NAME_PLAYOFF, 0x14, 1, 0, 0x22,
                          0xffffffffu, 0, 2};     /* as 0x4203d4..0x420414 */
        stage = call_this(F_LEAGUE_STAGE, stage, 22, a);
        if (rt_unwinding) return 0;
    }
    season_sizes_game_free(tmpl);
    season_sizes_game_free(places);
    season_sizes_game_free(list);
    if (!readable(stage)) return 0;
    wr32(g_mem, rd32(g_mem, first + COMP_STAGES) + 4u * (uint32_t)index, stage);
    wr32(g_mem, first + COMP_STAGE_LAST, (uint32_t)index);
    return stage;
}

/* 1 when the second division's season is over, its final included. */
static int second_done(uint32_t second)
{
    uint32_t a[2] = {1, 1};
    return readable(second) && (call_this(rd32(g_mem, rd32(g_mem, second) + 0x10), second, 2, a) & 0xff);
}

static void build_groups(uint32_t first, const SeasonBelForm *f)
{
    uint32_t second = league_of(KEY_SECOND), group[2][GROUP_CLUBS];
    int have[2] = {0, 0}, year = (int16_t)rd16(g_mem, first + COMP_YEAR);
    if (!after_split(first) || !second_done(second) || rt_unwinding) return;
    for (int i = 0; i < row_count(first); i++) {
        int g = season_bel_group_of_place(place_at_split(row_at(first, i)));
        if (g >= 0 && have[g] < GROUP_CLUBS) group[g][have[g]++] = rd32(g_mem, row_at(first, i));
    }
    /* The best clubs of the second division's table, less the promoted club (status 0 or 5). */
    for (int i = 0, b = 0; i < row_count(second) && b < f->b_clubs; i++) {
        int s = g_mem[row_at(second, i) + ROW_STATUS], g = season_bel_group_of_b(b);
        if (s == ST_UP || s == ST_TITLE) continue;
        if (g >= 0 && have[g] < GROUP_CLUBS) group[g][have[g]++] = rd32(g_mem, row_at(second, i));
        b++;
    }
    if (have[0] != GROUP_CLUBS || have[1] != GROUP_CLUBS) {
        no_more(first, "the groups of Play-off II do not fill");
        return;
    }
    int days = 0;
    uint32_t code = 0, dates = season_sizes_split_dates(first, season_split_meetings(first), &days, &code);
    if (rt_unwinding) return;
    if (!dates || days < GROUP_ROUNDS) {
        season_sizes_game_free(dates);
        no_more(first, "no dates for the groups of Play-off II");
        return;
    }
    move_dates_on(dates, GROUP_ROUNDS);
    for (int g = 0; g < 2 && !rt_unwinding; g++) {
        if (!make_group(first, g, group[g], dates)) {
            if (!rt_unwinding) no_more(first, "a group was not made");
            break;
        }
        say("group %c:", year, 'A' + g);
        for (int k = 0; k < GROUP_CLUBS; k++) fprintf(stderr, "%s%.40s", k ? ", " : " ", club_name(group[g][k]));
        fprintf(stderr, "\n");
    }
    season_sizes_game_free(dates);
    if (!rt_unwinding && test_save("group")) testrun_save_in(TEST_SAVE_GROUP_DAYS);
}

void season_bel_po_update(uint32_t obj)
{
    SeasonBelForm f;
    if (live(obj, &f) && f.b_clubs && (int32_t)rd32(g_mem, obj + COMP_STAGE_LAST) < 0) build_groups(obj, &f);
}

/* ---- The two ties ---- */

/* A tie of two clubs at index: club[0] is at home in the first match (Cup_Stage::map_fixture_tree 0x5241e0). */
static uint32_t make_tie(uint32_t first, int index, const uint32_t *club, int legs)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, first + COMP_YEAR), table = season_sizes_game_malloc(ROUND_SIZE);
    uint32_t list = season_sizes_game_malloc(8), seeds = season_sizes_game_malloc(2), sz[1] = {0xb2};
    if (!table || !list || !seeds) return 0;
    memset(g_mem + table, 0, ROUND_SIZE);
    wr16(g_mem, table + 7, CODE_FINAL);
    wr16(g_mem, table + 0xd, (uint16_t)(legs == 2 ? 0x200 : 0x203));    /* fixed pairs; one match: extra time, penalties */
    wr16(g_mem, table + 0xf, legs == 2 ? 3 : 0);    /* two legs: both in the second leg (0x51a6b3) */
    g_mem[table + 0x17] = 6;
    wr16(g_mem, table + 0x18, 2);
    wr16(g_mem, table + 0x1a, 1);
    wr16(g_mem, table + 0x1c, 2);
    g_mem[table + 0x21] = (uint8_t)legs;
    g_mem[table + 0x22] = legs == 2 ? LEG_GAP : 0;
    set_dates(table, 1, 3, year);
    if (rt_unwinding) return 0;
    for (int i = 0; i < 2; i++) {
        wr32(g_mem, list + 4u * (uint32_t)i, club[i]);
        g_mem[seeds + (uint32_t)i] = (uint8_t)(i + 1);
    }
    uint32_t stage = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(stage)) return 0;
    uint32_t a[15] = {first, 2, list, 1, rd32(g_mem, first + COMP_RECORD), table, year, (uint32_t)index, 1, NAME_PLAYOFF,
                      0x14, 1, 0, 0, seeds};    /* as season_arg_stages.c make_cup */
    call_this(F_CUP_STAGE, stage, 15, a);
    season_sizes_game_free(list);
    season_sizes_game_free(seeds);
    season_sizes_game_free(table);
    if (rt_unwinding) return 0;
    wr32(g_mem, rd32(g_mem, first + COMP_STAGES) + 4u * (uint32_t)index, stage);
    uint32_t r0[1] = {0};
    call_this(F_SET_UP_ROUND, stage, 1, r0);
    if (rt_unwinding) return 0;
    wr32(g_mem, first + COMP_STAGE_LAST, (uint32_t)index);      /* only now: the save reads stages[0..+0x30] */
    return stage;
}

/* The clubs of a stage's table in the order of their places. */
static int stage_order(uint32_t stage, uint32_t *club)
{
    uint32_t z[1] = {0};
    call_this(F_SORT_TABLE, stage, 1, z);
    int n = row_count(stage), pos[MAX_ROWS];
    if (rt_unwinding || !n) return 0;
    for (int i = 0; i < n; i++) {
        uint32_t c = rd32(g_mem, row_at(stage, i));
        int v = g_mem[row_at(stage, i) + ROW_POSITION], j = i;
        for (; j > 0 && pos[j - 1] > v; j--) club[j] = club[j - 1], pos[j] = pos[j - 1];
        club[j] = c;
        pos[j] = v;
    }
    return n;
}

/* The winner of group g: from the table of the First Division, or from its stage. A second-division club does not go
 * on (no such club ever won a group): the best First Division club of the group does. */
static uint32_t group_winner(uint32_t first, const SeasonBelForm *f, int g)
{
    if (!f->b_clubs) {
        for (int i = 0; i < row_count(first); i++)
            if (season_bel_group_of_place(place_at_split(row_at(first, i))) == g) return rd32(g_mem, row_at(first, i));
        return 0;
    }
    uint32_t club[MAX_ROWS], stage = rd32(g_mem, rd32(g_mem, first + COMP_STAGES) + 4u * (uint32_t)g);
    int n = readable(stage) ? stage_order(stage, club) : 0;
    for (int i = 0; i < n; i++)
        if (rd32(g_mem, club[i] + CLUB_DIVISION) == rd32(g_mem, first + COMP_RECORD)) return club[i];
    return 0;
}

/* The winner and the loser of a complete tie. */
static int tie_result(uint32_t stage, uint32_t *win, uint32_t *lose)
{
    if (!readable(stage) || g_mem[stage + STAGE_TYPE] != 1 || !g_mem[stage + CUP_COMPLETE]) return 0;
    uint32_t e = rd32(g_mem, stage + CUP_ENTRIES);
    if (!readable(e)) return 0;
    int first_won = g_mem[e + 5] == 1;
    if (!first_won && g_mem[e + 6 + 5] != 1) return 0;
    *win = rd32(g_mem, e + (first_won ? 0u : 6u));
    *lose = rd32(g_mem, e + (first_won ? 6u : 0u));
    return 1;
}

static void build_final(uint32_t first, const SeasonBelForm *f, int index)
{
    uint32_t a = group_winner(first, f, 0), b = group_winner(first, f, 1);
    if (rt_unwinding) return;
    if (!readable(a) || !readable(b)) {
        no_more(first, "no group winners");
        return;
    }
    /* Two legs: the winner of group A at home in the second leg. One match: at the club higher after the 30 rounds. */
    int a_home = f->final_legs == 1 && place_of_club(first, a) < place_of_club(first, b);
    uint32_t club[2] = {a_home ? a : b, a_home ? b : a};
    if (!make_tie(first, index, club, f->final_legs) && !rt_unwinding) no_more(first, "the final was not made");
    if (!rt_unwinding && test_save("tie")) testrun_save_in(1);
    say("Play-off II final, %d leg(s): %.40s v %.40s\n",
            (int16_t)rd16(g_mem, first + COMP_YEAR), f->final_legs, club_name(club[0]), club_name(club[1]));
}

static void build_europa(uint32_t first, const SeasonBelForm *f, int index)
{
    uint32_t win = 0, lose = 0, stages = rd32(g_mem, first + COMP_STAGES);
    int year = (int16_t)rd16(g_mem, first + COMP_YEAR);
    if (!tie_result(rd32(g_mem, stages + 4u * (uint32_t)(index - 1)), &win, &lose)) {
        no_more(first, "the final has no winner");
        return;
    }
    /* The honours of the cup final are written after the day's league updates (0x51f920): the draw waits for them. */
    uint32_t nation = rd32(g_mem, win + CLUB_NATION);
    int played = season_uefa_cup_final_played(nation);
    if (rt_unwinding || (!played && (int16_t)rd16(g_mem, GAME_DATE) < CUP_WAIT_LAST_DAY)) return;
    uint32_t cup = season_uefa_cup_winner(nation);
    if (rt_unwinding) return;
    /* 2010/11: the Play-off II winner held a European place from the cup, and the match was not played. */
    if (cup == win) {
        no_more(first, "the Play-off II winner has the cup winner's place");
        return;
    }
    int cup_place = 0;
    for (int i = 0; i < PLAYOFF_ONE && i < row_count(first); i++)
        if (rd32(g_mem, row_at(first, i)) == cup) cup_place = i + 1;
    uint32_t top = rd32(g_mem, row_at(first, season_bel_europa_place(first_season(first), cup_place) - 1));
    /* Two legs: the Play-off I club at home in the second leg. One match: at the Play-off I club. */
    uint32_t club[2] = {f->europa_legs == 2 ? win : top, f->europa_legs == 2 ? top : win};
    if (!make_tie(first, index, club, f->europa_legs) && !rt_unwinding) no_more(first, "the Europa match was not made");
    say("Europa League play-off, %d leg(s): %.40s v %.40s (cup winner's place %d)\n",
            year, f->europa_legs, club_name(club[0]), club_name(club[1]), cup_place);
}

int season_bel_po_advance(uint32_t obj)
{
    SeasonBelForm f;
    if (!live(obj, &f)) return 0;
    int last = (int32_t)rd32(g_mem, obj + COMP_STAGE_LAST), first_tie = f.stages - 2;
    if (!after_split(obj)) {
        no_more(obj, "the table did not split");
        return 1;
    }
    if (last < first_tie - 1) {
        build_groups(obj, &f);
        return 1;
    }
    uint32_t stages = rd32(g_mem, obj + COMP_STAGES), a[2] = {0, 0};
    for (int i = 0; i <= last; i++) {
        uint32_t s = rd32(g_mem, stages + 4u * (uint32_t)i);
        if (!readable(s) || !(call_this(rd32(g_mem, rd32(g_mem, s) + 0x10), s, 2, a) & 0xff) || rt_unwinding) return 1;
    }
    if (last + 1 == first_tie) build_final(obj, &f, last + 1);
    else if (last + 1 == first_tie + 1) build_europa(obj, &f, last + 1);
    return 1;
}

int season_bel_po_result(uint32_t obj, int stage)
{
    SeasonBelForm f;
    return stage != -1 && live(obj, &f);
}

int season_bel_po_unfinished(int e, int year, const char **file)
{
    uint32_t first = league_of(KEY_FIRST), second = league_of(KEY_SECOND), buf[ENTRY_COMPS], a[2] = {1, 1};
    int row[2], season = first_season(first);
    SeasonBelForm f;
    if (!on() || !first || !second || (int16_t)rd16(g_mem, first + COMP_YEAR) >= year) return 0;
    if (!live(first, &f) && !season_bel_top_entrants(season, row_count(first), row)) return 0;
    int n = season_change_entry_comps(e, buf, ENTRY_COMPS), seen = 0;
    for (int i = 0; i < n; i++) seen |= buf[i] == first;
    if (!seen) return 0;
    for (int k = 0; k < 2 && !rt_unwinding; k++) {
        uint32_t lg = k ? second : first;
        if (!(call_this(rd32(g_mem, rd32(g_mem, lg) + 0x10), lg, 2, a) & 0xff)) {
            *file = k ? "bel_second" : "bel_first";
            return 2;
        }
    }
    return 1;
}

/* ---- The European place ---- */

void o_0068aa30(void);
RT_WRAP(0068aa30, league_final_places)
{
    uint32_t self = cpu.ecx;
    o_0068aa30();
    SeasonBelForm f;
    if (rt_unwinding || !live(self, &f)) return;
    int last = (int32_t)rd32(g_mem, self + COMP_STAGE_LAST);
    uint32_t win = 0, lose = 0;
    if (last != f.stages - 1 || !tie_result(rd32(g_mem, rd32(g_mem, self + COMP_STAGES) + 4u * (uint32_t)last), &win, &lose))
        return;
    if (place_of_club(self, win) <= PLAYOFF_ONE) {
        say("%.40s keeps its European place\n", (int16_t)rd16(g_mem, self + COMP_YEAR), club_name(win));
        return;
    }
    uint8_t keep = g_mem[win + CLUB_LAST_PLACE];
    g_mem[win + CLUB_LAST_PLACE] = g_mem[lose + CLUB_LAST_PLACE];
    g_mem[lose + CLUB_LAST_PLACE] = keep;
    say("places for Europe: %.40s %d, %.40s %d\n", (int16_t)rd16(g_mem, self + COMP_YEAR), club_name(win),
        (int8_t)g_mem[win + CLUB_LAST_PLACE], club_name(lose), (int8_t)g_mem[lose + CLUB_LAST_PLACE]);
    say("%.40s takes the European place of %.40s\n",
            (int16_t)rd16(g_mem, self + COMP_YEAR), club_name(win), club_name(lose));
}

/* ---- The second division's final round ---- */

/* The First Division clubs of the final round, in out (at most 2). *loser gets the relegation play-off's loser. */
static int entrants_of(uint32_t first, uint32_t *out, uint32_t *loser)
{
    int row[2], n = row_count(first), k = season_bel_top_entrants(first_season(first), n, row);
    *loser = 0;
    if (k == SEASON_BEL_PO3_WINNER) {
        /* The table order of the last section: the winner stands above the loser. */
        for (int i = 0; i < n; i++) {
            if (place_at_split(row_at(first, i)) < n - 1) continue;
            if (!out[0] || k == SEASON_BEL_PO3_WINNER) {
                out[0] = rd32(g_mem, row_at(first, i));
                k = 1;
            } else {
                *loser = rd32(g_mem, row_at(first, i));
            }
        }
        return *loser ? 1 : 0;
    }
    for (int i = 0; i < k; i++) out[i] = rd32(g_mem, row_at(first, row[i]));
    return k;
}

static int first_table_done(uint32_t first)
{
    uint32_t a[2] = {0, 0};
    return readable(first) && (call_this(rd32(g_mem, rd32(g_mem, first) + 0x10), first, 2, a) & 0xff);
}

/* BelSecond::advance_stage 0x4201a0, thiscall, no arguments: the final round waits for the First Division's table.
 * League::update_for_date 0x6847c0 calls it again the next day (0x6849e3). */
void o_004201a0(void);
RT_WRAP(004201a0, bel_second_advance)
{
    uint32_t self = cpu.ecx, first = league_of(KEY_FIRST);
    int row[2], next = (int32_t)rd32(g_mem, self + COMP_STAGE_LAST) + 1;
    /* 2015/16: no final round, the champion alone goes up. The season closes with the stages it has (0x6849d1); the
     * class's count of 4 (0x41f0a5) comes back after the chain, season_bel_po_moved. */
    if (on() && self == league_of(KEY_SECOND) && first && next == (int)rt_bel_playoff_stage(self, 3) &&
        season_bel_second_up(first_season(first)) == SEASON_BEL_UP_CHAMPION) {
        wr32(g_mem, self + COMP_STAGE_COUNT, (uint32_t)next);
        say("no final round this season\n", (int16_t)rd16(g_mem, self + COMP_YEAR));
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    if (on() && self == league_of(KEY_SECOND) && first &&
        (int32_t)rd32(g_mem, self + COMP_STAGE_LAST) + 1 == (int)rt_bel_playoff_stage(self, 3) &&
        season_bel_top_entrants(first_season(first), row_count(first), row) && !first_table_done(first)) {
        if (!rt_unwinding) rt_ret_cdecl(0);
        return;
    }
    o_004201a0();
}

static struct { uint32_t row, club; } carried[2];
static int ncarried;

/* First Division B 2016/17 to 2018/19: the 28 rounds ended before the promotion final of early March (first legs on 5
 * March 2017, 4 March 2018 and 9 March 2019; RSSSF belg2017 to belg2019), so that its clubs could join Play-off II. The
 * class's dates reach into May. One round a week from the first date, with a winter break from 20 December. */
static void weekly_rounds(uint32_t tmpl, int rounds, int base)
{
    int start = season_template_day(base, (int16_t)rd16(g_mem, tmpl + 2), (int16_t)rd16(g_mem, tmpl));
    int winter = season_template_day(base, 0, WINTER_DAY);
    for (int r = 0; r < rounds; r++) {
        uint32_t to = tmpl + (uint32_t)r * DATE_SIZE;
        int day = start + 7 * r, yoff, doy;
        if (day >= winter) day += WINTER_BREAK;
        if (r) memcpy(g_mem + to, g_mem + tmpl, DATE_SIZE);
        season_template_split(base, day, &yoff, &doy);
        wr16(g_mem, to, (uint16_t)doy);
        wr16(g_mem, to + 2, (uint16_t)yoff);
    }
}

/* BelSecond::create_schedule 0x41f340 (thiscall: stage, &count, &code, &flag). The final round that waited for the First
 * Division takes dates from tomorrow on. The class dates its play-off group in May (stage 3: 0x41f975..0x41f9ff). First Division
 * B's final of 2016/17 to 2018/19 comes soon after its table instead (real life: 5 and 11 March 2017), as Play-off II
 * waits for that final and its clubs: the dates move up to start in FINAL_AFTER days. */
void o_0041f340(void);
RT_WRAP(0041f340, bel_second_schedule)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff);
    o_0041f340();
    if (rt_unwinding) return;
    season_sizes_schedule(self, stage, count_at);
    int days = readable(count_at) ? (int16_t)rd16(g_mem, count_at) : 0;
    SeasonBelForm b;
    if (!rt_unwinding && stage == -1 && on() && readable(cpu.eax) && days == B_ROUNDS &&
        season_bel_form(season_sizes_row_season(self, "bel_second"), &b) && b.b_clubs) {
        weekly_rounds(cpu.eax, days, (int16_t)rd16(g_mem, self + COMP_YEAR));
        return;
    }
    if (rt_unwinding || stage != (int)rt_bel_playoff_stage(self, 3) || !readable(cpu.eax) || days <= 0) return;
    SeasonBelForm f;
    if (ncarried) move_dates_on(cpu.eax, days);
    else if (live(league_of(KEY_FIRST), &f) && f.b_clubs) {
        int shift = (int16_t)rd16(g_mem, GAME_DATE) + FINAL_AFTER - (int16_t)rd16(g_mem, cpu.eax);
        for (int k = 0; shift < 0 && k < days; k++) {
            uint32_t d = cpu.eax + (uint32_t)k * DATE_SIZE;
            wr16(g_mem, d, (uint16_t)((int16_t)rd16(g_mem, d) + shift));
        }
        say("First Division B's final moves %d days\n", (int16_t)rd16(g_mem, self + COMP_YEAR), shift);
    }
}

void season_bel_po_entrants_in(uint32_t second)
{
    uint32_t first = league_of(KEY_FIRST), club[2] = {0, 0}, loser;
    ncarried = 0;
    if (!on() || !first || second != league_of(KEY_SECOND)) return;
    int k = entrants_of(first, club, &loser), n = row_count(second);
    if (k <= 0) return;
    /* Room for the entrants: the lowest rows of status 1 beyond the second division's clubs give up their place. */
    int have = 0;
    for (int i = 0; i < n; i++) have += g_mem[row_at(second, i) + ROW_STATUS] == ST_PLAYOFF;
    for (int i = n - 1; i >= 0 && have > 4 - k; i--)
        if (g_mem[row_at(second, i) + ROW_STATUS] == ST_PLAYOFF) {
            g_mem[row_at(second, i) + ROW_STATUS] = ST_NONE;
            have--;
        }
    for (int i = n - 1; i >= 0 && ncarried < k; i--) {
        uint32_t row = row_at(second, i);
        if (g_mem[row + ROW_STATUS] != ST_NONE) continue;
        carried[ncarried].row = row;
        carried[ncarried].club = rd32(g_mem, row);
        wr32(g_mem, row, club[ncarried]);
        g_mem[row + ROW_STATUS] = ST_PLAYOFF;
        say("%.40s of the First Division plays the final round\n",
                (int16_t)rd16(g_mem, first + COMP_YEAR), club_name(club[ncarried]));
        ncarried++;
    }
}

void season_bel_po_entrants_out(void)
{
    for (int i = 0; i < ncarried; i++) {
        wr32(g_mem, carried[i].row, carried[i].club);
        g_mem[carried[i].row + ROW_STATUS] = ST_NONE;
    }
    ncarried = 0;
}

/* The First Division clubs that the final round decided, for the line after the chain. */
static struct { uint32_t club; int down; } decided[3];
static int ndecided;

void season_bel_po_settle(void)
{
    uint32_t first = league_of(KEY_FIRST), second = league_of(KEY_SECOND), club[2] = {0, 0}, loser = 0;
    ndecided = 0;
    if (!on() || !first || !second) return;
    /* First Division B 2016/17 to 2019/20: the winner of the final goes up (status 0), and the table's first place
     * (status 5) does not, unless it is that club. */
    if (season_bel_second_up(first_season(first)) == SEASON_BEL_UP_FINAL) {
        int won = 0;
        for (int i = 0; i < row_count(second); i++) won |= g_mem[row_at(second, i) + ROW_STATUS] == ST_UP;
        for (int i = 0; won && i < row_count(second); i++)
            if (g_mem[row_at(second, i) + ROW_STATUS] == ST_TITLE) g_mem[row_at(second, i) + ROW_STATUS] = ST_NONE;
    }
    int k = entrants_of(first, club, &loser), year = (int16_t)rd16(g_mem, first + COMP_YEAR);
    if (k <= 0) return;
    int index = (int)rt_bel_playoff_stage(second, 3);
    uint32_t stage = (int32_t)rd32(g_mem, second + COMP_STAGE_LAST) >= index
                         ? rd32(g_mem, rd32(g_mem, second + COMP_STAGES) + 4u * (uint32_t)index) : 0, order[MAX_ROWS];
    if (!readable(stage) || !stage_order(stage, order)) {
        say("no final round was played; the statuses stay\n", year);
        return;
    }
    int winner = -1;
    for (int i = 0; i < k; i++)
        if (order[0] == club[i]) winner = i;
    for (int i = 0; i < row_count(first); i++) {
        uint32_t row = row_at(first, i), c = rd32(g_mem, row);
        int down = c == loser;
        if (down) g_mem[row + ROW_STATUS] = ST_DOWN;
        for (int e = 0; e < k; e++)
            if (c == club[e]) {
                down = season_bel_final_round_down(k, winner, e);
                g_mem[row + ROW_STATUS] = down ? ST_DOWN : ST_NONE;
            }
        if ((c == loser || c == club[0] || (k > 1 && c == club[1])) && ndecided < 3) {
            decided[ndecided].club = c;
            decided[ndecided++].down = down;
        }
    }
    say("the final round is won by %.40s (%s)\n", year, club_name(order[0]),
            winner >= 0 ? "the First Division club stays up" : "a second-division club goes up");
}

void season_bel_po_moved(void)
{
    uint32_t first = league_of(KEY_FIRST), second = league_of(KEY_SECOND);
    if (second && (int32_t)rd32(g_mem, second + COMP_STAGE_COUNT) == CLASS_SECOND_STAGES - 1)
        wr32(g_mem, second + COMP_STAGE_COUNT, CLASS_SECOND_STAGES);
    for (int i = 0; first && i < ndecided; i++) {
        int up = rd32(g_mem, decided[i].club + CLUB_DIVISION) == rd32(g_mem, first + COMP_RECORD);
        say("%.40s %s the First Division next season%s\n", (int16_t)rd16(g_mem, first + COMP_YEAR),
            club_name(decided[i].club), up ? "stays in" : "leaves", up == decided[i].down ? " (fault)" : "");
    }
    ndecided = 0;
}

int season_bel_po_holds(uint32_t obj)
{
    SeasonBelForm f;
    return live(obj, &f);
}
