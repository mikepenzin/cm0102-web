/* The Ekstraklasa in its real format of each season (GitHub issue 92, 0.6.2 open item 221).
 *
 * The original's PolFirst plays the 2001/02 format in every season: two groups of 8, then a championship group and
 * a relegation group of 8. The rules table (pol_first, docs/research/rules-0.6.0/gre-pol-cro.tsv) gives one table
 * from 2002/03 (16 clubs, 14 in 2003/04 and 2004/05, 18 from 2021/22) and from 2013/14 to 2019/20 a split after 30
 * rounds: the top 8 and the bottom 8 play 7 more matches, with the points halved (rounded up) to 2016/17 and kept
 * in full from 2017/18 (en.wikipedia.org, Ekstraklasa: "The 2016-17 season was the last when teams started an
 * extra round with half the points"). With changing rules on, this module plays that format. The 2001/02 row, the
 * base game and changing rules off keep the original.
 *
 * The class (vtable 0x96e360):
 * - the constructor 0x7c8a80 allocates 2 stage slots at +0xc (0x7c8b06..0x7c8b1f, +0x2c = 2, +0x30 = -1), calls
 *   set_up_rules 0x7c9690 (which asks vtable +0x3c, create_schedule 0x7c8df0, for stage -1 at 0x7c9727),
 *   create_table 0x7c9450, create_fixtures 0x6835c0, the sort 0x6827d0 and create_second_group_stage 0x7c9900,
 *   then sorts the 16 club slots at +0xee (qsort count 16 at 0x7c8ce5) and sets 16 predicted positions (loop to
 *   16, cmp di, 0x10 at 0x7c8d09). start_new_season 0x7c9740 runs the same steps after the promotion chain;
 * - create_table puts 8 clubs in the object's own table (group A: +0x3e = 8 at 0x7c9469, 0x1d8 bytes at
 *   0x7c945f) and all 16 in the slot list +0xee by their last place (0x7c95a0..0x7c963d);
 * - create_second_group_stage makes group B, a League_Stage of slots 8 to 15, in stage slot 0 and sets +0x30 = 0;
 * - advance_stage 0x7c9ad0 moves +0x30 on and at 1 calls create_final_stage 0x7c9b60: one League_Stage of 16 in
 *   slot 1 (the top 4 of each group, then the bottom 4; club +0x60 the record of the championship group [0x9cf814]
 *   or the relegation group [0x9cf818]; points halved at 0x7c9df5..0x7c9e18; 14 matches, +0xa7 0xe at 0x7c9d54;
 *   places 0, 0, 2, 2 from the stack at 0x7c9b85..0x7c9b92), the order 0x7ca450 at +0xa3 (championship group
 *   first), and the fixtures of two League_Stages of 8 that it deletes again (0x7c9f4e, 0x7c9ffb);
 * - process_promotion_relegation_chain 0x7ca490 (vtable +0xb0) and PolSecond::create_play_off_stage 0x7cc290
 *   (0x7cc33a..0x7cc34f) read stage slot 1 as the final table.
 *
 * One table: create_table takes every club of the division (+0x3e and +0xee as long as the division), the date
 * template is the original's two templates (stage -1, 14 dates from July, and stage 1, 14 dates to June) with
 * extra midweek rounds (or fewer) for 2 (n - 1) rounds, and there is no stage: +0x2c = 0 and +0x30 = -1, as the
 * 3. Liga (season_divisions.c). Stage slot 1 holds the object itself, so the promotion chain and PolSecond read
 * its table. The status functions 0x7ca2a0, 0x7ca320 and 0x7ca3f0 treat stage -1 as the first group stage ("qualify
 * for championship group" news for the leader, 0x9b549c), so in one table they get stage 1, as the final table.
 *
 * The split: the same first 30 rounds with +0x2c = 1. When they end, League::update_for_date 0x6847c0 calls
 * advance_stage (+0x30 from -1 to 0), and this module makes the final stage as create_final_stage does, from the
 * table: the top 8 and the others, 7 matches (each pair once), points halved or kept by the season, the places of
 * the season (+0xc0, +0xc1). It goes in slot 0 and slot 1. */
#include "win.h"
#include "season.h"
#include "season_league.h"
#include "season_names.h"
#include "season_rules.h"
#include "season_pol_first.h"
#include "season_playoffs.h"
#include "season_pol_second.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u             /* the club table (0x245 bytes a club) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u
#define CLUB_GROUP 0x60u            /* club_comp pointer: the final group (0x7c9c5b) */
#define COMPS 0xae23d0u             /* the club_comp table, 0x6b bytes a record */
#define COMP_SIZE 0x6bu
#define KEY_POL_FIRST 0x9cf858u     /* the record id of the Polish First Division (0x7c95ad) */
#define KEY_CHAMPIONS 0x9cf814u     /* the championship group (0x7c9c38) */
#define KEY_RELEGATION 0x9cf818u    /* the relegation group (0x7c9cc7) */
#define POL_FIRST_VTABLE 0x96e360u  /* 0x7c8ac7 */

#define OBJ_RECORD 4u
#define OBJ_STAGES 0xcu
#define OBJ_STAGE_COUNT 0x2cu
#define OBJ_STAGE_NOW 0x30u
#define OBJ_CLUBS 0x3eu
#define OBJ_YEAR 0x40u
#define OBJ_MATCHES 0xa7u
#define OBJ_ORDER_FN 0xa3u          /* the row compare of a stage (0x7c9ead) */
#define OBJ_TABLE 0xb1u
#define OBJ_SLOTS 0xeeu
#define ROW_SIZE 0x3bu
#define DATE_SIZE 0x41u             /* a date of the template: +0 s16 day of the year, +2 s16 year */
#define STAGE_SIZE 0xeeu            /* a League_Stage (0x7c9d23) */

#define F_SCHEDULE 0x007c8df0u      /* PolFirst::create_schedule, thiscall (stage, &count, &code, &flag), ret 0x10 */
#define F_NEW 0x00944cf1u           /* operator new, cdecl (size) */
#define F_MALLOC 0x00944e46u        /* cdecl (size) */
#define F_FREE 0x009452cau          /* cdecl (block) */
#define F_LEAGUE_STAGE 0x0068b100u  /* League_Stage::League_Stage, thiscall, 22 arguments */
#define F_ROW 0x00687430u           /* League::init_table_row, thiscall (index, club, 0, 0), ret 0x10 */
#define F_TV 0x00683010u            /* League::pay_tv_rights, thiscall (amount, 0), ret 8 */
#define F_SORT 0x006827d0u          /* League::sort_table, thiscall (0), ret 4 */
#define F_FIND_ROW 0x0068ab70u      /* League::find_club_row, thiscall (club, 0), ret 8 */
#define F_PRIZE 0x00682f70u         /* League::calc_prize_fund, thiscall (share), ret 4 */
#define POL_ORDER 0x007ca450u       /* the row compare of the final stage: championship group first */

#define MIN_CLUBS 10                /* more than the original's group A of 8, so the two forms never mix */
#define MAX_CLUBS 24
#define SPLIT_TOP 8                 /* the championship round: the top 8 (rules table, 2013/14 to 2019/20) */
#define MAX_DATES 64

enum { ORIGINAL = 0, ONE_TABLE = 1, SPLIT = 2 };

static int readable(uint32_t p) { return rt_readable(p); }

static int logging(void)
{
    static int on = -1;
    if (on < 0) on = getenv("RECOMP_POL_FIRST_LOG") && *getenv("RECOMP_POL_FIRST_LOG");
    return on;
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

static uint32_t game_malloc(uint32_t size)
{
    uint32_t a[1] = {size};
    uint32_t p = call_cdecl(F_MALLOC, 1, a);
    if (rt_unwinding || !readable(p)) return 0;
    memset(g_mem + p, 0, size);
    return p;
}

static void game_free(uint32_t p)
{
    if (!readable(p) || rt_unwinding) return;
    uint32_t a[1] = {p};
    call_cdecl(F_FREE, 1, a);
}

/* The format of a season (its start year): the rules table row of pol_first (after its last row the row stays,
 * Q248). The 2001/02 row ("two groups of 8") and a season with no row are the original's. */
static int format_of(int year)
{
    int s = season_rules_last_season(year, "pol_first");
    const SeasonRule *r = s ? season_rules_find(s, "pol_first", 0) : NULL;
    if (!r || (r->format_text && strstr(r->format_text, "two groups"))) return ORIGINAL;
    for (int i = 0; i < season_league_rule_count; i++) {
        const SeasonLeagueRule *e = &season_league_rules[i];
        if (e->season == s && !strcmp(e->file, "pol_first")) return e->split_after > 0 ? SPLIT : ONE_TABLE;
    }
    return ONE_TABLE;
}

/* 1 when the points are halved at the split of a season (split_points 1 of the league rules). */
static int halved(int year)
{
    int s = season_rules_last_season(year, "pol_first");
    for (int i = 0; i < season_league_rule_count; i++) {
        const SeasonLeagueRule *e = &season_league_rules[i];
        if (e->season == s && !strcmp(e->file, "pol_first")) return e->split_points == 1;
    }
    return 0;
}

/* ---- the exchange with the I liga ---- */

/* The exchange of a season with the I liga (pol_second), from the rules table rows of both (docs/research/rules-0.6.0/
 * gre-pol-cro.tsv, each row with its sources): the Ekstraklasa's direct relegation places (down) and its play-off
 * places against the I liga (po, the encoded relegation play-off count of season_league_table.c); the I liga's direct
 * promotion places (up), its relegation places (ps_down), and its play-off: against the Ekstraklasa (vs, as many pairs
 * as the Ekstraklasa's play-off places: 2 in 2002/03, 1 from 2003/04 to 2008/09) or its own (own: 3rd to 6th for one
 * place from 2019/20, the encoded 4 promotion play-off places from the place after the direct ones). 2008/09 keeps
 * its play-off of 1 pair: the matches of Cracovia (14th) and Korona Kielce (3rd), 13 and 17 June 2009, were put off
 * over a protest of LKS Lodz and then cancelled after the punishment of Widzew Lodz (rsssf.org/tablesp/pol09.html,
 * read 12 November 2026; the PZPN decision of 12 June 2009, sportowefakty.wp.pl/pilka-nozna/81500/baraze-odwolane),
 * an event the game has no means to model. The I liga's relegation play-offs against the third tier (2003/04 to 2006/07,
 * 2008/09, 2014/15 to 2017/18) have no opponent in the game, whose third tier is a pool of clubs: those places stay up.
 * Returns 0 when a row is missing or holds no count. */
typedef struct { int down, po, up, vs, own, ps_down; } Exchange;

static const SeasonLeagueRule *league_rule(const char *file, int season)
{
    for (int i = 0; i < season_league_rule_count; i++)
        if (season_league_rules[i].season == season && !strcmp(season_league_rules[i].file, file))
            return &season_league_rules[i];
    return NULL;
}

static int exchange_of(int year, Exchange *x)
{
    int s1 = season_rules_last_season(year, "pol_first"), s2 = season_rules_last_season(year, "pol_second");
    const SeasonRule *rf = s1 ? season_rules_find(s1, "pol_first", 0) : NULL;
    const SeasonRule *rs = s2 ? season_rules_find(s2, "pol_second", 0) : NULL;
    const SeasonLeagueRule *lf = league_rule("pol_first", s1), *ls = league_rule("pol_second", s2);
    if (!rf || !rs || !lf || !ls || rf->down < 0 || rs->up < 0 || rs->down < 0) return 0;
    x->down = rf->down;
    x->po = lf->po_down;
    x->up = rs->up;
    x->ps_down = rs->down;
    x->vs = x->po;
    x->own = x->po == 0 && ls->po_up == 4 && ls->po_up_first == rs->up + 1;
    return x->down + x->po <= 6 && x->up + x->vs <= 6;
}

/* The I liga clubs that the season sends up: direct, and one more after its own play-off (the play-off pairs with
 * the Ekstraklasa swap clubs and change no size). */
static int sent_up(const Exchange *x) { return x->up + x->own; }

static int switched_on(void)
{
    static int off = -1;
    if (off < 0) off = getenv("RECOMP_POL_FIRST") && !strcmp(getenv("RECOMP_POL_FIRST"), "off");
    return !off && season_game_changes() && season_changing_rules();
}

static int is_pol_first(uint32_t obj) { return readable(obj) && rd32(g_mem, obj) == POL_FIRST_VTABLE; }

/* The format of a PolFirst object in the season of its year +0x40. */
static int mode_of(uint32_t obj)
{
    if (!switched_on() || !is_pol_first(obj)) return ORIGINAL;
    return format_of((int16_t)rd16(g_mem, obj + OBJ_YEAR));
}

/* A PolFirst object in this module's form: the original always has 2 stages. */
static int ours(uint32_t obj)
{
    if (!is_pol_first(obj)) return 0;
    int32_t n = (int32_t)rd32(g_mem, obj + OBJ_STAGE_COUNT);
    return n == 0 || n == 1;
}

/* The clubs of the division of the object, in the order of the club table: count, and the list when not NULL. */
static int division_clubs(uint32_t obj, uint32_t *list, int most)
{
    uint32_t rec = rd32(g_mem, obj + OBJ_RECORD), table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT), n = 0;
    if (!readable(rec) || !readable(table)) return 0;
    int32_t id = (int32_t)rd32(g_mem, rec);
    for (int i = 0; i < count; i++) {
        uint32_t club = table + (uint32_t)i * CLUB_SIZE, div = rd32(g_mem, club + CLUB_DIVISION);
        if (!readable(div) || (int32_t)rd32(g_mem, div) != id) continue;
        if (list && n < most) list[n] = club;
        n++;
    }
    return n;
}

static int club_count(uint32_t obj)
{
    int n = division_clubs(obj, NULL, 0);
    return n < MIN_CLUBS ? 0 : n > MAX_CLUBS ? MAX_CLUBS : n;
}

/* The rounds of the first phase: 2 (n - 1) for an even n, a round more for an odd n (a club rests). The split adds
 * the rounds of its top group, each pair once. */
static int first_rounds(int n) { return 2 * (n + (n & 1) - 1); }
/* The matches of a club in the first phase: 2 (n - 1). With an odd n a club rests one of the 2n dates of each meeting. */
static int first_matches(int n) { return 2 * (n - 1); }
static int split_rounds(void) { return SPLIT_TOP - 1; }

/* ---- the date template ---- */

static int inner;                   /* 1 while this module asks the original for a template */

/* One original template (stage -1 or 1): its dates are copied to out from index at; returns the new count. */
static int original_dates(uint32_t self, int stage, uint8_t *out, int at)
{
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 16;
    uint32_t count = cpu.esp, code = cpu.esp + 4, flag = cpu.esp + 8;
    wr32(g_mem, count, 0), wr32(g_mem, code, 0), wr32(g_mem, flag, 0);
    uint32_t a[4] = {(uint32_t)(stage & 0xff), count, code, flag};
    inner = 1;
    uint32_t t = call_this(F_SCHEDULE, self, 4, a);
    inner = 0;
    if (rt_unwinding) return -1;
    int n = (int16_t)rd16(g_mem, count);
    cpu.esp = saved_esp;
    if (!readable(t) || n <= 0 || at + n > MAX_DATES) {
        game_free(t);
        return -1;
    }
    memcpy(out + (size_t)at * DATE_SIZE, g_mem + t, (size_t)n * DATE_SIZE);
    game_free(t);
    return at + n;
}

static int day_of(const uint8_t *d) { return (int16_t)(d[0] | d[1] << 8); }
static int year_of(const uint8_t *d) { return (int16_t)(d[2] | d[3] << 8); }

/* The season's dates in order: the original's 28, with extra rounds on the Wednesday after a date whose next date
 * is at least a week later in the same year (as the 3. Liga, season_divisions.c), spread over the season, or with
 * dates taken out at even points. Returns the count (want), or 0. */
static int season_dates(uint32_t self, uint8_t *out, int want)
{
    static uint8_t base[MAX_DATES * DATE_SIZE];
    int have = original_dates(self, -1, base, 0);
    if (have > 0) have = original_dates(self, 1, base, have);
    if (have <= 0 || want <= 0 || want > MAX_DATES) return 0;
    if (want <= have) {                 /* fewer rounds: drop have - want dates at even points */
        int drop = have - want, k = 0, done = 0;
        for (int i = 0; i < have; i++) {
            if (done < drop && i == (done + 1) * have / (drop + 1)) {
                done++;
                continue;
            }
            memcpy(out + (size_t)k++ * DATE_SIZE, base + (size_t)i * DATE_SIZE, DATE_SIZE);
        }
        return k == want ? want : 0;
    }
    int extra = want - have;
    for (int gap = 7; gap >= 3; gap -= 4) {     /* a week's gap first, then any gap of 3 days or more */
        int k = 0, done = 0;
        for (int i = 0; i < have; i++) {
            const uint8_t *a = base + (size_t)i * DATE_SIZE;
            memcpy(out + (size_t)k++ * DATE_SIZE, a, DATE_SIZE);
            if (done >= extra || i + 1 >= have || i < done * have / (extra + 1)) continue;
            const uint8_t *b = a + DATE_SIZE;
            int day = day_of(a), step = gap == 7 ? 4 : 1;
            if (year_of(a) == year_of(b) && day_of(b) - day >= gap && day + step < 365) {
                uint8_t *c = out + (size_t)k++ * DATE_SIZE;
                memcpy(c, a, DATE_SIZE);
                c[0] = (uint8_t)((day + step) & 0xff), c[1] = (uint8_t)((day + step) >> 8);
                done++;
            }
        }
        if (done == extra) return want;
    }
    return 0;
}

/* PolFirst::create_schedule 0x7c8df0 (thiscall: stage, &count, &code, &flag; ret 0x10). One table and the split:
 * stage -1 gets the first phase's dates, stage 1 (the split) the last 7. */
void o_007c8df0(void);
RT_WRAP(007c8df0, pol_first_schedule)
{
    uint32_t self = cpu.ecx, count_at = rt_arg(1);
    int stage = (int8_t)(rt_arg(0) & 0xff), mode = inner ? ORIGINAL : mode_of(self);
    o_007c8df0();
    if (rt_unwinding || mode == ORIGINAL || (stage != -1 && !(stage == 1 && mode == SPLIT))) return;
    int n = club_count(self);
    if (!n || !readable(count_at) || !readable(cpu.eax)) return;
    int first = first_rounds(n), total = first + (mode == SPLIT ? split_rounds() : 0);
    static uint8_t dates[MAX_DATES * DATE_SIZE];
    uint32_t old = cpu.eax;
    if (season_dates(self, dates, total) != total) {
        fprintf(stderr, "recomp: pol first: no template of %d dates for %d clubs, the original's stays\n", total, n);
        return;
    }
    int from = stage == -1 ? 0 : first, k = stage == -1 ? first : total - first;
    uint32_t t = game_malloc((uint32_t)k * DATE_SIZE);
    if (!t) return;
    memcpy(g_mem + t, dates + (size_t)from * DATE_SIZE, (size_t)k * DATE_SIZE);
    game_free(old);
    wr16(g_mem, count_at, (uint16_t)k);
    cpu.eax = t;
    if (logging())
        fprintf(stderr, "recomp: pol first: %d/%02d stage %d: %d dates for %d clubs (%s)\n",
                (int16_t)rd16(g_mem, self + OBJ_YEAR), ((int16_t)rd16(g_mem, self + OBJ_YEAR) + 1) % 100, stage, k,
                n, mode == SPLIT ? "split" : "one table");
}

/* ---- the table ---- */

/* PolFirst::create_table 0x7c9450 (thiscall, returns 1, ret 0). One table and the split: every club of the
 * division in the table and in the slot list +0xee, in the order of the club table; the TV rights as 0x7c966c. */
void o_007c9450(void);
RT_WRAP(007c9450, pol_first_table)
{
    uint32_t self = cpu.ecx;
    int mode = mode_of(self), n = mode == ORIGINAL ? 0 : club_count(self);
    if (!n) {
        o_007c9450();
        return;
    }
    uint32_t list[MAX_CLUBS];
    division_clubs(self, list, n);
    uint32_t table = game_malloc((uint32_t)n * ROW_SIZE), slots = table ? game_malloc(4u * (uint32_t)n) : 0;
    if (!slots) {
        game_free(table);
        if (!rt_unwinding) rt_ret_stdcall(0, 0);
        return;
    }
    game_free(rd32(g_mem, self + OBJ_SLOTS));
    wr32(g_mem, self + OBJ_TABLE, table);
    wr32(g_mem, self + OBJ_SLOTS, slots);
    wr16(g_mem, self + OBJ_CLUBS, (uint16_t)n);
    wr16(g_mem, self + OBJ_MATCHES, 0xffffu);     /* check_complete counts (n - 1) x meetings again */
    /* The places of the season's exchange (the original's final table has 0, 0, 2, 2 at 0x7c9b85..0x7c9b92):
     * set_up_rules gives the object 4 up for its group (+0xbe at 0x7c96d9) and 0 for the others (0x7c96e1..0x7c96ed),
     * and the season code sets none for pol_first (the I liga has a floor, season_sizes.c). The play-off rows (status
     * 2) go to PolSecond::create_play_off_stage 0x7cc290 (0x7cc34f). Without a row the original's 2 and 2 stay. */
    Exchange x;
    int year = (int16_t)rd16(g_mem, self + OBJ_YEAR), real = exchange_of(year, &x);
    g_mem[self + 0xbe] = 0, g_mem[self + 0xbf] = 0;
    g_mem[self + 0xc0] = (uint8_t)(real ? x.po : 2), g_mem[self + 0xc1] = (uint8_t)(real ? x.down : 2);
    if (logging() && real)
        fprintf(stderr, "recomp: pol first: %d/%02d: %d down, %d to the play-off; I liga %d up, %s, %d down\n", year,
                (year + 1) % 100, x.down, x.po, x.up,
                x.vs ? (x.vs == 1 ? "1 play-off pair with the Ekstraklasa" : "2 play-off pairs with the Ekstraklasa")
                     : x.own ? "its own play-off of 3rd to 6th for one place" : "no play-off", x.ps_down);
    for (int i = 0; i < n; i++) {
        wr32(g_mem, slots + 4u * (uint32_t)i, list[i]);
        uint32_t a[4] = {(uint32_t)i, list[i], 0, 0};
        call_this(F_ROW, self, 4, a);
        if (rt_unwinding) return;
    }
    uint32_t tv[2] = {0x7a120u, 0};
    call_this(F_TV, self, 2, tv);
    if (rt_unwinding) return;
    if (logging())
        fprintf(stderr, "recomp: pol first: %d/%02d: one table of %d clubs%s\n", (int16_t)rd16(g_mem, self + OBJ_YEAR),
                ((int16_t)rd16(g_mem, self + OBJ_YEAR) + 1) % 100, n, mode == SPLIT ? ", split after 30 rounds" : "");
    rt_ret_stdcall(1, 0);
}

/* The stage slots of this module's form: a new array of 2, slot 0 and slot 1 (the final table that the promotion
 * chain and PolSecond read: the stage, or the object itself before it). */
static void set_slots(uint32_t self, uint32_t stage)
{
    uint32_t slots = game_malloc(8);
    if (!slots) return;
    uint32_t old = rd32(g_mem, self + OBJ_STAGES);
    wr32(g_mem, slots, stage);
    wr32(g_mem, slots + 4, stage ? stage : self);
    wr32(g_mem, self + OBJ_STAGES, slots);
    game_free(old);
}

/* PolFirst::create_second_group_stage 0x7c9900 (thiscall, ret 0). No group B: no stage in one table, one stage to
 * come in the split. */
void o_007c9900(void);
RT_WRAP(007c9900, pol_first_group_b)
{
    uint32_t self = cpu.ecx;
    int mode = mode_of(self);
    if (mode == ORIGINAL || (int16_t)rd16(g_mem, self + OBJ_CLUBS) <= SPLIT_TOP) {
        o_007c9900();
        return;
    }
    set_slots(self, 0);
    if (rt_unwinding) return;
    wr32(g_mem, self + OBJ_STAGE_COUNT, mode == SPLIT ? 1u : 0u);
    wr32(g_mem, self + OBJ_STAGE_NOW, 0xffffffffu);
    rt_ret_stdcall(0, 0);
}

/* ---- the split ---- */

/* One League_Stage of the clubs of list, with the arguments of create_final_stage (0x7c9d41..0x7c9d90 and
 * 0x7c9f00..0x7c9f4e): parent, clubs, list, meetings, record, round table, match days, +0xc2, +0xc3, +0xc4, order
 * +0xc5, places, year, stage index, 0, 0x14, 1, 0, 3, the matches of a club, then the prize fund and 1 (the 16) or
 * 0 and 2 (the groups). The stage index goes to the fixtures (+0x42), and League code finds a fixture's stage by it
 * in the slots 0 to +0x30 (0x685ddb..0x685dfa, box "league.cpp 3214" at 0x685e5e when it is above +0x30). The original
 * passes 1 for its final stage in slot 1. In this module's form the split stage is in slot 0 with +0x30 0, so its index
 * is 0 (an index of 1 gave the box in a 2013/14 game, day 101 of 2014). */
static uint32_t make_stage(uint32_t self, int n, uint32_t list, int meetings, uint32_t dates, int days,
                           uint32_t places, uint32_t matches, uint32_t a20, uint32_t a21)
{
    uint32_t sz[1] = {STAGE_SIZE};
    uint32_t obj = call_cdecl(F_NEW, 1, sz);
    if (rt_unwinding || !readable(obj)) return 0;
    uint32_t a[22] = {self, (uint32_t)n, list, (uint32_t)meetings, rd32(g_mem, self + OBJ_RECORD), dates,
                      (uint32_t)days, g_mem[self + 0xc2], g_mem[self + 0xc3], g_mem[self + 0xc4], self + 0xc5, places,
                      (uint32_t)(int16_t)rd16(g_mem, self + OBJ_YEAR), 0, 0, 0x14, 1, 0, 3, matches, a20, a21};
    call_this(F_LEAGUE_STAGE, obj, 22, a);
    return rt_unwinding ? 0 : obj;
}

/* The final stage of the split (advance_stage to stage 0): the top 8 of the table in the championship round, the
 * others in the relegation round, each pair once; the statistics of the 30 rounds with the points halved (rounded
 * up, as 0x7c9df5..0x7c9e18) or kept. */
static void make_split(uint32_t self)
{
    int n = (int16_t)rd16(g_mem, self + OBJ_CLUBS), year = (int16_t)rd16(g_mem, self + OBJ_YEAR);
    uint32_t table = rd32(g_mem, self + OBJ_TABLE), comps = rd32(g_mem, COMPS);
    if (n <= SPLIT_TOP || n > MAX_CLUBS || !readable(table) || !readable(comps)) return;
    uint32_t zero[1] = {0};
    call_this(F_SORT, self, 1, zero);
    uint32_t list = game_malloc(4u * (uint32_t)n);
    if (!list) return;
    uint32_t champions = comps + rd32(g_mem, KEY_CHAMPIONS) * COMP_SIZE;
    uint32_t relegation = comps + rd32(g_mem, KEY_RELEGATION) * COMP_SIZE;
    for (int i = 0; i < n; i++) {
        uint32_t club = rd32(g_mem, table + (uint32_t)i * ROW_SIZE);
        wr32(g_mem, list + 4u * (uint32_t)i, club);
        if (readable(club)) wr32(g_mem, club + CLUB_GROUP, i < SPLIT_TOP ? champions : relegation);
    }
    /* The places of the final table: the object's (0, 0, 2, 2, set in create_table as the original passes them from
     * the stack). The bytes sit on the game stack, as the original's. */
    uint32_t saved_esp = cpu.esp;
    cpu.esp -= 8;
    uint32_t places = cpu.esp;
    g_mem[places] = 0, g_mem[places + 1] = 0, g_mem[places + 2] = g_mem[self + 0xc0], g_mem[places + 3] = g_mem[self + 0xc1];
    uint32_t share[1] = {0x11170u};
    uint32_t prize = call_this(F_PRIZE, self, 1, share);
    uint32_t stage = rt_unwinding ? 0 : make_stage(self, n, list, 2, 0, 0, places, (uint32_t)split_rounds(), prize, 1);
    cpu.esp = saved_esp;
    if (!stage) {
        game_free(list);
        return;
    }
    set_slots(self, stage);
    int half = halved(year);
    uint32_t rows = rd32(g_mem, stage + OBJ_TABLE);
    for (int i = 0; readable(rows) && i < n; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        uint32_t a[2] = {rd32(g_mem, row), 0};
        uint32_t from = call_this(F_FIND_ROW, self, 2, a);
        if (rt_unwinding) return;
        if (!readable(from)) continue;
        /* The three sections of 0xe bytes (+0x12, +0x20, +0x2e): wins, draws, losses (bytes), goals for and against
         * (words) as 0x7c9e1c..0x7c9e7e, and the points (+7) as 0x7c9df5..0x7c9e18, halved or kept. */
        for (uint32_t s = 0x12; s <= 0x2e; s += 0xe) {
            for (uint32_t b = 0; b < 3; b++) g_mem[row + s + b] = g_mem[from + s + b];
            wr16(g_mem, row + s + 3, rd16(g_mem, from + s + 3));
            wr16(g_mem, row + s + 5, rd16(g_mem, from + s + 5));
            wr16(g_mem, row + s + 7, rd16(g_mem, from + s + 7));
        }
        if (half) {
            int all = ((int16_t)rd16(g_mem, from + 0x19) + 1) / 2, second = ((int16_t)rd16(g_mem, from + 0x27) + 1) / 2;
            wr16(g_mem, row + 0x19, (uint16_t)all);
            wr16(g_mem, row + 0x27, (uint16_t)second);
            wr16(g_mem, row + 0x35, (uint16_t)(all - second));
        }
    }
    wr32(g_mem, stage + OBJ_ORDER_FN, POL_ORDER);
    call_this(F_SORT, stage, 1, zero);
    if (rt_unwinding) return;
    /* The fixtures: a League_Stage of each round with the season's last 7 dates, each pair once; the original
     * deletes its two at once (vtable +0, flag 1, 0x7c9f62..0x7c9f68), and so does this. */
    uint32_t count_at = 0;
    cpu.esp -= 16;
    count_at = cpu.esp;
    wr32(g_mem, count_at, 0), wr32(g_mem, count_at + 4, 0), wr32(g_mem, count_at + 8, 0);
    uint32_t sa[4] = {1, count_at, count_at + 4, count_at + 8};
    uint32_t dates = call_this(F_SCHEDULE, self, 4, sa);
    int days = rt_unwinding ? 0 : (int16_t)rd16(g_mem, count_at);
    cpu.esp = saved_esp;
    if (!readable(dates) || days <= 0) return;
    for (int g = 0; g < 2; g++) {
        int k = g ? n - SPLIT_TOP : SPLIT_TOP;
        uint32_t part = make_stage(self, k, list + (g ? 4u * SPLIT_TOP : 0), 1, dates, days, self + 0xbe,
                                   0xffffffffu, 0, 2);
        if (!part) break;
        uint32_t del[1] = {1};
        call_this(rd32(g_mem, rd32(g_mem, part)), part, 1, del);
        if (rt_unwinding) return;
    }
    game_free(dates);
    game_free(list);
    if (logging())
        fprintf(stderr, "recomp: pol first: %d/%02d: split after 30 rounds, %d and %d clubs, %d dates, points %s\n",
                year, (year + 1) % 100, SPLIT_TOP, n - SPLIT_TOP, days, half ? "halved" : "kept");
}

/* PolFirst::advance_stage 0x7c9ad0 (thiscall, ret 0). In the split +0x30 goes from -1 to 0 and the final stage is
 * made. */
void o_007c9ad0(void);
RT_WRAP(007c9ad0, pol_first_advance)
{
    uint32_t self = cpu.ecx;
    int split = ours(self) && rd32(g_mem, self + OBJ_STAGE_COUNT) == 1;
    o_007c9ad0();
    if (rt_unwinding || !split || (int32_t)rd32(g_mem, self + OBJ_STAGE_NOW) != 0) return;
    make_split(self);
}

/* ---- loads and the status functions ---- */

/* PolFirst::PolFirst 0x7c8a80 (thiscall: year, record; ret 8). After a load League::load 0x687b10 makes a stage
 * array of +0x2c slots (0x688e44..0x688e5b) and loads the stages to +0x30: here the array of 2 again, with the
 * final stage's order (as 0x7c8aee..0x7c8af4 does for slot 1 of the original). */
void o_007c8a80(void);
RT_WRAP(007c8a80, pol_first_ctor)
{
    o_007c8a80();
    uint32_t self = cpu.eax;
    if (rt_unwinding || !ours(self)) return;
    uint32_t old = rd32(g_mem, self + OBJ_STAGES), stage = 0;
    if ((int32_t)rd32(g_mem, self + OBJ_STAGE_COUNT) == 1 && (int32_t)rd32(g_mem, self + OBJ_STAGE_NOW) == 0 &&
        readable(old) && readable(rd32(g_mem, old))) {
        stage = rd32(g_mem, old);
        wr32(g_mem, stage + OBJ_ORDER_FN, POL_ORDER);
    }
    uint32_t keep = cpu.eax;
    set_slots(self, stage);
    if (!rt_unwinding) cpu.eax = keep;
}

/* The status functions with a stage argument treat a stage below 1 as the original's groups: stage -1 of one table
 * is the final table (stage 1), and so is stage 0 of the split (its stage index, make_stage). */
static void stage_one(int index)
{
    if (!ours(cpu.ecx)) return;
    int count = (int)rd32(g_mem, cpu.ecx + OBJ_STAGE_COUNT), stage = (int8_t)(rt_arg(index) & 0xff);
    if ((count == 0 && stage == -1) || (count == 1 && stage == 0))
        g_mem[cpu.esp + 4 + 4 * (uint32_t)index] = 1;
}

void o_007ca2a0(void);
RT_WRAP(007ca2a0, pol_first_staff_status)     /* (club, status, stage, ...), ret 0x18 */
{
    stage_one(2);
    o_007ca2a0();
}

void o_007ca320(void);
RT_WRAP(007ca320, pol_first_fate_news)        /* (club index, fate, stage, ...), ret 0x28 */
{
    stage_one(2);
    o_007ca320();
}

void o_007ca3f0(void);
RT_WRAP(007ca3f0, pol_first_progress)         /* (club, stage, code 1, code 2, code 3), ret 0x14 */
{
    stage_one(1);
    o_007ca3f0();
}

/* The constructor's predicted positions (qsort count at 0x7c8ce5, loop at 0x7c8d09) and set_predicted_positions
 * 0x7ca1e0 (0x7ca264, 0x7ca288) take 16 slots: in this module's form the clubs of the table (tools/recomp/pol_first.py). */
uint32_t rt_pol_first_slots(uint32_t obj, uint32_t original)
{
    if (!ours(obj)) return original;
    int n = (int16_t)rd16(g_mem, obj + OBJ_CLUBS);
    return n >= MIN_CLUBS && n <= MAX_CLUBS ? (uint32_t)n : original;
}

/* The club count of the Polish First Division at the club moves of a new game in a season (season_clubs.c): the
 * row's count in one table or the split, else -1 (the class's 16). */
int season_pol_first_clubs(int season)
{
    if (!switched_on() || format_of(season) == ORIGINAL) return -1;
    int s = season_rules_last_season(season, "pol_first");
    const SeasonRule *r = s ? season_rules_find(s, "pol_first", 0) : NULL;
    return r && r->clubs >= MIN_CLUBS && r->clubs <= MAX_CLUBS ? r->clubs : -1;
}

/* ---- the I liga side of the exchange ---- */

#define POL_SECOND_VTABLE 0x96e4b4u /* PolSecond (season_pol_playoff.c) */
#define F_ENG_RESULT 0x00576c50u    /* EngFirst +0x44: the final's winner gets status 0 (0x576d9e) */

/* The exchange of the season of an I liga object, 0 when its Ekstraklasa season is the original's. */
static int second_exchange(uint32_t obj, Exchange *x)
{
    if (!switched_on() || !readable(obj) || rd32(g_mem, obj) != POL_SECOND_VTABLE) return 0;
    int year = (int16_t)rd16(g_mem, obj + OBJ_YEAR);
    return format_of(year) != ORIGINAL && exchange_of(year, x);
}

/* PolSecond::create_table 0x7cbdf0 (thiscall, returns 1, ret 0), after set_up_rules 0x7cbf40 (2 up, 2 to the
 * play-off, 4 down at 0x7cbf92..0x7cbfaf; 6 in 2001): the season's places. The play-off stage (+0x2c 1 from the
 * constructor, 0x7cb23d) stays when a play-off is played, else no stage, as the 3. Liga (season_divisions.c). */
void o_007cbdf0(void);
RT_WRAP(007cbdf0, pol_second_table)
{
    uint32_t self = cpu.ecx;
    o_007cbdf0();
    Exchange x;
    if (rt_unwinding || !second_exchange(self, &x)) return;
    uint32_t keep = cpu.eax;
    g_mem[self + 0xbe] = (uint8_t)x.up;
    g_mem[self + 0xbf] = (uint8_t)(x.vs ? x.vs : x.own ? 4 : 0);
    g_mem[self + 0xc0] = 0;
    g_mem[self + 0xc1] = (uint8_t)x.ps_down;
    int stages = x.vs || x.own;
    if (stages && !readable(rd32(g_mem, self + OBJ_STAGES))) {
        uint32_t slot = game_malloc(4);
        if (!slot) return;
        wr32(g_mem, slot, 0);
        wr32(g_mem, self + OBJ_STAGES, slot);
    }
    wr32(g_mem, self + OBJ_STAGE_COUNT, (uint32_t)stages);
    wr32(g_mem, self + OBJ_STAGE_NOW, 0xffffffffu);
    cpu.eax = keep;
}

/* PolSecond::create_play_off_stage 0x7cc290 (thiscall, ret 0). Its own play-off (3rd to 6th for one place) is the
 * play-off of slice 19 (season_playoffs.c: two-leg semi-finals 6th v 3rd and 5th v 4th, then the final). The play-off
 * against the Ekstraklasa is the original's, with 2 clubs when 1 pair plays (tools/recomp/pol_first.py). */
void o_007cc290(void);
RT_WRAP(007cc290, pol_second_playoff)
{
    uint32_t self = cpu.ecx;
    /* The places of barred reserve sides go on first (item 243, season_pol_second.c). */
    season_pol_second_before_playoff(self);
    if (rt_unwinding) return;
    Exchange x;
    if (second_exchange(self, &x) && x.own) {
        /* PolSecond::advance_stage 0x7cc1c0 sets +0x30 to 0 (0x7cc276..0x7cc281) before it calls this function, so the
         * stage is built whatever +0x30 holds. The slot is cleared first: a play-off that is not built (a direct
         * promotion, no time left) leaves no stage, as the 3. Liga (season_divisions.c): +0x2c 0 and +0x30 -1, or
         * League::update_for_date 0x6847c0 calls the stage of an empty slot each day (0x6848f8, call to 0x1). */
        uint32_t slots = rd32(g_mem, self + OBJ_STAGES);
        if (readable(slots)) wr32(g_mem, slots, 0);
        season_playoffs_build_for(self);
        if (rt_unwinding) return;
        if (!readable(slots) || !readable(rd32(g_mem, slots))) {
            wr32(g_mem, self + OBJ_STAGE_COUNT, 0);
            wr32(g_mem, self + OBJ_STAGE_NOW, 0xffffffffu);
        }
        rt_ret_stdcall(0, 0);
        return;
    }
    o_007cc290();
}

/* PolSecond +0x44, the result handler 0x7cc490 (club, code, stage, ...; ret 0x18): a result of its own play-off goes
 * to the English handler, as season_playoffs.c does, so only the final's winner goes up (0x7cc490 gives status 0 to
 * each winner of the I liga, 0x7cc5bc). */
void o_007cc490(void);
RT_WRAP(007cc490, pol_second_result)
{
    Exchange x;
    if (g_mem[cpu.esp + 0xc] != 0xff && second_exchange(cpu.ecx, &x) && x.own) {
        rt_jump(F_ENG_RESULT);
        return;
    }
    o_007cc490();
}

/* The clubs of the play-off against the Ekstraklasa (push 4 at 0x7cc3d1, the object in esi): 2 for 1 pair. */
uint32_t rt_pol_playoff_clubs(uint32_t obj, uint32_t original)
{
    Exchange x;
    return second_exchange(obj, &x) && x.vs == 1 ? 2u : original;
}

/* The round table of the play-off against the Ekstraklasa (PolSecond::create_schedule 0x7cb4a0, stage 0, for
 * create_play_off_stage at 0x7cc390): 4 teams (+0x18, 0x7cbdc1), 2 ties (+0x1a, 0x7cbdc5) and 4 new teams (+0x1c,
 * 0x7cbdcb). With 1 pair the stage holds 2 clubs, and Cup::draw_round 0x51c410 moves the draw on each day while a
 * team slot of the round is empty (0x51c47c..0x51c549): the play-off of 2003/04 was never drawn, the I liga never
 * ended and the Polish season change failed from 2004 (a 2002 game, day 230 of 2004). 1 pair: 2 teams, 1 tie. */
void season_pol_first_playoff_round(uint32_t obj, uint32_t t)
{
    Exchange x;
    if (!readable(t) || !second_exchange(obj, &x) || x.vs != 1) return;
    wr16(g_mem, t + 0x18, 2);
    wr16(g_mem, t + 0x1a, 1);
    wr16(g_mem, t + 0x1c, 2);
}

/* The chain 0x7ca490 moves the clubs in pairs (push 1 at 0x7ca4cc, the object in esi): in this module's form each club
 * moves on its own, so the division takes the size of the season (16 to 14 in 2003/04, 18 from 2021/22). */
uint32_t rt_pol_first_pairwise(uint32_t obj, uint32_t original)
{
    return ours(obj) ? 0u : original;
}

static uint32_t pol_object(void);
static uint32_t split_stage(uint32_t obj);

/* The chain's upper league: the object in one table, or in the seasons of the split the split stage (slot 0), whose
 * promotion and relegation places are the season's (the review of 13 November 2026, point 2). */
int season_pol_first_moves_alone(uint32_t upper)
{
    if (ours(upper)) return 1;
    uint32_t obj = readable(upper) ? pol_object() : 0;
    return obj && upper == split_stage(obj);
}

/* 0x7ca030 (start_new_season, the object in ebx) takes 4 clubs of the Polish third tier, a pool (cmp si, 4 at 0x7ca132,
 * push 4 at 0x7ca13c, mov edi, 4 at 0x7ca14a) and sends the I liga's relegation places down. In this module's form it
 * takes as many as keep the I liga at its size: its relegation places, plus the clubs it sent up, less the clubs the
 * Ekstraklasa sent down (the season that ends: +0x40 before start_new_season adds 1 at 0x7c9791). */
uint32_t rt_pol_lower_intake(uint32_t obj, uint32_t original)
{
    Exchange x;
    if (!ours(obj) || !exchange_of((int16_t)rd16(g_mem, obj + OBJ_YEAR), &x)) return original;
    int k = x.ps_down + sent_up(&x) - x.down;
    return k >= 1 && k <= 8 ? (uint32_t)k : original;
}

/* ---- the final table in the club history and on the table screen ---- */

#define COMP_OBJECTS 0x00adadfcu    /* g_comp_object_table (season_pol_playoff.c) */
#define COMP_SLOTS 0x00adadf8u

/* The Ekstraklasa object in this module's form, or 0. */
static uint32_t pol_object(void)
{
    uint32_t table = rd32(g_mem, COMP_OBJECTS), id = rd32(g_mem, KEY_POL_FIRST);
    uint32_t obj = readable(table) && id < rd32(g_mem, COMP_SLOTS) ? rd32(g_mem, table + 4u * id) : 0;
    return ours(obj) ? obj : 0;
}

/* The split stage (slot 0 after 30 rounds) of the object, or 0. */
static uint32_t split_stage(uint32_t obj)
{
    if (!obj || rd32(g_mem, obj + OBJ_STAGE_COUNT) != 1 || (int32_t)rd32(g_mem, obj + OBJ_STAGE_NOW) != 0) return 0;
    uint32_t slots = rd32(g_mem, obj + OBJ_STAGES);
    return readable(slots) ? rd32(g_mem, slots) : 0;
}

/* A value for cmp byte [t+0x44], imm that matches only when t is the final table (or does not). */
static uint32_t index_match(uint32_t t, int match) { return match ? g_mem[t + 0x44] : (g_mem[t + 0x44] + 1u) & 0xffu; }

/* Club_History::record_comp_season 0x442e80 (cmp at 0x442f95): the final table is recorded, the object in one table,
 * the split stage in the split. Without these the history of the clubs had no Ekstraklasa season (the review of
 * 12 November 2026: the object's +0x44 is 0xff, the split stage's 0). */
uint32_t rt_pol_history_index(uint32_t t, uint32_t original)
{
    uint32_t obj = pol_object();
    if (!obj || !readable(t)) return original;
    int one = rd32(g_mem, obj + OBJ_STAGE_COUNT) == 0, match = one ? t == obj : t == split_stage(obj);
    static int logged = -1;
    int year = (int16_t)rd16(g_mem, obj + OBJ_YEAR);
    if (match && logging() && logged != year) {
        logged = year;
        fprintf(stderr, "recomp: pol first: %d/%02d: club history of the final table, %d clubs, %d matches before it\n",
                year, (year + 1) % 100, (int16_t)rd16(g_mem, t + OBJ_CLUBS), one ? 0 : first_matches((int16_t)rd16(g_mem, obj + OBJ_CLUBS)));
    }
    return index_match(t, match);
}

/* The table screen (cmp at 0x49215b): only the split stage has matches before it. */
uint32_t rt_pol_split_index(uint32_t t, uint32_t original)
{
    uint32_t obj = pol_object();
    if (!obj || !readable(t)) return original;
    return index_match(t, t == split_stage(obj));
}

/* The matches of a club before the final table (0x44301e, 0x49216d, and half of them at 0x49217c for the home and
 * away views): none in one table, the 30 rounds (2 (n - 1)) before the split. The split stage's rows count only its
 * own 7 matches, as League::check_complete 0x689ad0 tests them against +0xa7. */
uint32_t rt_pol_played_before(uint32_t t, uint32_t original)
{
    uint32_t obj = pol_object();
    if (!obj || !readable(t)) return original;
    if (t == obj) return 0;
    if (t != split_stage(obj)) return original;
    uint32_t before = (uint32_t)first_matches((int16_t)rd16(g_mem, obj + OBJ_CLUBS));
    return original == 7u ? before / 2u : before;
}

/* The honours of the season (vtable +0x30 0x595160, a jump to slot +0x30 of stage 1 at 0x595168): in one table slot 1
 * is the object itself, so the jump came back to 0x595160 until the stack ran out (a 2025/26 game, day 135 of 2026,
 * from League::update_for_date 0x6847c0 at the season's end). One table takes League::record_season_honours 0x684640,
 * as a league with no stage. */
#define F_HONOURS 0x00684640u
int season_pol_first_honours(uint32_t obj)
{
    if (!ours(obj) || rd32(g_mem, obj + OBJ_STAGE_COUNT) != 0) return 0;
    rt_jump(F_HONOURS);
    return 1;
}

int season_pol_first_own_playoff(uint32_t obj)
{
    Exchange x;
    return second_exchange(obj, &x) && x.own;
}
