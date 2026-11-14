/* The Copa America of 16 teams (0.6.0 slice 27): the Copa America Centenario of 2016 (int-continental-165) and
 * the 2024 Copa America (int-continental-253) have 4 groups of 4, the top two to the quarter-finals, then
 * semi-finals, a third-place match and the final. Entry: the 10 CONMEBOL nations and 6 CONCACAF guests. The
 * edition table (season_intl_table.c) gives the team count. The base game (season_intl_on() 0) and every
 * edition of 12 teams take the original path: each site and wrap here gives the original's value or calls the
 * original then.
 *
 * The class copa_america (goldcup.cpp, ctor 0x5e0660, vtable 0x96b400). The League object is group A,
 * stage[0..1] (+0xc, count +0x2c = 3 at 0x5e0729) are groups B and C, stage[2] the Cup_Stage of 8 (0x5e18a0, with
 * the two best thirds). The entrants (0x5e1cd0) write 12 team pointers to +0xee (+0xf2 the count as a dword,
 * 0x5e1cde), sorted by strength (0x5e20fe). There is no random draw: group g takes the teams g, g + 3, g + 6,
 * g + 9 (group A 0x5e128e, groups B and C 0x5e1721..0x5e1735). With 16 teams stage[0..2] are groups B to D and
 * stage[3] the Cup_Stage of 8 from the top two of each group, and group g takes g, g + 4, g + 8, g + 12.
 *
 * Sources (read 2026-10-18): https://en.wikipedia.org/wiki/Copa_Am%C3%A9rica_Centenario (guests United States,
 * Mexico, Costa Rica, Jamaica, Haiti, Panama; quarter-finals A1-B2, B1-A2, C1-D2, D1-C2, semi-finals B1/A2 side
 * against C1/D2 side and A1/B2 side against D1/C2 side) and https://en.wikipedia.org/wiki/2024_Copa_Am%C3%A9rica
 * (guests Canada, Costa Rica, Jamaica, Mexico, Panama, United States; quarter-finals A1-B2, B1-A2, C1-D2, D1-C2,
 * semi-finals QF1-QF2 and QF3-QF4, read from the matches Argentina-Canada and Uruguay-Colombia). The rows of
 * int-continental.tsv give "10 CONMEBOL and 6 CONCACAF teams" without names. The names are those of the 3.9.68
 * nation.dat.
 *
 * Left out: the planned 2020 Copa America (int-continental-209, 2 zones of 6, top four to the quarter-finals,
 * guests Australia and Qatar). Its groups of 6 need a group template of 5 matchdays and 15 fixtures (the
 * original's group templates have 3 matchdays for 4 teams, 0x5e0900), tables of 6 (group A is made with 4 rows of
 * 0x3b bytes, 0xec at 0x5e11e0..0x5e11ee) and group builders of 6 (they copy exactly 4 teams: 0x5e171c, malloc
 * 0x10 at 0x5e16d0), and a knockout from the top four of two groups. So it keeps the shape of 12. */
#include "win.h"
#include "season_change.h"
#include "season_intl.h"
#include "season_nl.h"                     /* slice 28: the Gold Cup */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COPA_VTABLE 0x96b400u
#define COPA_KEY 0x9cf7a8u              /* key global of the Copa America (0x5e1f55) */
#define HOSTS 0xb5e134u                 /* HostCountry */
#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u          /* continent record, its +0 the continent id */
#define NATION_REPUTATION 0x8eu         /* s16 (season_intl.c) */
#define CONTINENT_SOUTH_AMERICA 0x9cfa1cu /* 0x5e1fd7 */
#define CONTINENT_NORTH_AMERICA 0x9cfa14u /* sweep section 8, 0x91633e */

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (0x5e20f5, 0x5e2416) */
#define NATIONAL_TEAM 0x0053b3d0u       /* national team club of a nation record, cdecl (nation) */
#define FIND_HOSTS 0x005fa730u          /* HostCountry::find_hosts, thiscall (key, year, &host1, &host2, 1) */
#define SET_FINAL_POSITION 0x004a2540u  /* Comp_Stats::set_club_final_position, thiscall (club, position) */
#define CUP_STAGE_NEW 0x00522e00u       /* Cup_Stage ctor, thiscall, 15 arguments (ret 0x3c at 0x5230b9) */
#define CUP_START 0x0051c800u           /* thiscall (0): 0x5e1c9c */

#define TABLE 0xb1u                     /* League table: rows of 0x3b bytes, +0 the club (0x6826d0) */
#define ROW_SIZE 0x3bu
#define TEAMS_OF 0xeeu                  /* team pointers, 4 bytes each */
#define TEAM_COUNT 0xf2u                /* dword (0x5e1cde) */

#define TEAMS 16
#define GROUPS 4
#define KO 3                            /* knockout stage index with 16 teams (2 with 12) */

/* The CONCACAF guests and the quarter-finals of each edition of 16. Bracket: the 8 teams in pair order ("1A"
 * winner of A, "2A" runner-up of A); the Cup_Stage pairs 0-1, 2-3 and so on and the winners in the same order
 * (inference, see season_intl_euro.c). */
static const struct {
    int year;
    const char *guests[6];
    const char *bracket[8];
} editions[] = {
    {2016, {"United States", "Mexico", "Costa Rica", "Jamaica", "Haiti", "Panama"},
     {"1B", "2A", "1C", "2D", "1A", "2B", "1D", "2C"}},
    {2024, {"United States", "Mexico", "Canada", "Jamaica", "Panama", "Costa Rica"},
     {"1A", "2B", "1B", "2A", "1C", "2D", "1D", "2C"}},
};
#define EDITIONS ((int)(sizeof editions / sizeof editions[0]))

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

static uint32_t vcall(uint32_t self, uint32_t slot, int nargs, const uint32_t *args)
{
    return call_this(rd32(g_mem, rd32(g_mem, self) + slot), self, nargs, args);
}

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + 0x40u); }

/* The index into editions[] when the object is a Copa America of 16 teams in a season game, else -1. */
static int copa16(uint32_t self)
{
    /* Slice 28 part b: the CONCACAF Gold Cup object (season_gold_cup.c, by its record) of 16 teams takes the
     * shape and the bracket of the 2024 edition (decision). */
    if (season_gold_cup_of(self)) return season_gold_cup_teams(self) == TEAMS ? EDITIONS - 1 : -1;
    if (!season_intl_on() || !readable(self) || rd32(g_mem, self) != COPA_VTABLE) return -1;
    const SeasonIntlEdition *e = season_intl_edition("goldcup", year_of(self));
    if (!e || e->teams != TEAMS) return -1;
    for (int i = 0; i < EDITIONS; i++)
        if (editions[i].year == year_of(self)) return i;
    return -1;
}

/* ---- Sites of seasons.py ---- */

/* The knockout stage index: 2 in the original (0x5e0f56, 0x5e13b5, 0x5e169f, 0x5e22eb, and 4 times it at
 * 0x5e1437). The stage count is one more (0x5e0710, 0x5e0729). */
uint32_t rt_intl_copa_ko(uint32_t self, uint32_t original)
{
    return copa16(self) >= 0 ? (uint32_t)KO : original;
}

/* The group count, which is also the step between the teams of a group in +0xee: 3 in the original (group A
 * 0x5e128e, groups B and C 0x5e172e and 0x5e1735). */
uint32_t rt_intl_copa_groups(uint32_t self, uint32_t original)
{
    return copa16(self) >= 0 ? (uint32_t)GROUPS : original;
}

/* start_new_season 0x5e1530 keeps the stage array of the ctor. After the step of the year (0x5e1590) an
 * edition of another size gets a new array (0x5e1595). A save keeps the count: the League load makes the array
 * from it (0x688e11, 0x688e53). */
void rt_intl_copa_stages(uint32_t self)
{
    if (!season_intl_on() || !readable(self) || (rd32(g_mem, self) != COPA_VTABLE && !season_gold_cup_of(self))) return;
    uint32_t want = rt_intl_copa_ko(self, 2u) + 1u, have = rd32(g_mem, self + 0x2cu);
    if (have == want) return;
    uint32_t stages = game_malloc(4u * want);
    if (rt_unwinding || !stages) return;
    game_free(rd32(g_mem, self + 0xcu));
    if (rt_unwinding) return;
    wr32(g_mem, self + 0xcu, stages);
    wr32(g_mem, self + 0x2cu, want);
    if (logging()) fprintf(stderr, "recomp: season intl: goldcup %d has %u stages\n", year_of(self), want);
}

/* ---- The template function (0x5e0900) ---- */

/* thiscall (index as a byte, short *rounds, short *id, int *flag), ret 0x10. Called by the wrap of 0x5e0900 in
 * season_intl.c inside its date shift, at the entry state (cpu.esp at the return address, this in cpu.ecx). It
 * always completes the call and gives the result.
 *
 * 16 teams: index -1 to 2 are the groups A to D, 3 the knockout. The original has 3 group templates (index -1
 * to 1, 0x5e0a1b..0x5e0a2b) and its knockout at index 2 (0x5e0912): quarter-finals, semi-finals, third place
 * and final for 8 teams (0x5e0be0..0x5e0f38). So group D takes the template of group A (inference: then its
 * kick-off flags are those of group A) with the id 0x420 + index (0x5e0940, the text of 0x422 is unknown), and
 * the knockout is the original's with index 2. */
uint32_t season_intl_copa_template(void (*original)(void))
{
    uint32_t self = cpu.ecx, sp = cpu.esp;
    int idx = (int8_t)rd8(g_mem, sp + 4u);
    if (copa16(self) < 0 || idx < -1 || idx > KO) {
        original();
        return cpu.eax;
    }
    int mapped = idx == KO ? 2 : (idx + 1) % 3 - 1;
    uint32_t out_id = rd32(g_mem, sp + 0xcu);
    wr8(g_mem, sp + 4u, (uint32_t)mapped);
    original();
    if (rt_unwinding) return cpu.eax;
    if (idx < KO && mapped != idx && readable(out_id)) wr16(g_mem, out_id, (uint32_t)(idx + 0x420));
    return cpu.eax;
}

/* ---- Entrants (0x5e1cd0) ---- */

static int nation_count(void) { return (int)rd32(g_mem, NATION_COUNT); }
static uint32_t nation_at(int id) { return rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE; }

static uint32_t continent_of(uint32_t nation)
{
    uint32_t c = rd32(g_mem, nation + NATION_CONTINENT);
    return readable(c) ? rd32(g_mem, c) : 0xffffffffu;
}

static uint32_t team_nation(uint32_t club) { return readable(club) ? rd32(g_mem, club + 0x53u) : 0; }

static int nation_in(const uint32_t *teams, int n, uint32_t nation)
{
    for (int k = 0; k < n; k++)
        if (team_nation(teams[k]) == nation) return 1;
    return 0;
}

/* The national team of a nation when its club names the nation (season_intl.c fill_slots: a season database
 * can give an empty row), else 0. */
static uint32_t team_of(uint32_t nation)
{
    uint32_t a[1] = {nation};
    uint32_t club = call_cdecl(NATIONAL_TEAM, 1, a);
    return !rt_unwinding && team_nation(club) == nation ? club : 0;
}

/* The nation of the continent with the highest reputation that is not in yet, or 0. */
static uint32_t strongest(uint32_t continent, const uint32_t *teams, int n)
{
    uint32_t best = 0;
    int best_rep = -100000;
    for (int id = 0; id < nation_count(); id++) {
        uint32_t nation = nation_at(id);
        int rep = (int16_t)rd16(g_mem, nation + NATION_REPUTATION);
        if (rep <= best_rep || continent_of(nation) != rd32(g_mem, continent) || nation_in(teams, n, nation)) continue;
        if (!team_of(nation)) continue;
        best = nation;
        best_rep = rep;
    }
    return best;
}

static int add_team(uint32_t *teams, int n, uint32_t nation)
{
    if (!nation || n >= TEAMS || nation_in(teams, n, nation)) return n;
    uint32_t club = team_of(nation);
    if (rt_unwinding || !club) return n;
    teams[n] = club;
    return n + 1;
}

/* The original takes the hosts (find_hosts 0x5e1f5b), then the South American nations and the guests of its
 * year (0x5e1fbd..0x5e206c), must find exactly 12 (0x5e207c) and sorts all by strength (0x5e20fe). Here: the
 * hosts first and kept in front (so the first host is the first seed and plays in group A, as the United
 * States in 2016 and 2024), then the CONMEBOL nations, then the guests by name. A guest that the data lacks is
 * replaced by the CONCACAF nation with the highest reputation that is not in, and a missing CONMEBOL nation
 * by the next CONMEBOL nation, then CONCACAF. The rest is sorted by strength as the original.
 * The replacement is only a logged net (O259 row A29): the seven guest names and the ten CONMEBOL nations are
 * key nations of match_nation_key_id 0x60d810, so a database without one stops at load in the original's own
 * check (box "index.cpp 5809", 0x60d7f0; a copy of October 2015 with Haiti renamed). The 51 databases of the
 * season tiles and the 3.9.68 data hold all of them (the O188 nation sweep, 2026-10-22). Only a database that
 * moves a CONMEBOL nation to another continent reaches the CONMEBOL fill. */
static uint32_t entrants16(int e, uint32_t self)
{
    uint32_t teams[TEAMS] = {0};
    int n = 0, hosts = 0;
    uint32_t out = game_malloc(8);
    if (rt_unwinding) return 0;
    if (out) {
        wr32(g_mem, out, 0xffffffffu);
        wr32(g_mem, out + 4u, 0xffffffffu);
        uint32_t fa[5] = {rd32(g_mem, COPA_KEY), (uint32_t)year_of(self), out, out + 4u, 1};
        int count = (int8_t)call_this(FIND_HOSTS, rd32(g_mem, HOSTS), 5, fa);
        if (rt_unwinding) return 0;
        for (int h = 0; h < count && h < 2; h++) {
            int id = (int32_t)rd32(g_mem, out + 4u * (uint32_t)h);
            if (id >= 0 && id < nation_count()) n = add_team(teams, n, nation_at(id));
        }
        game_free(out);
        if (rt_unwinding) return 0;
    }
    hosts = n;
    uint32_t south = rd32(g_mem, CONTINENT_SOUTH_AMERICA);
    int conmebol = 0;
    for (int k = 0; k < n; k++) conmebol += continent_of(team_nation(teams[k])) == south;
    for (int id = 0; id < nation_count() && n < TEAMS && conmebol < 10; id++) {
        uint32_t nation = nation_at(id);
        if (continent_of(nation) != south) continue;
        int before = n;
        n = add_team(teams, n, nation);
        if (rt_unwinding) return 0;
        conmebol += n > before;
    }
    for (int g = 0; g < 6 && n < TEAMS; g++) {
        int found = 0;
        for (int id = 0; id < nation_count() && !found; id++)
            if (!strcmp(win_str(nation_at(id) + NATION_NAME), editions[e].guests[g])) {
                found = 1;
                n = add_team(teams, n, nation_at(id));
            }
        if (rt_unwinding) return 0;
        if (!found && logging())
            fprintf(stderr, "recomp: season intl: copa %d: no nation %s, a CONCACAF nation plays\n", year_of(self),
                    editions[e].guests[g]);
    }
    if (conmebol < 10 && logging())
        fprintf(stderr, "recomp: season intl: copa %d: %d CONMEBOL nations in the data, the strongest others play\n",
                year_of(self), conmebol);
    for (int k = conmebol; k < 10 && n < TEAMS; k++) {
        n = add_team(teams, n, strongest(CONTINENT_SOUTH_AMERICA, teams, n));
        if (rt_unwinding) return 0;
    }
    while (n < TEAMS) {
        int before = n;
        n = add_team(teams, n, strongest(CONTINENT_NORTH_AMERICA, teams, n));
        if (rt_unwinding) return 0;
        if (n == before) break;
    }
    if (rt_unwinding) return 0;

    uint32_t list = game_malloc(4u * TEAMS);
    if (rt_unwinding || !list) return 0;
    for (int k = 0; k < n; k++) wr32(g_mem, list + 4u * (uint32_t)k, teams[k]);
    if (n - hosts > 1) {
        uint32_t qa[4] = {list + 4u * (uint32_t)hosts, (uint32_t)(n - hosts), 4, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
        if (rt_unwinding) return 0;
    }
    wr32(g_mem, self + TEAMS_OF, list);
    wr32(g_mem, self + TEAM_COUNT, TEAMS);
    if (logging())
        fprintf(stderr, "recomp: season intl: goldcup %d: %d teams (%d hosts)\n", year_of(self), n, hosts);
    if (n != TEAMS) {
        fprintf(stderr, "recomp: season intl: goldcup %d: only %d teams of %d\n", year_of(self), n, TEAMS);
        return 0;
    }
    return 1;
}

void o_005e1cd0(void);

/* copa_america entrants, thiscall (), returns 1 or 0 (0x5e21f2). */
RT_WRAP(005e1cd0, copa_entrants)
{
    uint32_t self = cpu.ecx;
    if (season_gold_cup_of(self)) {             /* slice 28 part b: the Gold Cup's own entrants */
        uint32_t r = season_gold_cup_entrants(self);
        if (rt_unwinding) return;
        rt_ret_stdcall(r, 0);
        return;
    }
    int e = copa16(self);
    if (e < 0) {
        o_005e1cd0();
        return;
    }
    uint32_t r = entrants16(e, self);
    if (rt_unwinding) return;
    rt_ret_stdcall(r, 0);
}

/* ---- Predicted positions (vtable +0x5c, 0x5e2340) ---- */

void o_005e2340(void);

/* The original sorts the teams with 0x4b07d0 (0x5e2431) and gives 12 positions 1, 2, 3, 3, 5 x 4, 9, 10, 10, 10
 * (0x5e243f..0x5e24dc). With 16 teams: 1, 2, 3, 3, 5 x 4, 9 x 8. */
RT_WRAP(005e2340, copa_predicted)
{
    static const int pos16[TEAMS] = {1, 2, 3, 3, 5, 5, 5, 5, 9, 9, 9, 9, 9, 9, 9, 9};
    uint32_t self = cpu.ecx, stats = readable(self) ? rd32(g_mem, self + 8u) : 0;
    uint32_t teams = readable(self) ? rd32(g_mem, self + TEAMS_OF) : 0;
    int n = readable(self) ? (int)rd32(g_mem, self + TEAM_COUNT) : 0;
    if (copa16(self) < 0 || !stats || !readable(teams) || n <= 0) {
        o_005e2340();
        return;
    }
    if (n > TEAMS) n = TEAMS;
    uint32_t list = game_malloc((uint32_t)n * 4u);
    if (rt_unwinding) return;
    if (list) {
        memcpy(g_mem + list, g_mem + teams, (size_t)n * 4u);
        uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
        if (rt_unwinding) return;
        for (int k = 0; k < n; k++) {
            uint32_t club = rd32(g_mem, list + 4u * (uint32_t)k);
            if (!club) continue;
            uint32_t pa[2] = {club, (uint32_t)pos16[k]};
            call_this(SET_FINAL_POSITION, stats, 2, pa);
            if (rt_unwinding) return;
        }
        game_free(list);
        if (rt_unwinding) return;
    }
    rt_ret_stdcall(cpu.eax, 0);
}

/* ---- Final positions (vtable +0x68, 0x5e2500) ---- */

void o_005e2500(void);

/* thiscall (club, stage as a byte, a, b, c), ret 0x14. The original maps the places of a group stage (stage
 * below 2, 0x5e2516) and passes the knockout's on. It loses this at 0x5e2504, so the test cannot be a site.
 * With 16 teams group D (stage 2) takes the group branch and the knockout 3 the knockout branch. */
RT_WRAP(005e2500, copa_positions)
{
    if (copa16(cpu.ecx) >= 0) {
        int stage = (int8_t)rd8(g_mem, cpu.esp + 8u);
        if (stage == 2) wr8(g_mem, cpu.esp + 8u, 0);
        else if (stage == KO) wr8(g_mem, cpu.esp + 8u, 2);
    }
    o_005e2500();
}

/* ---- The knockout stage (vtable +0x30 0x40f590, builder 0x5e18a0) ---- */

void o_0040f590(void);

/* vtable +0x30 of the Copa America and the Asian Cup (0x96b430, 0x967644): a jump to vtable +0x30 of stage[2]
 * (0x40f590..0x40f598). The Asian Cup's stage index is a site of season_intl_cups.c (0x40f593). For a Copa
 * America of 16 the jump goes to stage[3]. */
/* One wrap of 0x40f590 serves every slice. Slice 26's season_club_world_history_thunk() (season_change.h,
 * season_kind_club_world.c: the Club World Championship's knockout of Form K and Form G) runs first and gives 1
 * when it handled the call. */

RT_WRAP(0040f590, copa_asian_final_stage)
{
    uint32_t self = cpu.ecx;
    if (season_club_world_history_thunk() || rt_unwinding) return;
    cpu.ecx = self;
    if (copa16(self) < 0) {
        o_0040f590();
        return;
    }
    uint32_t stage = rd32(g_mem, rd32(g_mem, self + 0xcu) + 4u * KO);
    cpu.ecx = stage;
    rt_call(rd32(g_mem, rd32(g_mem, stage) + 0x30u));
}

static uint32_t table_club(uint32_t stage, int pos)
{
    uint32_t table = readable(stage) ? rd32(g_mem, stage + TABLE) : 0;
    return readable(table) ? rd32(g_mem, table + (uint32_t)pos * ROW_SIZE) : 0;
}

/* The knockout of 16 teams: the top two of 4 groups in the bracket of the edition. The original (0x5e18a0)
 * ranks the thirds of 3 groups, draws the 8 at random (0x4afb40 at 0x5e1b94), makes the template (index 2,
 * 0x5e1bab) and the Cup_Stage of 8 at stage[2] (0x5e1bfc, argument 11 is 0) and starts it (0x5e1c9c). */
static void knockout16(int e, uint32_t self)
{
    uint32_t stages = rd32(g_mem, self + 0xcu);
    if ((int)rd32(g_mem, self + 0x2cu) <= KO || !readable(stages)) {
        fprintf(stderr, "recomp: season intl: goldcup %d: %d stages, no room for the knockout\n", year_of(self),
                (int)rd32(g_mem, self + 0x2cu));
        return;
    }
    uint32_t g[GROUPS];
    g[0] = self;
    for (int i = 1; i < GROUPS; i++) g[i] = rd32(g_mem, stages + 4u * (uint32_t)(i - 1));
    uint32_t teams = game_malloc(8u * 4u), out = game_malloc(8);
    if (rt_unwinding) return;
    if (!teams || !out) {                               /* no leak when only one block came */
        if (teams) game_free(teams);
        if (out) game_free(out);
        return;
    }
    for (int k = 0; k < 8; k++) {
        const char *b = editions[e].bracket[k];
        wr32(g_mem, teams + 4u * (uint32_t)k, table_club(g[b[1] - 'A'], b[0] - '1'));
    }
    uint32_t xa[4] = {KO, out, out + 4u, 0};
    uint32_t tmpl = vcall(self, 0x3cu, 4, xa);
    if (rt_unwinding) return;
    uint32_t ba[1] = {0xb2}, zero[1] = {0};
    uint32_t cup = call_cdecl(GAME_NEW, 1, ba);
    if (rt_unwinding) return;
    if (readable(cup)) {
        uint32_t ca[15] = {self, 8, teams, (uint32_t)(int16_t)rd16(g_mem, out), rd32(g_mem, self + 4u), tmpl,
                           (uint32_t)year_of(self), KO, 4, (uint32_t)(int16_t)rd16(g_mem, out + 4u), 0x14, 0, 0, 0, 0};
        cup = call_this(CUP_STAGE_NEW, cup, 15, ca);
        if (rt_unwinding) return;
    } else {
        cup = 0;
    }
    wr32(g_mem, stages + 4u * KO, cup);
    game_free(teams);
    game_free(tmpl);
    game_free(out);
    if (rt_unwinding) return;
    if (!cup) {
        fprintf(stderr, "recomp: season intl: goldcup %d: no knockout stage\n", year_of(self));
        return;
    }
    call_this(CUP_START, cup, 1, zero);
    if (rt_unwinding) return;
    wr32(g_mem, self + 0x30u, KO);
    if (logging()) fprintf(stderr, "recomp: season intl: goldcup %d quarter-finals of 16 teams\n", year_of(self));
}

void o_005e18a0(void);

/* copa_america::create_knockout_stage, thiscall (), from the stage step 0x5e1620 (0x5e16a4) when the stage index
 * reaches ko (site 0x5e169f). */
RT_WRAP(005e18a0, copa_knockout)
{
    uint32_t self = cpu.ecx;
    int e = copa16(self);
    if (e < 0) {
        o_005e18a0();
        return;
    }
    knockout16(e, self);
    if (rt_unwinding) return;
    rt_ret_stdcall(cpu.eax, 0);
}
