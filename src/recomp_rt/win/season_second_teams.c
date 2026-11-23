/* Second teams: shared code of the nation rules (open items 111 and 114 to 122). See season_second_teams.h.
 *
 * How the original sets the places: League::update_positions_and_status 0x684cd0 (vtable +0x94) sorts the table
 * (0x6827d0 at 0x684d18), gives each position a zone from the places +0xbe..+0xc1 and sets a row's status (+0x37)
 * when the row can no longer leave its zone: 5 the title, 0 promotion, 1 the play-off, 3 relegation. A row of status
 * 0xfc is no part of the zones from the top (0x684dea..0x684df4, 0x684e46..0x684e53) and gets neither the title,
 * promotion nor the play-off (0x68512e..0x685137, 0x68517f..0x68518b), so the next row takes its place; in the zone
 * of relegation it gets 3 (0x68525e). This is the original's own mark for a B side whose senior side plays above
 * (set at 0x5dd942 for Germany, through club_get_linked_b_team 0x540a50). The chain 0x689c20 walks the leagues
 * below (+0x20) and calls league_promote_and_relegate 0x689c80 for each pair (0x689c60), which moves the rows of
 * status 0 or 5 of the lower table up and the rows of status 3 of the upper table down. */
#include "win.h"
#include "season_second_teams.h"
#include "season_level_ties.h"
#include "season.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define COMPS 0xae23d0u
#define COMP_SIZE 0x6bu
#define LEAGUES 0xadadfcu               /* the competition objects, by competition id (0x792966) */
#define LEAGUE_CLUBS 0x3eu              /* s16 rows */
#define LEAGUE_TABLE 0xb1u              /* rows of 0x3b bytes */
#define MAX_HOOKS 16

static const SecondTeamHooks *hooks[MAX_HOOKS];
static int nhooks;

void season_second_register(const SecondTeamHooks *h)
{
    if (nhooks < MAX_HOOKS) hooks[nhooks++] = h;
}

int season_second_barred(uint32_t club, uint32_t div)
{
    for (int i = 0; i < nhooks; i++)
        if (hooks[i]->barred_at_start && hooks[i]->barred_at_start(club, div)) return 1;
    return 0;
}

int st_readable(uint32_t p) { return rt_readable(p); }

int st_env_is(const char *name, const char *value)
{
    const char *v = getenv(name);
    return v && !strcmp(v, value);
}

uint32_t st_clubs(int *count)
{
    uint32_t table = rd32(g_mem, CLUBS);
    *count = st_readable(table) ? (int)rd32(g_mem, CLUB_COUNT) : 0;
    if (*count <= 0) *count = 0;
    return *count ? table : 0;
}

int st_nation_is(uint32_t club, uint32_t nation_key)
{
    uint32_t nation = rd32(g_mem, club + ST_CLUB_NATION);
    return st_readable(nation) && (int32_t)rd32(g_mem, nation) == (int32_t)rd32(g_mem, nation_key);
}

uint32_t st_comp_of(uint32_t key)
{
    return rd32(g_mem, COMPS) + rd32(g_mem, key) * COMP_SIZE;
}

uint32_t st_league_of(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    int32_t id = (int32_t)rd32(g_mem, key);
    if (!st_readable(leagues) || id < 0 || id > 0xffff) return 0;
    uint32_t league = rd32(g_mem, leagues + 4u * (uint32_t)id);
    return st_readable(league) ? league : 0;
}

int st_season_year(uint32_t key)
{
    uint32_t league = st_league_of(key);
    int start = season_start_year();
    int y = league ? (int16_t)rd16(g_mem, league + 0x40u) : 0;
    return y >= start && y <= start + 100 ? y : start;
}

uint32_t st_table_of(uint32_t league, int *n)
{
    *n = 0;
    if (!st_readable(league)) return 0;
    uint32_t table = rd32(g_mem, league + LEAGUE_TABLE);
    int rows = (int16_t)rd16(g_mem, league + LEAGUE_CLUBS);
    if (!st_readable(table) || rows <= 0 || rows > 64) return 0;
    *n = rows;
    return table;
}

void st_test_mid_table(uint32_t league, StBarred side)
{
    int n, k = 0, pts[64];
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; table && i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (!side(rd32(g_mem, row))) pts[k++] = (int16_t)rd16(g_mem, row + ST_ROW_POINTS);
    }
    if (k < 2) return;
    for (int i = 1; i < k; i++)                  /* the points of the other clubs, high first */
        for (int j = i; j > 0 && pts[j] > pts[j - 1]; j--) {
            int t = pts[j];
            pts[j] = pts[j - 1];
            pts[j - 1] = t;
        }
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        if (side(rd32(g_mem, row))) wr16(g_mem, row + ST_ROW_POINTS, (uint16_t)pts[k / 2]);
    }
}

uint32_t st_call_this(uint32_t fn, uint32_t self, int nargs, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, nargs, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static int up_status(int s) { return s == ST_TITLE || s == ST_UP || s == ST_PLAYOFF; }

void st_mark(uint32_t league, StBarred barred, const char *tag, const char *what, int log)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        if (s == ST_DOWN || s == ST_BARRED || !barred(rd32(g_mem, row))) continue;
        g_mem[row + ST_ROW_STATUS] = ST_BARRED;
        if (log) fprintf(stderr, "recomp: %s: %.51s may not go up from %s\n", tag,
                         win_str(rd32(g_mem, row) + ST_CLUB_NAME), what);
    }
}

void st_shift_up(uint32_t league, StBarred barred, const char *tag, const char *when, int log)
{
    int n;
    uint32_t table = st_table_of(league, &n);
    int any = 0;
    for (int i = 0; i < n && !any; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        any = up_status(g_mem[row + ST_ROW_STATUS]) && barred(rd32(g_mem, row));
    }
    if (!any) return;
    uint8_t places[64];
    int np = 0;
    for (int i = 0; i < n; i++) {
        uint8_t s = g_mem[table + (uint32_t)i * ST_ROW_SIZE + ST_ROW_STATUS];
        if (up_status(s)) places[np++] = s;
    }
    int k = 0;
    for (int i = 0; i < n; i++) {
        uint32_t row = table + (uint32_t)i * ST_ROW_SIZE;
        uint8_t s = g_mem[row + ST_ROW_STATUS];
        if (barred(rd32(g_mem, row))) {
            if (s != ST_DOWN) g_mem[row + ST_ROW_STATUS] = ST_BARRED;
            continue;
        }
        if (s == ST_DOWN) continue;
        uint8_t want = k < np ? places[k] : (up_status(s) ? ST_NONE : s);
        k++;
        if (want != s && log)
            fprintf(stderr, "recomp: %s: %.51s takes status %d in place of a second team %s\n", tag,
                    win_str(rd32(g_mem, row) + ST_CLUB_NAME), want == ST_NONE ? -1 : want, when);
        g_mem[row + ST_ROW_STATUS] = want;
    }
}

/* League::update_positions_and_status 0x684cd0 (vtable +0x94), thiscall, no arguments. */
void o_00684cd0(void);
RT_WRAP(00684cd0, second_teams_status)
{
    uint32_t self = cpu.ecx;
    for (int i = 0; self && i < nhooks; i++)
        if (hooks[i]->before_status) hooks[i]->before_status(self);
    uint32_t level = season_level_ties_before(self);   /* open item 167, season_level_ties_game.c */
    o_00684cd0();
    season_level_ties_after(self, level);
}

/* The promotion chain 0x689c20 (vtable +0xb0), thiscall (pairwise) ret 4. */
void o_00689c20(void);
RT_WRAP(00689c20, second_teams_chain)
{
    uint32_t self = cpu.ecx;
    if (!st_readable(self)) {
        o_00689c20();
        return;
    }
    for (int i = 0; i < nhooks; i++)
        if (hooks[i]->before_chain) hooks[i]->before_chain(self);
    o_00689c20();
    for (int i = 0; i < nhooks && !rt_unwinding; i++)
        if (hooks[i]->after_chain) hooks[i]->after_chain(self);
}
