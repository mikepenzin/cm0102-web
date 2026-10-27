/* The league phase in the game (season_uefa_league.h), 0.6.0 slice 25. Evidence:
 * docs/research/2026-10-19-uefa-league-phase.md.
 *
 * A league phase is one League_Stage of 36 clubs (League_Stage::League_Stage 0x68b100, thiscall, 22 arguments,
 * ret 0x58 at 0x68b4c1), as the group stages of the original's cups are (draw_phase1_groups 0x5858b6). Its
 * constructor calls League::create_fixtures 0x6835c0 (0x68b3b9) when it has a round table (+0xba). That
 * function has two paths: a round robin (0x683ac9), or a fixed list at +0xea when the year word +2 of its first
 * entry equals the league's year +0x40 (0x68382e..0x68383d). The fixed list is an array of 16-byte entries made
 * with new[] (the count before the first entry, deleted at 0x683aa1..0x683abe with the empty destructor
 * 0x5a53c0): entry d * (clubs / 2) + i is match i of match day d (0x6838ef..0x683906), +8 the home club id and
 * +0xc the away club id (compared with the club id of each table row, +0xb1 rows of 0x3b bytes, 0x68392f..
 * 0x68395a), +2 the year. The date of each match is the date of its match day in the round table (0x6839f8..
 * 0x683a10). The Italian lists of the original are made so (fixed_fixture_set_by_club_names 0x68aff0 writes the
 * ids at +8 and +0xc). So the league phase gives its constructor a round table of 8 (or 6) match days and puts
 * the matches of the draw (season_uefa_league_draw.c) into +0xea: the wrap of 0x6835c0 below writes the list
 * into the one object that is being made, just before the original reads it.
 *
 * The end of the stage. League::check_complete 0x689ad0 wants each table row's played count (row +0x11) to reach
 * +0xa7 (0x689b47..0x689b58). The constructor sets +0xa7 from its twentieth argument (0x68b215, 0x68b228); -1
 * there means (clubs - 1) * meetings (0x689b1d..0x689b36), 35 for 36 clubs. The league phase passes its match
 * count, 8 or 6. */
#include "win.h"
#include "season_records.h"
#include "season.h"
#include "season_uefa.h"
#include "season_uefa_league.h"
#include "season_cups.h"
#include "season_divisions_ita.h"

#include <stdio.h>
#include <string.h>

#define F_NEW 0x00944cf1u               /* operator new, cdecl (size) */
#define F_DELETE 0x00944c94u            /* operator delete, cdecl (block), as 0x683ab6 */
#define F_MALLOC 0x00944e46u
#define F_FREE 0x009452cau
#define F_RANDOM 0x0090cfc0u            /* utils_random, cdecl (range) */
#define F_QSORT 0x00945333u
#define F_SEED_ORDER 0x00586f60u        /* european_cup_compare_clubs_by_seed */
#define F_LEAGUE_STAGE 0x0068b100u
#define F_LEAGUE_ROW 0x006826d0u        /* League::get_table_row, thiscall (index): the row, club at +0 */
#define F_GROUP_NEWS 0x00684230u        /* League::send_group_draw_news, thiscall () */
#define F_GROUP_DATE 0x0068a160u        /* league_round_set_date, cdecl (table, round, day, month, year+, weekday, time, year, 0) */
#define F_GROUP_SLOT 0x0068a1c0u        /* league_round_set_match_slot, cdecl (table, round, slot, weekday, kind, option, value) */

#define FIXED_SIZE 0x10u                /* an entry of the fixed list (0x683903: index << 4) */
#define GROUP_ROUND_SIZE 0x41u          /* a match day of a League round table (0x68a160) */
#define STAGE_SIZE 0xeeu                /* a League_Stage (operator new 0xee at 0x585855) */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

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

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    return rt_unwinding || !readable(p) ? 0 : p;
}

static void game_free(uint32_t p)
{
    if (!readable(p) || rt_unwinding) return;
    uint32_t a[1] = {p};
    call_cdecl(F_FREE, 1, a);
}

/* The game's random number function for the draw: -1 stops the draw while the game unwinds. */
static int game_random(int n, void *ctx)
{
    (void)ctx;
    if (rt_unwinding) return -1;
    uint32_t a[1] = {(uint32_t)n};
    int r = (int)call_cdecl(F_RANDOM, 1, a);
    return rt_unwinding ? -1 : r;
}

int season_uefa_league_sort(uint32_t *clubs, int n)
{
    uint32_t list = game_malloc(4u * (uint32_t)n);
    if (!list) return 0;
    for (int i = 0; i < n; i++) wr32(g_mem, list + 4u * (uint32_t)i, clubs[i]);
    uint32_t q[4] = {list, (uint32_t)n, 4, F_SEED_ORDER};
    call_cdecl(F_QSORT, 4, q);
    if (rt_unwinding) return 0;
    for (int i = 0; i < n; i++) clubs[i] = rd32(g_mem, list + 4u * (uint32_t)i);
    game_free(list);
    return !rt_unwinding;
}

/* ---- The match days: season_uefa_days.c (season_uefa_league_days) ---- */

/* ---- The fixed list of the stage that is being made ---- */

static uint32_t pending_stage, pending_list;

/* A load or a new game (season_change_reset): no list is pending (load sweep). */
void season_uefa_league_reset(void) { pending_stage = pending_list = 0; }

void o_006835c0(void);
void season_fifa_stage_begin(uint32_t league);  /* season_friendly_dates.c */
void season_fifa_stage_end(void);

/* League::create_fixtures, thiscall () returning 1 or 0. Only the league phase stage that is being made gets the
 * list; every other league runs as it is. The one wrap of 0x6835c0: slice 21b's cup that waits for ever makes no
 * fixture (season_cup_breaks_no_fixtures, season_cups.h), and nor does slice 20b's group C until the regroup
 * (season_ita_no_fixtures, season_divisions_ita.h). */
RT_WRAP(006835c0, league_create_fixtures)
{
    if (season_cup_breaks_no_fixtures() || season_ita_no_fixtures(cpu.ecx)) {
        rt_ret_stdcall(1, 0);
        return;
    }
    uint32_t league = cpu.ecx;
    season_records_league_log(league, rd32(g_mem, cpu.esp));      /* test log of slice 15b's fix */
    if (pending_stage && league == pending_stage && pending_list) {
        wr32(g_mem, league + 0xea, pending_list);
        pending_stage = pending_list = 0;
    }
    season_fifa_stage_begin(league);        /* O259: a qualifier's FIFA day stays within its stage */
    o_006835c0();
    season_fifa_stage_end();
}

/* The fixed list of the draw (new[] of the game: the count, then the entries), or 0. */
static uint32_t fixed_list(const SeasonLeagueDraw *d, const uint32_t *clubs, uint32_t year)
{
    int n = d->days * SEASON_LEAGUE_DAY_MATCHES;
    uint32_t a[1] = {4u + FIXED_SIZE * (uint32_t)n};
    uint32_t block = call_cdecl(F_NEW, 1, a);
    if (rt_unwinding || !readable(block)) return 0;
    memset(g_mem + block, 0, 4u + FIXED_SIZE * (uint32_t)n);
    wr32(g_mem, block, (uint32_t)n);
    uint32_t list = block + 4;
    for (int day = 0; day < d->days; day++)
        for (int m = 0; m < SEASON_LEAGUE_DAY_MATCHES; m++) {
            uint32_t e = list + FIXED_SIZE * (uint32_t)(day * SEASON_LEAGUE_DAY_MATCHES + m);
            wr16(g_mem, e + 2, (uint16_t)year);
            wr32(g_mem, e + 8, rd32(g_mem, clubs[d->match[day][m].home]));
            wr32(g_mem, e + 0xc, rd32(g_mem, clubs[d->match[day][m].away]));
        }
    return list;
}

static void free_fixed_list(uint32_t list)
{
    if (!readable(list) || rt_unwinding) return;
    uint32_t a[1] = {list - 4};
    call_cdecl(F_DELETE, 1, a);
}

uint32_t season_uefa_league_table(uint32_t year, int days, const SeasonLeagueDay *dates, int weekday)
{
    uint32_t t = game_malloc((uint32_t)days * GROUP_ROUND_SIZE);
    for (int d = 0; t && d < days && !rt_unwinding; d++) {
        uint32_t a[9] = {t, (uint32_t)d, (uint32_t)dates[d].day, (uint32_t)dates[d].month, (uint32_t)dates[d].year_off,
                         (uint32_t)weekday, 2, year, 0};
        call_cdecl(F_GROUP_DATE, 9, a);
        uint32_t s[7] = {t, (uint32_t)d, 0, 0xffffffffu, 0xffffffffu, 0xffffffffu, 0};
        if (!rt_unwinding) call_cdecl(F_GROUP_SLOT, 7, s);
    }
    return rt_unwinding ? 0 : t;
}

uint32_t season_uefa_league_make(uint32_t parent, uint32_t *clubs, int days, int stage, uint32_t name,
                                 const SeasonLeagueDay *dates, int weekday)
{
    uint32_t year = (uint32_t)(int16_t)rd16(g_mem, parent + 0x40);
    int nation[SEASON_LEAGUE_CLUBS];
    for (int i = 0; i < SEASON_LEAGUE_CLUBS; i++)
        if (!readable(clubs[i])) return 0;
    if (!season_uefa_league_sort(clubs, SEASON_LEAGUE_CLUBS)) return 0;
    for (int i = 0; i < SEASON_LEAGUE_CLUBS; i++) nation[i] = (int)rd32(g_mem, clubs[i] + 0x53);
    static SeasonLeagueDraw d;
    if (!season_league_draw(days, nation, game_random, NULL, &d)) {
        if (!rt_unwinding)
            fprintf(stderr, "recomp: uefa: league phase %u/%02u: no draw found\n", year, (year + 1) % 100);
        return 0;
    }
    uint32_t members = game_malloc(4u * SEASON_LEAGUE_CLUBS);
    uint32_t table = members ? season_uefa_league_table(year, days, dates, weekday) : 0;
    uint32_t list = table ? fixed_list(&d, clubs, year) : 0;
    uint32_t sz[1] = {STAGE_SIZE};
    uint32_t obj = list ? call_cdecl(F_NEW, 1, sz) : 0;
    if (rt_unwinding) return 0;
    if (!readable(obj)) {
        free_fixed_list(list);
        game_free(table);
        game_free(members);
        return 0;
    }
    for (int i = 0; i < SEASON_LEAGUE_CLUBS; i++) wr32(g_mem, members + 4u * (uint32_t)i, clubs[i]);
    /* Two small byte tables on the game stack, as the group draws give them (0x5852b9..0x5852dc): the order of
     * the table and the places that go on (+0xbe: the first 8, who go to the round of 16). The groups pass 4, 1,
     * 2, 0: code 4 starts a head-to-head pass before the sort (0x682850..0x682877), code 1 is goal difference
     * (row +0x15 - +0x17) and 2 goals scored (+0x15) for clubs level on points (0x68a535..0x68a556). The league
     * phase ranks level clubs by goal difference, then goals scored, with no head-to-head (regulations
     * 2024/25, Art. 18), so it passes 1, 2, 0, 0. The later criteria (away goals, wins) are not in the game. */
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t order = cpu.esp, places = cpu.esp + 8;
    g_mem[order] = 1, g_mem[order + 1] = 2, g_mem[order + 2] = 0, g_mem[order + 3] = 0;
    g_mem[places] = 8, g_mem[places + 1] = 0, g_mem[places + 2] = 0, g_mem[places + 3] = 0;
    /* The arguments of the group draws (0x58587f..0x5858b6): parent, clubs, list, meetings, record, round table,
     * match days, 3, 1, 8, order, places, year, stage, name, 0xf, 2, 0, 8; then +0xa7 the matches of a club
     * (-1 in the groups), 0, 2. */
    uint32_t a[22] = {parent, SEASON_LEAGUE_CLUBS, members, 1, rd32(g_mem, parent + 4), table, (uint32_t)days, 3, 1, 8,
                      order, places, year, (uint32_t)stage, name, 0xf, 2, 0, 8, (uint32_t)days, 0, 2};
    pending_stage = obj, pending_list = list;
    call_this(F_LEAGUE_STAGE, obj, 22, a);
    if (rt_unwinding) {                         /* a later League at the same address must not take the list */
        pending_stage = pending_list = 0;
        return 0;
    }
    cpu.esp = saved_esp;
    if (pending_stage) {                        /* create_fixtures did not run: the list is not the stage's */
        pending_stage = pending_list = 0;
        free_fixed_list(list);
    }
    game_free(members);
    game_free(table);
    if (rt_unwinding) return 0;
    fprintf(stderr, "recomp: uefa: league phase %u/%02u: 36 clubs, %d matches each, draw rules %d\n", year,
            (year + 1) % 100, days, d.relax);
    return obj;
}

void season_uefa_league_news(uint32_t stage)
{
    if (readable(stage) && !rt_unwinding) call_this(F_GROUP_NEWS, stage, 0, NULL);
}

uint32_t season_uefa_league_club_at(uint32_t stage, int place)
{
    uint32_t a[1] = {(uint32_t)(place - 1)};
    uint32_t row = call_this(F_LEAGUE_ROW, stage, 1, a);
    return rt_unwinding || !readable(row) ? 0 : rd32(g_mem, row);
}

int season_uefa_league_knockout_list(uint32_t stage, uint32_t list, uint32_t tiers)
{
    uint32_t by_place[SEASON_LEAGUE_CLUBS + 1];
    int level = 0, wrong = 0, pts0 = 0, gd0 = 0, gf0 = 0;
    for (int p = 1; p <= SEASON_LEAGUE_CLUBS; p++) {
        uint32_t a[1] = {(uint32_t)(p - 1)};
        uint32_t row = call_this(F_LEAGUE_ROW, stage, 1, a);
        if (rt_unwinding || !readable(row)) return 0;
        by_place[p] = rd32(g_mem, row);
        if (!readable(by_place[p])) return 0;
        /* The test seam of the order: clubs level on points (+0x19) with the goal difference and goals of the
         * row (+0x15 scored, +0x17 conceded, as 0x68a53f..0x68a55a read them). */
        int pts = (int16_t)rd16(g_mem, row + 0x19), gf = (int8_t)g_mem[row + 0x15], gd = gf - (int8_t)g_mem[row + 0x17];
        if (p > 1 && pts == pts0) {
            level++;
            wrong += gd > gd0 || (gd == gd0 && gf > gf0);
        }
        pts0 = pts, gd0 = gd, gf0 = gf;
    }
    /* The test seam: the matches each table row has played (row +0x11, the count check_complete reads at
     * 0x689b51; rows at +0xb1, 0x3b bytes each), and the places of the knockout clubs. */
    uint32_t rows = rd32(g_mem, stage + 0xb1);
    int lo = 99, hi = -1, clubs = (int16_t)rd16(g_mem, stage + 0x3e);
    for (int i = 0; readable(rows) && i < clubs; i++) {
        int played = (int8_t)g_mem[rows + 0x3bu * (uint32_t)i + 0x11];
        lo = played < lo ? played : lo, hi = played > hi ? played : hi;
    }
    int po[16], r16[8];
    if (!season_league_knockout(game_random, NULL, po, r16)) return 0;
    char ties[16 * 8] = "";
    for (int k = 0; k < 8; k++)
        snprintf(ties + strlen(ties), sizeof ties - strlen(ties), "%s%d-%d", k ? " " : "", po[2 * k], po[2 * k + 1]);
    fprintf(stderr, "recomp: uefa: league phase table: %d clubs, played %d to %d; play-off ties %s\n", clubs, lo, hi,
            ties);
    fprintf(stderr, "recomp: uefa: league phase order: %d level pairs, %d not by goal difference and goals\n", level,
            wrong);
    /* Slots 0 to 15: the 8 play-off ties, the unseeded club first (it plays the first leg at home), tier 1.
     * Slots 16 to 23: places 1 to 8, tier 2 (the seeded draw of the round of 16 pairs them with the winners). */
    for (int k = 0; k < 16; k++) {
        wr32(g_mem, list + 4u * (uint32_t)k, by_place[po[k]]);
        g_mem[tiers + (uint32_t)k] = 1;
    }
    for (int k = 0; k < 8; k++) {
        wr32(g_mem, list + 4u * (uint32_t)(16 + k), by_place[r16[k]]);
        g_mem[tiers + 16u + (uint32_t)k] = 2;
    }
    /* Places 25 to 36 are out, as the group fourths of the original (0x5862fe). */
    for (int p = 25; p <= SEASON_LEAGUE_CLUBS; p++) wr32(g_mem, by_place[p] + 0x1db, 0xffffffffu);
    return 1;
}
