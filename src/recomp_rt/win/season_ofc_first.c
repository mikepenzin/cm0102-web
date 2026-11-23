/* The preliminary round of OFC World Cup qualifying (item 198 of 0.6.2). Changing rules on only (season_intl_on),
 * for the World Cups from 2006: the base game, a game with changing rules off and the 2002 campaign of a 2001 game
 * do not change (Q152).
 *
 * The original (wc_oceania_league, class WcOceaniaLeague, vtable 0x970d78). The pool builder 0x91fa80 takes 10
 * entrants less the exempt count +0xee (0x91fa95, 0x91fbda: the hosts of find_hosts and the holder of the
 * continent), reads the Oceania nations (continent 0x9cfa18) in nation-table order and leaves out two key nations:
 * New Caledonia [0x9cf530] at 0x91fc89 and Papua New Guinea [0x9cf41c] at 0x91fc93. It stops at the count
 * (0x91fc9b) and sorts the list +0xef by reputation (qsort 0x945333 with 0x4b07d0 at 0x91fd04). Since Australia is
 * in Asia, our site at 0x91fc89 (rt_intl_oceania_left_out) lets New Caledonia in, so with the 11 Oceania nations of
 * the data the 10 places are full and Papua New Guinea never plays (open item 198: its last match in a long game
 * is its last OFC Nations Cup). The ctor 0x91df90 and start_new_season 0x91ef40 call the pool (0x91e06c,
 * 0x91eff6), the predicted positions (vtable +0x5c), set_up_rules 0x91ecf0 and init_group_table 0x91ec00, which
 * fills group A, the League object itself, with the even places of the list (0x91eca1..0x91ecb9: row r takes
 * place 2r, init_table_row 0x687430). On the group draw day (+0xb6 day of the year +0x40 + +0xb8: 15 January
 * of +0x40 from set_up_rules, 0x91ed25..0x91ed7c, day 16 with the season date shifts; the compare 0x91f428..0x91f484) update_for_date 0x91f3d0 makes the fixtures and the
 * second group from the odd places (create_second_group_stage 0x91f1a0 at 0x91f5fd) when +0x45 is 0 and the
 * update is full (0x91f492..0x91f4a6).
 *
 * The change:
 * - The pool (season_ofc_first_pool, from the pool wrap in season_wcq26.c, after the original): every Oceania
 *   nation whose national team names it, less the hosts of the campaign's World Cup (season_intl_host_ids, as the
 *   AFC first round; the holder is off in a season game, rt_intl_wc_holder), sorted by the original's compare.
 *   P of them and G places (+0xf3): when P > G the list becomes the best G in that order, so the original's
 *   groups, predictions and draw take them as before; the W = P - G weakest wait outside it.
 * - The round (from OFC_FIRST_LEAD days before the group draw and not before the section's own season start of
 *   that year, item 245, season_started; on the first update day while +0x45 is 0 and +0x30 is -1): the W
 *   outside play two-leg ties against the W weakest of the list (season_ofc_first_plan, as the AFC
 *   first round of item 196, season_afc_first.c) on FIFA days (season_ofc_first_days), as a Cup_Stage (0x522e00)
 *   in slot 0 with +0x30 = 0. The legs come in the autumn before the group draw (September of +0x40 - 1: real
 *   2018 cycle, 31 August to 4 September 2015). While slot 0 holds the ties the wrap of update_for_date calls the stage's update
 *   (vtable +0x14), and advance_stage 0x91f0d0 waits (season_wcq26.c asks season_ofc_first_waiting).
 * - On the group draw day, before the original draws: each tie's winner takes its seed's place in the list, and
 *   when that place is even, its row of group A (init_table_row, as 0x91ecb9); the stage is freed (vtable +0
 *   with 1, as start_new_season 0x91ef99), slot 0 cleared and +0x30 is -1.
 *
 * Save and load: League save 0x6891c0 writes stages 0 to +0x30 (a Cup_Stage as type 1) and League load 0x687b10
 * reads them back, so the ties come back with +0x30 = 0 (as item 196). The list +0xef and its count +0xf3 are
 * saved and loaded by 0x91fd20 and its save partner: the seeds are teams, so the save keeps the original's shape.
 *
 * The status of a tie's team (Cup 0x519690 passes stage 0 to the section's status 0x91e840): a winner (status 1)
 * gets nothing (0x91e847), a loser (0xfe) the record of a first-stage loser (0x86c1d0 with 0x41c at 0x91e892) and
 * its competition cleared (0x775000 at 0x91e8a1). */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_intl.h"
#include "season_ofc_first.h"
#include "season_uefa_days.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VT_OCEANIA 0x00970d78u
#define VT_CUP_STAGE 0x00969468u
#define KEY_WORLD_CUP 0x9cf79cu
#define CONTINENT_OCEANIA 0x9cfa18u
#define NATIONS 0xae23a8u
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define GAME_DATE 0xae2c90u             /* s16 day of the year (0 is 1 January), s16 year */
#define NATIONAL_TEAMS 0xb74318u

#define GAME_NEW 0x00944cf1u
#define GAME_MALLOC 0x00944e46u
#define GAME_FREE 0x009452cau
#define GAME_QSORT 0x00945333u
#define CLUB_COMPARE 0x004b07d0u        /* the sort of the pool, 0x91fd04 */
#define CUP_STAGE_NEW 0x00522e00u
#define CUP_START 0x0051c800u           /* Cup::set_up_round */
#define CUP_DRAW_DATE 0x00521e60u       /* cdecl (rec, round, day, month0, yoff, weekday, year) */
#define CUP_MATCH_DATE 0x00521eb0u      /* cdecl (rec, round, day, month0, yoff, weekday, time, year, venue) */
#define NATIONAL_TEAM 0x0053b3d0u
#define INIT_TABLE_ROW 0x00687430u      /* League::init_table_row, thiscall (row byte, team, 0, 0) (0x91eca7..0x91ecb9) */
#define STATS_SET_POSITION 0x004a2540u
#define SET_CURRENT_COMP 0x00775220u

#define F_RECORD 0x4u
#define F_STATS 0x8u
#define F_STAGES 0xcu
#define F_STAGE_COUNT 0x2cu
#define F_STAGE 0x30u
#define F_TEAMS 0x3eu
#define F_YEAR 0x40u
#define F_DRAWN 0x45u
#define F_DRAW_DAY 0xb6u
#define F_DRAW_YEAR 0xb8u
#define F_LIST 0xefu
#define F_LIST_COUNT 0xf3u
#define F_CUP_ROWS 0xa7u
#define F_TABLE 0xb1u                   /* group A's table rows (0x91ec1c) */
#define ROW_SIZE 0x3bu                  /* a table row (0x6874f4..0x68751b) */
#define ROUND_SIZE 0x68u
#define ROUND_CODE 0xau                 /* a first round (season_afc_first.c) */

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_INTL_LOG");
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

static uint32_t test_stage;               /* item 245 test: the preliminary round's stage */
static long test_first_day;

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + F_YEAR); }

static const char *team_name(uint32_t club)
{
    uint32_t nation = readable(club) ? rd32(g_mem, club + 0x53u) : 0;
    return readable(nation) ? win_str(nation + 4u) : "none";
}

/* The day being updated: the Date argument of update_for_date (s16 day of the year, s16 year: the words that the
 * original compares at 0x91f42d..0x91f484), else the game date. */
static uint32_t update_date;

static long today_abs(void)
{
    uint32_t d = update_date && readable(update_date) ? update_date : GAME_DATE;
    return season_abs_day((int16_t)rd16(g_mem, d + 2), 1, 1) + (int16_t)rd16(g_mem, d);
}

static int on(uint32_t self)
{
    return season_intl_on() && readable(self) && rd32(g_mem, self) == VT_OCEANIA && year_of(self) + 2 >= 2006;
}

/* The group draw day of the cycle, as days from 1 January 1970 (update_for_date's compare, 0x91f405..0x91f428). */
static long group_draw_abs(uint32_t self)
{
    int y = year_of(self) + (int16_t)rd16(g_mem, self + F_DRAW_YEAR);
    return season_abs_day(y, 1, 1) + (int16_t)rd16(g_mem, self + F_DRAW_DAY);
}

/* 1 when the stage array is there to hold slot 0 (the ctor's malloc 0x91e0f9, start_new_season's 0x91f00b). */
static int slot0_array(uint32_t self)
{
    return readable(rd32(g_mem, self + F_STAGES)) && (int)rd32(g_mem, self + F_STAGE_COUNT) >= 1;
}

static uint32_t slot0(uint32_t self)
{
    uint32_t stages = rd32(g_mem, self + F_STAGES);
    if (!readable(stages) || (int)rd32(g_mem, self + F_STAGE_COUNT) < 1) return 0;
    uint32_t s = rd32(g_mem, stages);
    return readable(s) ? s : 0;
}

/* 1 while slot 0 holds the ties: before the draw (+0x45 0), +0x30 0 and a Cup_Stage in slot 0. */
static int ties_live(uint32_t self)
{
    if (!on(self) || rd32(g_mem, self + F_DRAWN) != 0 || (int32_t)rd32(g_mem, self + F_STAGE) != 0) return 0;
    uint32_t s = slot0(self);
    return s && rd32(g_mem, s) == VT_CUP_STAGE;
}

int season_ofc_first_waiting(uint32_t self) { return ties_live(self); }

/* The Oceania nations that can enter, sorted best first by the original's compare, into out (max). */
static int oceania_pool(uint32_t self, uint32_t *out, int max)
{
    uint32_t nations = rd32(g_mem, NATIONS);
    int count = (int)rd32(g_mem, NATION_COUNT), hosts[6], n = 0;
    if (!readable(nations) || count <= 0 || count > 1000) return 0;
    int n_hosts = season_intl_host_ids(rd32(g_mem, KEY_WORLD_CUP), year_of(self) + 2, hosts);
    for (int id = 0; id < count && n < max; id++) {
        uint32_t nation = nations + (uint32_t)id * NATION_SIZE, c = rd32(g_mem, nation + 0x71u);
        if (!readable(c) || rd32(g_mem, c) != rd32(g_mem, CONTINENT_OCEANIA)) continue;
        int host = 0;
        for (int k = 0; k < n_hosts; k++) host |= hosts[k] == id;
        if (host) continue;
        uint32_t a[1] = {nation};
        uint32_t team = call_cdecl(NATIONAL_TEAM, 1, a);
        if (rt_unwinding) return 0;
        if (!readable(team) || rd32(g_mem, team + 0x53u) != nation) continue;
        out[n++] = team;
    }
    if (n < 2) return n;
    uint32_t arr = game_malloc((uint32_t)n * 4u);
    if (rt_unwinding || !arr) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, arr + 4u * (uint32_t)i, out[i]);
    uint32_t q[4] = {arr, (uint32_t)n, 4, CLUB_COMPARE};
    call_cdecl(GAME_QSORT, 4, q);
    if (rt_unwinding) return 0;
    for (int i = 0; i < n; i++) out[i] = rd32(g_mem, arr + 4u * (uint32_t)i);
    game_free(arr);
    return n;
}

static int in_list(uint32_t self, uint32_t team)
{
    uint32_t list = rd32(g_mem, self + F_LIST);
    int count = (int)rd32(g_mem, self + F_LIST_COUNT);
    for (int i = 0; readable(list) && i < count && i < OFC_FIRST_MAX_POOL; i++)
        if (rd32(g_mem, list + 4u * (uint32_t)i) == team) return i;
    return -1;
}

/* After the original pool builder (season_wcq26.c pool wrap): the list becomes the best G of every Oceania nation
 * (head comment). */
void season_ofc_first_pool(uint32_t self)
{
    if (!on(self)) return;
    uint32_t pool[OFC_FIRST_MAX_POOL], list = rd32(g_mem, self + F_LIST);
    int places = (int)rd32(g_mem, self + F_LIST_COUNT);
    if (!readable(list) || places <= 0 || places > OFC_FIRST_MAX_POOL / 2) return;
    int p = oceania_pool(self, pool, OFC_FIRST_MAX_POOL);
    if (rt_unwinding || p < places) return;
    for (int i = 0; i < places; i++) wr32(g_mem, list + 4u * (uint32_t)i, pool[i]);
    if (logging()) {
        fprintf(stderr, "recomp: season intl: wc qualifying %d: OFC pool of %d for %d places", year_of(self) + 2, p,
                places);
        if (p > places) {
            fputs(", preliminary round for", stderr);
            for (int i = places; i < p; i++) fprintf(stderr, " %s;", team_name(pool[i]));
        }
        fputc('\n', stderr);
    }
}

/* The round record (dates as years from y): one two-leg round of 2W teams in the list's pairs (season_afc_first.c round_record: the
 * fields of the Asian section's two-leg tie, stage 12 0x9124ac..0x9124ed, with bit 0x200 that keeps the pairs). */
static uint32_t round_record(int y0, int teams, long draw, long leg1, long leg2)
{
    uint32_t rec = game_malloc(ROUND_SIZE);
    if (rt_unwinding || !rec) return 0;
    int y, m, d;
    season_from_abs_day(draw, &y, &m, &d);
    uint32_t a[7] = {rec, 0, (uint32_t)d, (uint32_t)(m - 1), (uint32_t)(y - y0), 0xffffffffu, (uint32_t)y0};
    call_cdecl(CUP_DRAW_DATE, 7, a);
    if (rt_unwinding) return 0;
    season_from_abs_day(leg1, &y, &m, &d);
    uint32_t b[9] = {rec, 0, (uint32_t)d, (uint32_t)(m - 1), (uint32_t)(y - y0), 0xffffffffu, 1, (uint32_t)y0, 0};
    call_cdecl(CUP_MATCH_DATE, 9, b);
    if (rt_unwinding) return 0;
    wr16(g_mem, rec + 7u, ROUND_CODE);
    wr16(g_mem, rec + 9u, 0);
    wr16(g_mem, rec + 0xbu, 0);
    wr16(g_mem, rec + 0xdu, 0x200u);
    wr16(g_mem, rec + 0xfu, 3u);
    wr8(g_mem, rec + 0x17u, 0xa);
    wr16(g_mem, rec + 0x18u, (uint32_t)teams);
    wr16(g_mem, rec + 0x1au, (uint32_t)teams / 2u);
    wr16(g_mem, rec + 0x1cu, (uint32_t)teams);
    wr16(g_mem, rec + 0x1eu, 0);
    wr8(g_mem, rec + 0x20u, 0);
    wr8(g_mem, rec + 0x21u, 2);
    wr8(g_mem, rec + 0x22u, (uint32_t)(leg2 - leg1));
    return rec;
}

/* Makes the round of the cycle (head comment). Returns 1 when slot 0 holds the ties. */
static int build(uint32_t self)
{
    static int logged_year;
    int c = year_of(self), places = (int)rd32(g_mem, self + F_LIST_COUNT);
    long today = today_abs(), draw, leg1, leg2;
    uint32_t pool[OFC_FIRST_MAX_POOL], outs[OFC_FIRST_MAX_POOL], seeds[OFC_FIRST_MAX_POOL];
    if (places <= 0 || places > OFC_FIRST_MAX_POOL / 2 || !slot0_array(self)) return 0;
    int p = oceania_pool(self, pool, OFC_FIRST_MAX_POOL);
    if (rt_unwinding) return 0;
    /* The nations outside the list (the pool's order, best first), and as many of the list's weakest. */
    int w = 0, s = 0;
    for (int i = 0; i < p && w < places; i++)
        if (in_list(self, pool[i]) < 0) outs[w++] = pool[i];
    for (int i = p - 1; i >= 0 && s < w; i--)
        if (in_list(self, pool[i]) >= 0) seeds[s++] = pool[i];     /* weakest first */
    w = s;
    int dated = w > 0 && season_ofc_first_days(group_draw_abs(self), today, &draw, &leg1, &leg2);
    if (!dated) {
        if (logging() && logged_year != c) {
            logged_year = c;
            fprintf(stderr, "recomp: season intl: wc qualifying %d: OFC pool of %d for %d places: %s\n", c + 2, p,
                    places, w > 0 ? "no FIFA window before the group draw, no preliminary round" : "no preliminary "
                                                                                                   "round");
        }
        return 0;
    }
    /* Tie k: the k-th best seed against the k-th weakest outside (season_ofc_first_plan). */
    uint32_t teams = game_malloc((uint32_t)(2 * w) * 4u);
    if (rt_unwinding || !teams) return 0;
    for (int k = 0; k < w; k++) {
        wr32(g_mem, teams + 8u * (uint32_t)k, seeds[w - 1 - k]);
        wr32(g_mem, teams + 8u * (uint32_t)k + 4u, outs[w - 1 - k]);
    }
    /* The stage's year: the year of the round's draw. Cup::process_fixture leaves out a fixture of a year before
     * the stage's year +0x40 (0x51a29e..0x51a2aa), and the legs come in the year before the campaign year (the
     * group draw is on 17 January of +0x40). The round record's dates are years from it. */
    int dy, dm, dd;
    season_from_abs_day(draw, &dy, &dm, &dd);
    uint32_t rec = round_record(dy, 2 * w, draw, leg1, leg2), cup = 0;
    if (rt_unwinding) return 0;
    uint32_t na[1] = {0xb2};
    if (rec) cup = call_cdecl(GAME_NEW, 1, na);
    if (rt_unwinding) return 0;
    if (readable(cup)) {
        /* As season_wcq26.c build_round and season_afc_first.c: (parent, teams, list, rounds, record, template,
         * year, stage, 4, code, 0x14, 1, 0, 0, 0). */
        uint32_t ca[15] = {self, (uint32_t)(2 * w), teams, 1, rd32(g_mem, self + F_RECORD), rec, (uint32_t)dy, 0, 4,
                           ROUND_CODE, 0x14, 1, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return 0;
    } else {
        cup = 0;
    }
    if (!cup) {
        game_free(teams);
        game_free(rec);
        return 0;
    }
    wr32(g_mem, rd32(g_mem, self + F_STAGES), cup);
    wr32(g_mem, self + F_STAGE, 0);
    test_stage = cup;
    test_first_day = 0;
    uint32_t stats = rd32(g_mem, self + F_STATS), nt = rd32(g_mem, NATIONAL_TEAMS);
    for (int i = 0; i < 2 * w; i++) {
        uint32_t team = rd32(g_mem, teams + 4u * (uint32_t)i);
        if (readable(stats)) {
            uint32_t sa[2] = {team, 0x1f};
            call_this(STATS_SET_POSITION, stats, 2, sa);
            if (rt_unwinding) return 0;
        }
        if (readable(nt)) {
            uint32_t ta[2] = {rd32(g_mem, team + 0x53u), rd32(g_mem, self + F_RECORD)};
            call_this(SET_CURRENT_COMP, nt, 2, ta);
            if (rt_unwinding) return 0;
        }
    }
    if (logging()) {
        int y1, m1, d1, y2, m2, d2;
        season_from_abs_day(leg1, &y1, &m1, &d1);
        season_from_abs_day(leg2, &y2, &m2, &d2);
        fprintf(stderr, "recomp: season intl: wc qualifying %d: OFC preliminary round of %d ties (pool %d, %d "
                        "places), %04d-%02d-%02d and %04d-%02d-%02d:", c + 2, w, p, places, y1, m1, d1, y2, m2, d2);
        for (int k = 0; k < w; k++)
            fprintf(stderr, " %s v %s;", team_name(rd32(g_mem, teams + 8u * (uint32_t)k)),
                    team_name(rd32(g_mem, teams + 8u * (uint32_t)k + 4u)));
        fputc('\n', stderr);
    }
    game_free(teams);
    game_free(rec);
    uint32_t z[1] = {0};
    call_this(CUP_START, cup, 1, z);
    if (rt_unwinding) return 0;
    return 1;
}

/* Item 245, RECOMP_TEST_OFC_LEVEL=1 (tests only): every fixture of the preliminary round's stage is processed as a
 * level tie, as the play-off test of season_playoffs.c. Every match as 0-0, so after the second leg the tie is level
 * on aggregate and on away goals (the round's +0x0f 3 keeps the away goals rule, 0x51f696). The first leg is the
 * first match day of the stage. When a later match's flags ask for extra time and
 * penalties (bits 0x3: 0x51f696, 0x51c1c9) it gets what the engine would give, 0-0 in extra time and penalties
 * 5-4. The result block is fixture +0x43..: goals +0, +1, extra time +4, +5, penalties +6, +7 (as the play-off
 * test). Each match prints its flags (fixture +0x38) and its round (+0x34). */
void season_ofc_first_test_fixture(uint32_t cup, uint32_t fx)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_TEST_OFC_LEVEL") && *getenv("RECOMP_TEST_OFC_LEVEL") == '1';
    if (!on || !test_stage || cup != test_stage || !readable(fx)) return;
    long day = season_abs_day((int16_t)rd16(g_mem, GAME_DATE + 2), 1, 1) + (int16_t)rd16(g_mem, GAME_DATE);
    if (!test_first_day) test_first_day = day;
    uint32_t b = fx + 0x43u;
    uint16_t flags = rd16(g_mem, fx + 0x38u);
    int first = day == test_first_day;
    for (int i = 0; i < 10; i++) g_mem[b + (uint32_t)i] = 0;
    if (!first && (flags & 3u) == 3u) {
        g_mem[b + 6] = 5;
        g_mem[b + 7] = 4;
    }
    fprintf(stderr, "recomp: season intl: test: OFC preliminary match on day %ld, round %d, forced %s, match flags "
                    "%04x%s\n", day - season_abs_day((int16_t)rd16(g_mem, GAME_DATE + 2), 1, 1),
            (int16_t)rd16(g_mem, fx + 0x34u), first ? "0-0" : "0-0 level on aggregate", flags,
            first ? "" : (flags & 3u) == 3u ? ", extra time 0-0 and penalties 5-4 given"
                                             : ", no extra time or penalties asked for");
}

/* The winner of tie k of the stage (rows 2k and 2k + 1, status +5 = 1), as season_wcq26.c tie_winner. */
static uint32_t tie_winner(uint32_t cup, int k)
{
    uint32_t rows = rd32(g_mem, cup + F_CUP_ROWS);
    for (int i = 2 * k; readable(rows) && i < 2 * k + 2; i++)
        if (rd8(g_mem, rows + (uint32_t)i * 6u + 5u) == 1u) return rd32(g_mem, rows + (uint32_t)i * 6u);
    return 0;
}

/* The group draw day: the winners into the list and group A, the stage freed, slot 0 cleared, +0x30 = -1. */
static void finish(uint32_t self)
{
    uint32_t cup = slot0(self), rows = rd32(g_mem, cup + F_CUP_ROWS), list = rd32(g_mem, self + F_LIST);
    int n = (int16_t)rd16(g_mem, cup + F_TEAMS), group_a = (int16_t)rd16(g_mem, self + F_TEAMS);
    for (int k = 0; readable(rows) && readable(list) && k < n / 2; k++) {
        uint32_t a = rd32(g_mem, rows + 12u * (uint32_t)k), b = rd32(g_mem, rows + 12u * (uint32_t)k + 6u);
        uint32_t win = tie_winner(cup, k);
        int at = in_list(self, a);
        if (at < 0) at = in_list(self, b);
        if (at < 0) continue;
        if (!win) {
            fprintf(stderr, "recomp: season intl: wc qualifying %d: OFC preliminary tie %s v %s not decided on the "
                            "group draw day, %s goes on\n", year_of(self) + 2, team_name(a), team_name(b),
                    team_name(rd32(g_mem, list + 4u * (uint32_t)at)));
            continue;
        }
        if (logging())
            fprintf(stderr, "recomp: season intl: wc qualifying %d: OFC preliminary round: %s goes to the groups\n",
                    year_of(self) + 2, team_name(win));
        if (rd32(g_mem, list + 4u * (uint32_t)at) == win) continue;
        wr32(g_mem, list + 4u * (uint32_t)at, win);
        if (at % 2 == 0 && at / 2 < group_a) {
            /* init_table_row mallocs the row's array +9 again (0x687570..0x687582): the old one is freed. */
            uint32_t table = rd32(g_mem, self + F_TABLE), old = 0;
            if (readable(table)) old = rd32(g_mem, table + (uint32_t)(at / 2) * ROW_SIZE + 9u);
            uint32_t ra[4] = {(uint32_t)(at / 2), win, 0, 0};
            call_this(INIT_TABLE_ROW, self, 4, ra);
            if (rt_unwinding) return;
            if (readable(old) && readable(table) && rd32(g_mem, table + (uint32_t)(at / 2) * ROW_SIZE + 9u) != old)
                game_free(old);
            if (rt_unwinding) return;
        }
    }
    uint32_t one[1] = {1};
    call_this(rd32(g_mem, rd32(g_mem, cup)), cup, 1, one);         /* scalar deleting dtor, as 0x91ef99 */
    if (rt_unwinding) return;
    wr32(g_mem, rd32(g_mem, self + F_STAGES), 0);
    wr32(g_mem, self + F_STAGE, 0xffffffffu);
}

static int draw_day(uint32_t self) { return today_abs() == group_draw_abs(self); }

/* Item 245: 0 until the day after the section's own season start of the window's year, when that start comes
 * before the group draw (the day of its league table entry, season_change_start_day: entry 0, 24 July in 2023).
 * The start day itself waits too: the competitions' daily update (0x5ce060, where the update_for_date wrap below
 * runs) comes before the season pass 0x5d5ae0 on the same day. That start_new_season
 * 0x91ef40 frees slot 0 (0x91ef99) and sets +0x30 to -1, but the fixtures of a round built before it stay in the
 * fixture store. The window then built a second round, and the store played both rounds' legs into the one
 * stage left: two first legs, and a leg without the second-leg flags (+0x0f, OR'd in by apply_round_rules 0x521d60
 * from 0x51a6b3 after the first leg) that ended level, so Cup::process_fixture 0x51a150 asked for a third match
 * (box 0x5aa at 0x51ab0f). The AFC round waits for 1 August for the same reason (season_afc_first.c). */
static int season_started(uint32_t self, long now)
{
    int day = season_change_start_day(self), y, m, d;
    if (day < 0) return 1;
    season_from_abs_day(now, &y, &m, &d);
    long start = season_abs_day(y, 1, 1) + day;
    return !(now <= start && start < group_draw_abs(self));
}

void o_0091f3d0(void);

/* update_for_date 0x91f3d0 (vtable +0x14), thiscall (Date *today, int full), ret 8. */
RT_WRAP(0091f3d0, ofc_first_update)
{
    uint32_t self = cpu.ecx, today = rt_arg(0), full = rt_arg(1);
    if (on(self) && rd32(g_mem, self + F_DRAWN) == 0) {
        update_date = today;
        long now = today_abs(), gd = group_draw_abs(self);
        /* Each day of the window while slot 0 holds no ties: a day with no FIFA window left or no nation outside
         * the list builds nothing (season_ofc_first_days, the pool), and the next day asks again. */
        if ((int32_t)rd32(g_mem, self + F_STAGE) == -1 && now >= gd - OFC_FIRST_LEAD - 30 && now < gd &&
            season_started(self, now)) {
            build(self);
            if (rt_unwinding) {
                update_date = 0;
                return;
            }
        }
        if (ties_live(self)) {
            test_stage = slot0(self);
            if (full && draw_day(self)) {
                finish(self);
            } else {
                uint32_t cup = slot0(self), a[2] = {today, full};
                call_this(rd32(g_mem, rd32(g_mem, cup) + 0x14u), cup, 2, a);
            }
            if (rt_unwinding) {
                update_date = 0;
                return;
            }
            cpu.ecx = self;
        }
        update_date = 0;
    }
    o_0091f3d0();
}

