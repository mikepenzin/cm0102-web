/* The Swedish third tier in its real groups (issue 106, 0.7.3 slice 8).
 *
 * Real life (rules table, rows den-swe-190 to den-swe-322): six regional groups of 12 to the season of 2005, two groups
 * of 14 (Norra, Sodra) from 2006 and two groups of 16 from 2018.
 *
 * The original. SweSecond (the extra division of Sweden) always plays six groups of 12: the league object is the North
 * group (create_table 0x8921b0 sets +0x3e = 12 at 0x8921c2) and the five other groups are the stages [+0xc][0..4]
 * (create_section_stage 0x892670, called five times at 0x891942 and 0x8925a4). Stages 5 and 6 are the play-offs
 * (advance_stage 0x8925d0 makes them when the stage reaches 5). +0x2c is the stage count, 7 (0x891764). League save
 * 0x6891c0, load 0x687b10 and the destructor walk the stages 0..+0x30, and League::update_for_date 0x6847c0 closes the
 * season when +0x30 = +0x2c - 1. So an edition with +0x2c = 1 and the one stage 0 saves, loads and ends with no change
 * of the save format, and advance_stage never runs.
 *
 * In an edition whose row says "2 groups of N" (changing rules on, league sizes on, the extra division played):
 * - Norra is the league object and Sodra is stage 0. A club's group is its +0x60 record: the North record [0x9cf5f8]
 *   for Norra, the East Svealand record [0x9cf5fc] for Sodra, because 0x892670 takes that record for stage 0
 *   (0x8926c8) and update_staff_records_for_status 0x892ee0 finds stage 0 by it (0x892f4d).
 * - The fixed loops over six groups are replaced: get_club_array 0x893320 (72 clubs), set_predicted_positions 0x893470
 *   (a block of 12), the promotion chain 0x8907f0 (the six calls of 0x689c80 at 0x890888..0x89096d), the moves down and
 *   up 0x890b40 and the deal into six sections 0x890c70 (it stops under 71 clubs, 0x890d86).
 * - The dates: create_schedule 0x891a60 makes 22 rounds (0x891af8), the template takes the rounds of the group.
 * - The places of each group are half the row's: 0x684cd0 reads them from +0xbe and +0xc1 of the table and the stage.
 * The deal runs in create_table, at a new game and at each season start: the division takes its clubs from the Lower Division or
 * sends its lowest clubs by reputation there, never a human manager's club, and the clubs go to Norra or Sodra by the
 * region scores of the original, swe_prm_region_fit_score 0x891370.
 *
 * RECOMP_SWE_GROUPS=off (test switch) keeps the six groups. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_league.h"
#include "season_rules.h"
#include "season_second_teams.h"
#include "season_sizes.h"
#include "season_swe_groups.h"
#include "season_swe_groups_plan.h"
#include "season_template.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VT_SECOND 0x009702a0u       /* the vtable of SweSecond (0x89173c) */
#define KEY_SWEDEN 0x9cf4a8u
#define KEY_SECOND 0x9cf7c8u
#define KEY_LOWER 0x9cf6acu
#define KEY_PREMIER 0x9cf5ecu
#define KEY_FIRST 0x9cf5f0u
#define KEY_NORRA 0x9cf5f8u         /* the North record: the league object */
#define KEY_SODRA 0x9cf5fcu         /* the East Svealand record: stage 0 (0x8926c8) */
static const uint32_t NORTH_KEYS[3] = {0x9cf5f8u, 0x9cf5fcu, 0x9cf604u};     /* North, East and West Svealand */
static const uint32_t SOUTH_KEYS[3] = {0x9cf600u, 0x9cf608u, 0x9cf5f4u};     /* East, West and South Gotaland */

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define CLUB_ZONE 0x60u
#define COMP_RECORD 4u
#define COMP_STATS 8u
#define COMP_STAGES 0xcu
#define COMP_STAGE_COUNT 0x2cu
#define COMP_STAGE_NOW 0x30u
#define COMP_CLUBS 0x3eu
#define COMP_YEAR 0x40u
#define COMP_TABLE 0xb1u
#define UP 0xbeu
#define UP_PLAYOFF 0xbfu
#define DOWN_PLAYOFF 0xc0u
#define DOWN 0xc1u
#define VT_FINISH 0xa4u             /* vtable +0xa4: 0x68aa30, called on each division before the chain (0x890803) */

#define F_ADD_ROW 0x00687430u       /* League: fills table row, thiscall (row, club, 0, 0), as called at 0x89229b */
#define F_CHAIN 0x00689c80u         /* league_promote_and_relegate, thiscall (upper, lower, 1, 1, -1, -1) */
#define F_UP 0x006830b0u            /* thiscall (club, record, 1): a club goes up (0x890c51) */
#define F_DOWN 0x006831a0u          /* thiscall (club, record, 1): a club goes down (0x890bfb) */
#define F_REGION 0x00891370u        /* stdcall (club, section id): 0 to 100 */
#define F_PREDICTED 0x004a2540u     /* Comp_Stats, thiscall (club, value), as 0x8935a5 */
#define F_QSORT 0x00945333u         /* cdecl (base, n, size, compare) */
#define F_STRENGTH 0x004b07d0u      /* the comparator of 0x893579 */
#define F_IS_HUMAN 0x007875e0u      /* thiscall on [HUMANS] (club) */
#define HUMANS 0xb7431cu

#define NAME_NORRA 0x7f10           /* stage names of our own (season_round_names_game.c) */
#define NAME_SODRA 0x7f11

int season_swe_barred(uint32_t club, uint32_t div);

static uint32_t call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

/* The plan of edition `year`, 0 for the class's six groups. */
static int plan_of(int year, SweGroupsPlan *p)
{
    static int off = -1;
    if (off < 0) off = st_env_is("RECOMP_SWE_GROUPS", "off");
    if (off || !season_changing_rules() || !season_sizes_on()) return 0;
    const SeasonRule *r = season_league_edition_row("swe_second", year);
    return r && season_swe_groups_plan(r->split_text, r->clubs, r->up, r->down, p);
}

/* 1 when the object is the Second Division in an edition of two groups. */
static int two_groups(uint32_t obj)
{
    return st_readable(obj) && rd32(g_mem, obj) == VT_SECOND && (int32_t)rd32(g_mem, obj + COMP_STAGE_COUNT) == 1;
}

static uint32_t sodra_of(uint32_t second)
{
    uint32_t stages = rd32(g_mem, second + COMP_STAGES);
    return st_readable(stages) ? rd32(g_mem, stages) : 0;
}

int season_swe_groups_skip_stage(uint32_t second, int group) { return two_groups(second) && group != 0; }

const char *season_swe_groups_name(int id)
{
    return id == NAME_NORRA ? "Norra" : id == NAME_SODRA ? "S\xf6" "dra" : NULL;
}

static int is_human(uint32_t club)
{
    uint32_t a[1] = {club}, humans = rd32(g_mem, HUMANS);
    return st_readable(humans) && call_this(F_IS_HUMAN, humans, 1, a) != 0;
}

/* A club changes division: at a new game by its record alone, as SwePrm's constructor 0x88fc23. */
static void move_club(uint32_t prm, uint32_t fn, uint32_t club, uint32_t record, int creating)
{
    if (creating) {
        wr32(g_mem, club + ST_CLUB_DIVISION, record);
        return;
    }
    uint32_t a[3] = {club, record, 1};
    call_this(fn, prm, 3, a);
}

static int region(uint32_t club, const uint32_t keys[3])
{
    int best = 0;
    for (int k = 0; k < 3 && !rt_unwinding; k++) {
        uint32_t a[2] = {club, rd32(g_mem, keys[k])};
        int fit = (int)rt_callback(F_REGION, 2, a);
        if (fit > best) best = fit;
    }
    return best;
}

/* The clubs that the groups sent down at this season change (season_swe_groups_down): the deal that follows on the
 * same day takes them back only when the Lower Division holds no other club. */
#define WENT_MAX 32
static uint32_t went_down[WENT_MAX];
static int went_count;

static int just_down(uint32_t club)
{
    for (int i = 0; i < went_count; i++)
        if (went_down[i] == club) return 1;
    return 0;
}

/* The clubs of the division that the plan holds, each in Norra or Sodra. */
static void deal(uint32_t prm, const SweGroupsPlan *p, int year, int creating)
{
    uint32_t table = rd32(g_mem, CLUBS);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    uint32_t second = st_comp_of(KEY_SECOND), lower = st_comp_of(KEY_LOWER);
    if (!st_readable(table) || !st_readable(second) || !st_readable(lower)) return;
    int have = 0, went = 0, came = 0;
    for (int i = 0; i < count; i++)
        have += rd32(g_mem, table + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION) == second;
    for (; have > p->clubs && !rt_unwinding; have--, went++) {
        uint32_t worst = 0;
        for (int i = 0; i < count; i++) {
            uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
            if (rd32(g_mem, c + ST_CLUB_DIVISION) != second || (!creating && is_human(c))) continue;
            if (!worst || (int16_t)rd16(g_mem, c + ST_CLUB_REPUTATION) <= (int16_t)rd16(g_mem, worst + ST_CLUB_REPUTATION))
                worst = c;
        }
        if (!worst) break;
        move_club(prm, F_DOWN, worst, lower, creating);
    }
    for (; have < p->clubs && !rt_unwinding; have++, came++) {
        uint32_t best = 0;
        for (int pass = 0; pass < 2 && !best; pass++)
            for (int i = 0; i < count; i++) {
                uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
                if (rd32(g_mem, c + ST_CLUB_DIVISION) != lower || !st_nation_is(c, KEY_SWEDEN) ||
                    season_swe_barred(c, second) || (!pass && just_down(c)))
                    continue;
                if (!best ||
                    (int16_t)rd16(g_mem, c + ST_CLUB_REPUTATION) > (int16_t)rd16(g_mem, best + ST_CLUB_REPUTATION))
                    best = c;
            }
        if (!best) break;
        move_club(prm, F_UP, best, second, creating);
    }
    if (rt_unwinding) return;
    went_count = 0;
    uint32_t list[SWE_GROUPS_MAX_CLUBS];
    int north[SWE_GROUPS_MAX_CLUBS], group[SWE_GROUPS_MAX_CLUBS], n = 0;
    for (int i = 0; i < count && n < SWE_GROUPS_MAX_CLUBS; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE;
        if (rd32(g_mem, c + ST_CLUB_DIVISION) != second) continue;
        list[n] = c;
        north[n] = region(c, NORTH_KEYS) - region(c, SOUTH_KEYS);
        if (rt_unwinding) return;
        n++;
    }
    season_swe_groups_deal(north, n, (n + 1) / 2, group);
    uint32_t norra = st_comp_of(KEY_NORRA), sodra = st_comp_of(KEY_SODRA);
    int in_norra = 0;
    for (int i = 0; i < n; i++) {
        wr32(g_mem, list[i] + CLUB_ZONE, group[i] ? sodra : norra);
        in_norra += !group[i];
    }
    int first = 0;
    for (int i = 0; i < count; i++)
        first += rd32(g_mem, table + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION) == st_comp_of(KEY_FIRST);
    fprintf(stderr, "recomp: swe groups: edition %d: Norra %d clubs, Sodra %d clubs (row 2 x %d), Superettan %d clubs, "
            "%d went down, %d came up\n", year, in_norra, n - in_norra, p->size, first, went, came);
}

/* SweSecond::create_table: Norra, the places of the group and the one stage of the edition. */
void o_008921b0(void);
RT_WRAP(008921b0, swe_groups_table)
{
    uint32_t self = cpu.ecx, table = rd32(g_mem, CLUBS), record = rd32(g_mem, self + COMP_RECORD);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT);
    int key = (int)rd32(g_mem, KEY_NORRA), n = 0;
    SweGroupsPlan p;
    int year = (int16_t)rd16(g_mem, self + COMP_YEAR);
    if (!plan_of(year, &p)) {
        o_008921b0();
        return;
    }
    /* Here, after SwePrm's chain, its deal and the size step (0x890642), so the groups hold the clubs that play. A
     * loaded game makes no table. */
    deal(st_league_of(KEY_PREMIER), &p, year, season_change_is_creating());
    if (rt_unwinding) return;
    for (int i = 0; i < count; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE, zone = rd32(g_mem, c + CLUB_ZONE);
        n += rd32(g_mem, c + ST_CLUB_DIVISION) == record && st_readable(zone) && (int)rd32(g_mem, zone) == key;
    }
    uint32_t rows = n > 0 ? season_sizes_game_malloc((uint32_t)n * ST_ROW_SIZE) : 0;
    if (!rows) {
        rt_ret_stdcall(0, 0);
        return;
    }
    wr32(g_mem, self + COMP_STAGE_COUNT, 1);
    wr16(g_mem, self + COMP_CLUBS, (uint16_t)n);
    wr32(g_mem, self + COMP_TABLE, rows);
    g_mem[self + UP] = (uint8_t)p.up;
    g_mem[self + UP_PLAYOFF] = 0;
    g_mem[self + DOWN_PLAYOFF] = 0;
    g_mem[self + DOWN] = (uint8_t)p.down;
    int row = 0;
    for (int i = 0; i < count && !rt_unwinding; i++) {
        uint32_t c = table + (uint32_t)i * ST_CLUB_SIZE, zone = rd32(g_mem, c + CLUB_ZONE);
        if (rd32(g_mem, c + ST_CLUB_DIVISION) != record || !st_readable(zone) || (int)rd32(g_mem, zone) != key) continue;
        uint32_t a[4] = {(uint32_t)row++, c, 0, 0};
        call_this(F_ADD_ROW, self, 4, a);
    }
    if (!rt_unwinding) rt_ret_stdcall(1, 0);
}

/* SweSecond::create_schedule: the rounds of a group of the plan. */
void o_00891a60(void);
RT_WRAP(00891a60, swe_groups_schedule)
{
    uint32_t self = cpu.ecx, rounds_at = rt_arg(1), name_at = rt_arg(2);
    int stage = (int8_t)(rt_arg(0) & 0xffu);
    SweGroupsPlan p;
    if (stage > 0 || !plan_of((int16_t)rd16(g_mem, self + COMP_YEAR), &p)) {
        o_00891a60();
        return;
    }
    o_00891a60();
    if (rt_unwinding) return;
    uint32_t old = cpu.eax;
    int have = (int16_t)rd16(g_mem, rounds_at);
    uint32_t dates = season_sizes_build_template(self, "swe_second", old, have, p.rounds, SEASON_TEMPLATE_SHUFFLED);
    if (rt_unwinding) return;
    if (dates) {
        season_sizes_game_free(old);
        cpu.eax = dates;
        wr16(g_mem, rounds_at, (uint16_t)p.rounds);
    }
    wr16(g_mem, name_at, stage < 0 ? NAME_NORRA : NAME_SODRA);
}

/* SweSecond::get_club_array: the clubs of the two groups. */
void o_00893320(void);
RT_WRAP(00893320, swe_groups_clubs)
{
    uint32_t self = cpu.ecx, out = rt_arg(0), count_at = rt_arg(1), sodra = sodra_of(self);
    if (!two_groups(self) || !st_readable(sodra)) {
        o_00893320();
        return;
    }
    int n1, n2;
    uint32_t t1 = st_table_of(self, &n1), t2 = st_table_of(sodra, &n2);
    uint32_t list = season_sizes_game_malloc((uint32_t)(n1 + n2) * 4u);
    wr32(g_mem, out, list);
    wr16(g_mem, count_at, list ? (uint16_t)(n1 + n2) : 0);
    for (int i = 0; list && i < n1 + n2; i++) {
        uint32_t row = i < n1 ? t1 + (uint32_t)i * ST_ROW_SIZE : t2 + (uint32_t)(i - n1) * ST_ROW_SIZE;
        wr32(g_mem, list + 4u * (uint32_t)i, rd32(g_mem, row));
    }
    rt_ret_stdcall(0, 2);
}

/* SweSecond::set_predicted_positions: each group by strength, as 0x893548..0x8935ab. */
void o_00893470(void);
RT_WRAP(00893470, swe_groups_predicted)
{
    uint32_t self = cpu.ecx, sodra = sodra_of(self), stats = rd32(g_mem, self + COMP_STATS);
    if (!two_groups(self) || !st_readable(sodra) || !st_readable(stats)) {
        o_00893470();
        return;
    }
    uint32_t groups[2] = {self, sodra};
    for (int g = 0; g < 2 && !rt_unwinding; g++) {
        int n;
        uint32_t rows = st_table_of(groups[g], &n), block = n > 0 ? season_sizes_game_malloc((uint32_t)n * 4u) : 0;
        if (!block) continue;
        for (int i = 0; i < n; i++) wr32(g_mem, block + 4u * (uint32_t)i, rd32(g_mem, rows + (uint32_t)i * ST_ROW_SIZE));
        uint32_t q[4] = {block, (uint32_t)n, 4, F_STRENGTH};
        rt_callback(F_QSORT, 4, q);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t a[2] = {rd32(g_mem, block + 4u * (uint32_t)i), (uint32_t)(i * 6 + 1)};
            call_this(F_PREDICTED, stats, 2, a);
        }
        if (!rt_unwinding) season_sizes_game_free(block);
    }
    if (!rt_unwinding) rt_ret_stdcall(0, 0);
}

/* The pairs of the chain (0x689c80 with pairs 1 swaps the rows of status 3 of the upper table, first row first, with the
 * rows of status 0 or 5 of the lower table, 0x689d1a..0x689dff). The Superettan class sends 3 down (0x88f099), so only
 * its last rows, as many as the groups send up, keep status 3, and the groups share the swaps. A group's other up rows
 * lose their status here. fill_first takes them, best placed first, if the Superettan needs them. */
static void share_places(uint32_t first, uint32_t second, uint32_t sodra)
{
    int nf, n[2], ups = 0, down = 0;
    uint32_t rows = st_table_of(first, &nf), groups[2] = {second, sodra}, t[2];
    for (int g = 0; g < 2; g++) {
        t[g] = st_table_of(groups[g], &n[g]);
        for (int i = 0; i < n[g]; i++) {
            int st = g_mem[t[g] + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS];
            ups += st == ST_UP || st == ST_TITLE;
        }
    }
    for (int i = 0; i < nf; i++) down += g_mem[rows + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS] == ST_DOWN;
    int pairs = ups < down ? ups : down, drop = down - pairs;
    for (int i = 0; i < nf && drop > 0; i++) {
        uint32_t row = rows + (uint32_t)i * ST_ROW_SIZE;
        if (g_mem[row + ST_ROW_STATUS] != ST_DOWN) continue;
        g_mem[row + ST_ROW_STATUS] = ST_NONE;
        drop--;
    }
    int quota[2] = {(pairs + 1) / 2, pairs / 2};
    for (int g = 0; g < 2; g++)
        for (int i = 0; i < n[g]; i++) {
            uint32_t row = t[g] + (uint32_t)i * ST_ROW_SIZE;
            int st = g_mem[row + ST_ROW_STATUS];
            if (st != ST_UP && st != ST_TITLE) continue;
            if (quota[g] > 0) quota[g]--;
            else g_mem[row + ST_ROW_STATUS] = ST_NONE;
        }
}

/* The promotion chain of SwePrm 0x8907f0, with the two groups in place of the six. */
void o_008907f0(void);
RT_WRAP(008907f0, swe_groups_chain)
{
    uint32_t self = cpu.ecx, second = st_league_of(KEY_SECOND), sodra = two_groups(second) ? sodra_of(second) : 0;
    if (!st_readable(sodra)) {
        o_008907f0();
        return;
    }
    uint32_t prm = st_league_of(KEY_PREMIER), first = st_league_of(KEY_FIRST);
    uint32_t finish[3] = {prm, first, second};
    uint32_t pairs[3][2] = {{prm, first}, {first, second}, {first, sodra}};
    for (int i = 0; i < 2 && !rt_unwinding; i++)
        call_this(rd32(g_mem, rd32(g_mem, finish[i]) + VT_FINISH), finish[i], 0, NULL);
    for (int i = 0; i < 3 && !rt_unwinding; i++) {
        if (i == 1) {
            call_this(rd32(g_mem, rd32(g_mem, second) + VT_FINISH), second, 0, NULL);
            if (rt_unwinding) break;
            share_places(first, second, sodra);
        }
        uint32_t a[6] = {pairs[i][0], pairs[i][1], 1, 1, 0xffffffffu, 0xffffffffu};
        call_this(F_CHAIN, self, 6, a);
    }
    if (!rt_unwinding) rt_ret_stdcall(0, 1);
}

int season_swe_groups_down(uint32_t prm)
{
    uint32_t second = st_league_of(KEY_SECOND), sodra = two_groups(second) ? sodra_of(second) : 0;
    if (!st_readable(sodra)) return 0;
    uint32_t lower = st_comp_of(KEY_LOWER), groups[2] = {second, sodra};
    went_count = 0;
    for (int g = 0; g < 2 && !rt_unwinding; g++) {
        int n;
        uint32_t rows = st_table_of(groups[g], &n);
        for (int i = 0; i < n && !rt_unwinding; i++) {
            uint32_t row = rows + (uint32_t)i * ST_ROW_SIZE;
            if (g_mem[row + ST_ROW_STATUS] != ST_DOWN) continue;
            if (went_count < WENT_MAX) went_down[went_count++] = rd32(g_mem, row);
            move_club(prm, F_DOWN, rd32(g_mem, row), lower, 0);
        }
    }
    return 1;
}

/* The Superettan after the chain: when the Allsvenskan grew it holds fewer clubs than its next row, and the best
 * placed clubs of the two groups, taken in turn, fill it. The tables of the season that ended are still there. */
static void fill_first(uint32_t prm, uint32_t second, int next)
{
    const SeasonRule *r = season_league_edition_row("swe_first", next);
    uint32_t table = rd32(g_mem, CLUBS), first = st_comp_of(KEY_FIRST), record = st_comp_of(KEY_SECOND);
    int count = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, NATION_COUNT), have = 0;
    if (!r || r->clubs <= 0 || !st_readable(table) || !st_readable(first)) return;
    for (int i = 0; i < count; i++) have += rd32(g_mem, table + (uint32_t)i * ST_CLUB_SIZE + ST_CLUB_DIVISION) == first;
    uint32_t groups[2] = {second, sodra_of(second)};
    int at[2] = {0, 0}, g = 0, idle = 0;
    while (have < r->clubs && idle < 2 && !rt_unwinding) {
        int n;
        uint32_t rows = st_table_of(groups[g], &n), club = 0;
        while (at[g] < n && !club) {
            uint32_t c = rd32(g_mem, rows + (uint32_t)at[g]++ * ST_ROW_SIZE);
            if (rd32(g_mem, c + ST_CLUB_DIVISION) == record && !season_swe_barred(c, first)) club = c;
        }
        idle = club ? 0 : idle + 1;
        if (club) {
            move_club(prm, F_UP, club, first, 0);
            have++;
        }
        g = !g;
    }
}

/* SwePrm's deal of the clubs into six sections (called at 0x89063a, before the edition number goes up at 0x890670).
 * In an edition of two groups create_table deals the clubs. */
void o_00890c70(void);
RT_WRAP(00890c70, swe_groups_deal)
{
    uint32_t self = cpu.ecx, second = st_league_of(KEY_SECOND);
    int next = (int16_t)rd16(g_mem, self + COMP_YEAR) + 1;
    SweGroupsPlan p;
    if (!st_readable(second) || !plan_of(next, &p)) {
        o_00890c70();
        return;
    }
    if (two_groups(second)) fill_first(self, second, next);
    if (!rt_unwinding) rt_ret_stdcall(0, 0);
}
