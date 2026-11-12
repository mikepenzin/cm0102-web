/* The CONCACAF Gold Cup and the Finalissima (0.6.0 slice 28 part b, Q368). Research and evidence:
 * docs/research/2026-10-18-gold-cup-class.md ("class doc" below) and 2026-10-18-nations-league.md (the
 * decisions of part b).
 *
 * The Gold Cup. One object of the class copa_america (goldcup.cpp: ctor 0x5e0660, size 0xf6, vtable 0x96b400)
 * on a new record "CONCACAF Gold Cup" (season_comps.h, `like`), in the international entry 1 as the Copa
 * America (0x830e4a). Nearly every Copa America fact of the class is a direct call or an inline instruction
 * (class doc 1), so this file and small blocks in slice 27's files test the object by its record
 * (season_gold_cup_of: [this + 4], written at 0x5e06b9 before every site of the ctor):
 *   - the years: the sites 0x5e06fb (ctor) and 0x5e1590 (start_new_season) call rt_copa_first_year and
 *     rt_copa_next_year below, which give the Gold Cup its own cycle and pass the Copa America to slice 27;
 *   - the dates: the stage builder wraps of season_intl.c move every date of the builder by the days between the
 *     class's first group day (6 July, 0x5e09a5) and the edition's first day (season_gold_cup_date_shift);
 *   - the entrants: slice 27's wrap of 0x5e1cd0 calls season_gold_cup_entrants for the Gold Cup;
 *   - 16 teams from 2019: slice 27's code of the Copa America of 16 (season_intl_copa.c, copa16) takes the Gold
 *     Cup of 16 with the bracket of the 2024 Copa America (A1-B2, B1-A2, C1-D2, D1-C2; decision);
 *   - a finals competition (squads of 23, no fixture clash): the wrap of 0x76d940 below;
 *   - an international competition (fixture type 1, the FIFA ranking): the wrap of 0x669ae0 in season_nl.c.
 * A vtable copy with no slot changed names the object in the rules report (season_comps_class) and keeps it
 * out of slice 27's vtable tests of the Copa America. The Gold Cups of 2002 and 2003 (4 groups of 3) cannot be
 * played by the class (class doc 5): the first Gold Cup of a game is 2005 or later.
 *
 * The Finalissima. One object of the class Eur_Super_Cup (ctor 0x57a100, size 0xb2, vtable 0x969f80) on a new
 * record "Finalissima", for its one edition of 1 June 2022 (int-continental-224; 2026 cancelled). The picker
 * 0x57a500 (a direct call from the ctor at 0x57a18e and from start_new_season at 0x57a780) takes the winners of
 * the Euro (key 0x9cf7a4) and the Copa America (key 0x9cf7a8) for it, and the round table 0x57a3c0 (vtable +0x3c,
 * called by the ctor at 0x57a1d6 through the class's vtable) gives 1 June, 90 minutes then penalties (rule flags
 * 0x1) and the ground of the first team (venue flags 0; decision: the class has no neutral ground of a nation
 * by record). Its start_new_season (slot 2 of the copy) never makes another edition.
 *
 * Every function here acts only in a season game (season_comps_on) and on these two objects. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_comps.h"
#include "season_nl.h"
#include "season_intl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ENTRY 1                         /* create_other_int_comps 0x830ac0 */

#define COPA_CTOR 0x005e0660u           /* thiscall (year, record), ret 8 */
#define COPA_SIZE 0xf6u                 /* operator new at 0x830ddb */
#define COPA_VTABLE 0x0096b400u
#define COPA_SLOTS 45                   /* League classes (sweep-europe 2.2) */
#define COPA_FIRST_DAY 6                /* the first group day of the class: 6 July (0x5e09a5) */
#define COPA_FIRST_MONTH 7

#define SUPER_CTOR 0x0057a100u          /* Eur_Super_Cup, thiscall (year, record), ret 8 */
#define SUPER_SIZE 0xb2u                /* 0x831584 */
#define SUPER_VTABLE 0x00969f80u
#define SUPER_SLOTS 40                  /* Cup classes */
#define FINALISSIMA_YEAR 2022

#define GAME_NEW 0x00944cf1u            /* operator new, cdecl (size) */
#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define FIND_HOSTS 0x005fa730u          /* HostCountry::find_hosts, thiscall (key, year, &host1, &host2, 1) */

/* The host table (season_intl.c): HostCountry at [0xb5e134], +0 the rows of 0x22 bytes, +4 the count (word). A
 * row: +0 the key id (here the record id), +4 the year (word), +6 three host pairs (nation ids, -1 none), +0x1e
 * the choice (0xfd fixed), +0x1f the decision day, +0x21 the move cycle. */
#define HOSTS 0xb5e134u
#define ROW_SIZE 0x22u
#define ROW_YEAR 0x4u
#define ROW_PAIRS 0x6u
#define ROW_CHOICE 0x1eu
#define ROW_DAY 0x1fu
#define ROW_CYCLE 0x21u
#define CHOICE_FIXED 0xfdu
#define NATION_NAME 0x4u
#define NATION_STADIUM 0x80u            /* the national stadium (0x85c730) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare) */
#define CLUB_COMPARE 0x004b07d0u        /* compare of club pointers (0x5e20f5) */
#define NATIONAL_TEAM 0x0053b3d0u       /* cdecl (nation record) */
#define LATEST_PLACED 0x004afd80u       /* comp_util_get_latest_placed_club, cdecl (record, place) (0x57a5a8) */
#define CUP_FIRST_DATE 0x00521e60u      /* cdecl (rec, round, day, month0, yoff, weekday, year) (0x57a484) */
#define CUP_LAST_DATE 0x00521eb0u       /* cdecl (rec, round, day, month0, yoff, weekday, flag, year, venue) */

#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_CONTINENT 0x71u
#define NATION_REPUTATION 0x8eu
#define CONTINENT_EUROPE 0x9cfa10u
#define CONTINENT_NORTH_AMERICA 0x9cfa14u
#define CONTINENT_SOUTH_AMERICA 0x9cfa1cu
#define RECORDS 0xae23d0u               /* competition records, 0x6b bytes */
#define RECORD_SIZE 0x6bu
#define KEY_EURO 0x9cf7a4u
#define KEY_COPA 0x9cf7a8u

#define F_RECORD 0x4u
#define F_YEAR 0x40u
#define F_TEAMS_OF 0xeeu                /* copa_america: team pointers (0x5e1cda) */
#define F_TEAM_COUNT 0xf2u              /* copa_america: dword (0x5e1cde) */
#define F_CUP_TEAMS 0xa7u               /* Cup: entries of 6 bytes (0x57a51e) */
#define F_CUP_TEAM_COUNT 0x3eu          /* Cup: word (0x57a50c) */

static int logging(void)
{
    static int on = -1;
    if (on < 0) {
        const char *v = getenv("RECOMP_SEASON_NL_LOG");
        on = v ? atoi(v) : 0;
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

static int year_of(uint32_t self) { return (int16_t)rd16(g_mem, self + F_YEAR); }

static int nation_count(void) { return (int)rd32(g_mem, NATION_COUNT); }
static uint32_t nation_at(int id) { return rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE; }

static int in_continent(uint32_t nation, uint32_t key)
{
    uint32_t c = rd32(g_mem, nation + NATION_CONTINENT);
    return readable(c) && rd32(g_mem, c) == rd32(g_mem, key);
}

/* The national team of a nation when its club names the nation (a season database can give an empty row:
 * season_nl_nation_team), else 0. */
static uint32_t team_of(int id) { return season_nl_nation_team(id); }

static int has_team(const uint32_t *teams, int n, uint32_t club)
{
    for (int k = 0; k < n; k++)
        if (teams[k] == club) return 1;
    return 0;
}

/* The national team of the continent with the highest reputation (nation +0x8e) that is not in teams, or 0. */
static uint32_t strongest(uint32_t continent, const uint32_t *teams, int n)
{
    uint32_t best = 0;
    int best_rep = -100000;
    for (int id = 0; id < nation_count(); id++) {
        uint32_t nation = nation_at(id);
        int rep = (int16_t)rd16(g_mem, nation + NATION_REPUTATION);
        if (rep <= best_rep || !in_continent(nation, continent) || !season_nl_nation_can_play(id)) continue;
        uint32_t club = team_of(id);
        if (rt_unwinding) return 0;
        if (!club || has_team(teams, n, club)) continue;
        best = club;
        best_rep = rep;
    }
    return best;
}

/* ---- The Gold Cup ---- */

extern const SeasonCompDef season_gold_cup_comp, season_finalissima_comp;

static uint32_t making_rec;             /* the record of an object whose ctor runs now (not yet in its entry) */
static SeasonNlGoldCup repeat;

static uint32_t gold_record(void)
{
    uint32_t o = season_comps_object(&season_gold_cup_comp);
    return o ? rd32(g_mem, o + F_RECORD) : making_rec;
}

int season_gold_cup_of(uint32_t self)
{
    if (!season_comps_on() || !readable(self)) return 0;
    uint32_t rec = gold_record();
    return rec && rd32(g_mem, self + F_RECORD) == rec;
}

/* The Gold Cup of a year in this game. With changing rules on, the table's edition; after the table's last
 * edition the last one again every two years (Q248). With changing rules off, the first edition from the start
 * season on, again every two years (Q232). NULL for a year with no edition. */
static const SeasonNlGoldCup *gold_edition(int year)
{
    const SeasonNlGoldCup *base = NULL;
    if (season_changing_rules()) {
        for (int i = 0; i < season_nl_gold_cup_count; i++) {
            if (season_nl_gold_cups[i].year == year) return &season_nl_gold_cups[i];
            if (season_nl_gold_cups[i].year < year) base = &season_nl_gold_cups[i];
        }
    } else {
        for (int i = 0; i < season_nl_gold_cup_count && !base; i++)
            if (season_nl_gold_cups[i].year > season_start_year()) base = &season_nl_gold_cups[i];
        if (!base && season_nl_gold_cup_count) base = &season_nl_gold_cups[season_nl_gold_cup_count - 1];
        if (base && base->year == year) return base;
        if (base && base->year > year) base = NULL;
    }
    if (!base || (year - base->year) % 2) return NULL;
    repeat = *base;
    repeat.year = year;
    return &repeat;
}

/* The first Gold Cup after season s's start (in the year s + 1 or later), 0 with none. */
static int gold_first_after(int s)
{
    for (int y = s + 1; y <= s + 40; y++)
        if (gold_edition(y)) return y;
    return 0;
}

int season_gold_cup_teams(uint32_t self)
{
    const SeasonNlGoldCup *g = season_gold_cup_of(self) ? gold_edition(year_of(self)) : NULL;
    return g ? g->teams : 0;
}

/* Sites 0x5e06fb (ctor, after the year is rounded to an odd year) and 0x5e1590 (start_new_season, `add word
 * [esi + 0x40], 2`) of the class (seasons.py). The Copa America keeps slice 27's rules (class index 3). */
uint32_t rt_copa_first_year(uint32_t self, uint32_t year)
{
    if (season_gold_cup_of(self)) return year & 0xffffu;   /* the year that make gives the ctor */
    return rt_intl_first_year(3u, year);
}

uint32_t rt_copa_next_year(uint32_t self, uint32_t year, uint32_t step)
{
    if (!season_gold_cup_of(self)) return rt_intl_next_year(3u, year, step);
    int y = (int16_t)(year & 0xffffu), next = y + 2;
    for (int k = y + 1; k <= y + 40; k++)
        if (gold_edition(k)) {
            next = k;
            break;
        }
    if (logging()) fprintf(stderr, "recomp: season gold cup: next edition %d after %d\n", next, y);
    return (uint32_t)next;
}

/* The day of the year from 0, in plain C: the stage builder calls this while its date shift is set, so it must
 * not call Date::Date 0x549f70, which that shift moves. */
static int day_of_year(int day, int month, int year)
{
    static const int before[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    int leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
    return before[month - 1] + (month > 2 && leap) + day - 1;
}

/* The days between the class's first group day and the edition's first day, for the stage builder and the
 * template of the class (season_intl.c). Returns 1 for the Gold Cup object. */
int season_gold_cup_date_shift(uint32_t self, int *shift)
{
    if (!season_gold_cup_of(self)) return 0;
    const SeasonNlGoldCup *g = gold_edition(year_of(self));
    if (!g) {
        *shift = 0;
        return 1;
    }
    int y = year_of(self);
    int from = day_of_year(COPA_FIRST_DAY, COPA_FIRST_MONTH, y);
    int to = day_of_year(g->start.day, g->start.month, y);
    *shift = to - from;
    return 1;
}

/* The entrants of the Gold Cup (in slice 27's wrap of 0x5e1cd0): the 12 or 16 CONCACAF national teams with the
 * highest reputation, sorted by strength as the original (qsort 0x4b07d0 at 0x5e20fe); group g then takes the
 * teams g, g + G, ... (class doc 4). Decision: no host places (the class's hosts come from the Copa America's
 * key at 0x5e1f55) and no qualifying through the Nations League. Returns 1, or 0 when too few nations. */
static void gold_log_hosts(uint32_t self);

uint32_t season_gold_cup_entrants(uint32_t self)
{
    int want = season_gold_cup_teams(self);
    if (want <= 0) want = 12;
    uint32_t teams[16] = {0};
    int n = 0;
    while (n < want) {
        uint32_t club = strongest(CONTINENT_NORTH_AMERICA, teams, n);
        if (rt_unwinding) return 0;
        if (!club) break;
        teams[n++] = club;
    }
    uint32_t list = game_malloc(4u * (uint32_t)want);
    if (rt_unwinding || !list) return 0;
    for (int k = 0; k < n; k++) wr32(g_mem, list + 4u * (uint32_t)k, teams[k]);
    uint32_t qa[4] = {list, (uint32_t)n, 4, CLUB_COMPARE};
    if (n > 1) call_cdecl(GAME_QSORT, 4, qa);
    if (rt_unwinding) return 0;
    wr32(g_mem, self + F_TEAMS_OF, list);
    wr32(g_mem, self + F_TEAM_COUNT, (uint32_t)want);
    if (logging()) {
        fprintf(stderr, "recomp: season gold cup %d: %d teams:", year_of(self), n);
        for (int k = 0; k < n; k++) fprintf(stderr, " %.50s;", win_str(rd32(g_mem, list + 4u * (uint32_t)k) + 4u));
        fputc('\n', stderr);
    }
    gold_log_hosts(self);
    if (rt_unwinding) return 0;
    if (n != want) {
        fprintf(stderr, "recomp: season gold cup %d: only %d teams of %d\n", year_of(self), n, want);
        return 0;
    }
    return 1;
}

static int nation_by_name(const char *name)
{
    for (int id = 0; id < nation_count(); id++)
        if (!strcmp(win_str(nation_at(id) + NATION_NAME), name)) return id;
    return -1;
}

/* Where the Gold Cup's matches are played. The class's rounds take venue flags 1, 4 and 0x80 (0x5e0c7d, 0x5e0d50,
 * 0x5e0e11, 0x5e0eb5); the venue code asks HostCountry::find_hosts 0x5fa730 with the fixture's record id and year
 * (0x85c5ff..0x85c61c) and plays at the host's stadium (nation +0x80, 0x85c730), else it takes the record's nation
 * (+0x5d, 0x85c709). The Gold Cup record has no host row, so it gets one per edition from its first edition: the
 * first two hosts of the table (the United States in every edition; Canada, Costa Rica as the second), choice
 * fixed, no move, as slice 27 writes the rows of the Copa America (season_intl.c row_write). The rows are in the
 * save with the rest of the host table. */
static void gold_host_rows(uint32_t rec, int from)
{
    uint32_t hc = rd32(g_mem, HOSTS), key = rd32(g_mem, rec);
    if (!readable(hc)) return;
    uint32_t rows = rd32(g_mem, hc);
    int count = (int16_t)rd16(g_mem, hc + 4u);
    if (!readable(rows) || count <= 0 || count > 1000) return;
    int years[40], n = 0;
    for (int y = from; y <= 2031 && n < 40; y++) {
        if (!gold_edition(y)) continue;
        int have = 0;
        for (int i = 0; i < count; i++)
            have |= rd32(g_mem, rows + (uint32_t)i * ROW_SIZE) == key &&
                    (int16_t)rd16(g_mem, rows + (uint32_t)i * ROW_SIZE + ROW_YEAR) == y;
        if (!have) years[n++] = y;
    }
    if (!n) return;
    uint32_t now = game_malloc((uint32_t)(count + n) * ROW_SIZE);
    if (rt_unwinding || !now) return;
    memcpy(g_mem + now, g_mem + rows, (size_t)count * ROW_SIZE);
    uint32_t fa[1] = {rows};
    call_cdecl(GAME_FREE, 1, fa);
    if (rt_unwinding) return;
    for (int k = 0; k < n; k++) {
        const SeasonNlGoldCup *g = gold_edition(years[k]);
        uint32_t row = now + (uint32_t)(count + k) * ROW_SIZE;
        int ids[2] = {-1, -1}, m = 0;
        char host[64];
        for (const char *p = g ? g->hosts : ""; *p && m < 2;) {
            size_t len = strcspn(p, ";");
            snprintf(host, sizeof host, "%.*s", (int)len, p);
            int id = nation_by_name(host);
            if (id >= 0) ids[m++] = id;
            p += len;
            if (*p == ';') p++;
        }
        wr32(g_mem, row, key);
        wr16(g_mem, row + ROW_YEAR, (uint32_t)years[k]);
        for (int j = 0; j < 6; j++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)j, 0xffffffffu);
        wr32(g_mem, row + ROW_PAIRS, (uint32_t)ids[0]);
        wr32(g_mem, row + ROW_PAIRS + 4u, (uint32_t)ids[1]);
        wr8(g_mem, row + ROW_CHOICE, CHOICE_FIXED);
        wr16(g_mem, row + ROW_DAY, (uint32_t)day_of_year(g ? g->start.day : 1, g ? g->start.month : 7, years[k]));
        wr8(g_mem, row + ROW_CYCLE, 0);
    }
    wr32(g_mem, hc, now);
    wr16(g_mem, hc + 4u, (uint32_t)(count + n));
}

/* Test log: the hosts that find_hosts gives the Gold Cup of the object's year, and their stadiums. */
static void gold_log_hosts(uint32_t self)
{
    if (!logging()) return;
    uint32_t hc = rd32(g_mem, HOSTS), out = game_malloc(8);
    if (rt_unwinding || !out || !readable(hc)) return;
    wr32(g_mem, out, 0xffffffffu);
    wr32(g_mem, out + 4u, 0xffffffffu);
    uint32_t fa[5] = {rd32(g_mem, rd32(g_mem, self + F_RECORD)), (uint32_t)year_of(self), out, out + 4u, 1};
    int count = (int8_t)call_this(FIND_HOSTS, hc, 5, fa);
    if (rt_unwinding) return;
    fprintf(stderr, "recomp: season gold cup %d hosts:", year_of(self));
    for (int h = 0; h < count && h < 2; h++) {
        int id = (int32_t)rd32(g_mem, out + 4u * (uint32_t)h);
        if (id < 0 || id >= nation_count()) continue;
        uint32_t st = rd32(g_mem, nation_at(id) + NATION_STADIUM);
        fprintf(stderr, " %.30s (stadium %.40s);", win_str(nation_at(id) + NATION_NAME), readable(st) ? win_str(st + 4u) : "none");
    }
    fputc('\n', stderr);
    uint32_t fr[1] = {out};
    call_cdecl(GAME_FREE, 1, fr);
}

static uint32_t gold_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    int y = loading ? year : gold_first_after(year);
    if (!y) return 0;
    uint32_t v = season_comps_vtable(d, COPA_VTABLE, COPA_SLOTS);
    uint32_t na[1] = {COPA_SIZE};
    uint32_t obj = v ? call_cdecl(GAME_NEW, 1, na) : 0;
    if (rt_unwinding || !readable(obj)) return 0;
    making_rec = rec;
    uint32_t ca[2] = {(uint32_t)y, rec};
    obj = call_this(COPA_CTOR, obj, 2, ca);
    making_rec = 0;
    if (rt_unwinding || !readable(obj)) return 0;
    wr32(g_mem, obj, v);
    if (!loading) gold_host_rows(rec, y);
    if (rt_unwinding) return 0;
    if (logging()) {
        const SeasonNlGoldCup *g = gold_edition(year_of(obj));
        fprintf(stderr, "recomp: season gold cup: %d (%d teams)%s\n", year_of(obj), g ? g->teams : 0,
                loading ? " from the save" : "");
    }
    return obj;
}

/* Part b (Q368): the CONCACAF Gold Cup on a new record with the continent, nation and colour ids of the CONCACAF
 * World Cup qualifying record. Level 17 (inference: below the Euro and the World Cup, 20). `early`: the table
 * gives the Gold Cups of July (2005, 2009, 2013, 2015, 2017, 2021) to the season that starts that summer, after
 * their first match; so the object is made a season before. */
const SeasonCompDef season_gold_cup_comp = {"CONCACAF Gold Cup", "new_concacaf_gold_cup", "CONCACAF Gold Cup", ENTRY, gold_make,
                                            "World Cup CONCACAF Qualifying Section", "Gold Cup", 17, 1};

void o_0076d940(void);

/* national_teams_is_finals_comp, stdcall (record), ret 4 (0x76d940..0x76d99e): 1 for the records of the finals
 * (squads of 23, 0x76d480; no clash of fixtures, 0x5aaae6). The Gold Cup is a finals competition as the Copa
 * America. */
RT_WRAP(0076d940, gold_cup_finals)
{
    uint32_t rec = rd32(g_mem, cpu.esp + 4u);
    if (rec && season_comps_on() && rec == gold_record()) {
        rt_ret_stdcall(1, 1);
        return;
    }
    o_0076d940();
}

/* ---- The Finalissima ---- */

static uint32_t finalissima_record(void)
{
    uint32_t o = season_comps_object(&season_finalissima_comp);
    return o ? rd32(g_mem, o + F_RECORD) : making_rec;
}

static int is_finalissima(uint32_t self)
{
    if (!season_comps_on() || !readable(self)) return 0;
    uint32_t rec = finalissima_record();
    return rec && rd32(g_mem, self + F_RECORD) == rec;
}

/* 1 for a record of the objects of this file (the wrap of 0x669ae0 in season_nl.c). */
int season_gold_cup_record(uint32_t rec)
{
    if (!rec) return 0;
    uint32_t g = season_comps_object(&season_gold_cup_comp), f = season_comps_object(&season_finalissima_comp);
    return (g && rd32(g_mem, g + F_RECORD) == rec) || (f && rd32(g_mem, f + F_RECORD) == rec) ||
           (making_rec && making_rec == rec);
}

/* The winner (place 0) of the latest edition of the competition of a key id (0x57a58e..0x57a5a8), else the
 * strongest national team of the continent (a season database can hold no such history). */
static uint32_t winner(uint32_t key, uint32_t continent, uint32_t other)
{
    uint32_t rec = rd32(g_mem, RECORDS) + rd32(g_mem, key) * RECORD_SIZE;
    uint32_t a[2] = {rec, 0};
    uint32_t club = call_cdecl(LATEST_PLACED, 2, a);
    if (rt_unwinding) return 0;
    if (season_nl_nation_of_club(club) >= 0 && club != other) return club;
    uint32_t have[1] = {other};
    club = strongest(continent, have, other ? 1 : 0);
    if (logging()) fprintf(stderr, "recomp: season finalissima: no winner in the history, %.50s takes the place\n",
                           club ? win_str(club + 4u) : "none");
    return club;
}

void o_0057a500(void);

/* Eur_Super_Cup picker, thiscall (), returns 0 or -1 (0x57a500..0x57a58d): two entries of 6 bytes at +0xa7
 * (club, seed, fate), +0x3e = 2. The Finalissima: the Euro winner, then the Copa America winner. */
RT_WRAP(0057a500, finalissima_teams)
{
    uint32_t self = cpu.ecx;
    if (!is_finalissima(self)) {
        o_0057a500();
        return;
    }
    wr16(g_mem, self + F_CUP_TEAM_COUNT, 2);
    uint32_t list = game_malloc(0xc);
    if (rt_unwinding) return;
    wr32(g_mem, self + F_CUP_TEAMS, list);
    uint32_t euro = winner(KEY_EURO, CONTINENT_EUROPE, 0);
    if (rt_unwinding) return;
    uint32_t copa = winner(KEY_COPA, CONTINENT_SOUTH_AMERICA, euro);
    if (rt_unwinding) return;
    if (list) {
        wr32(g_mem, list, euro);
        wr32(g_mem, list + 6u, copa);
    }
    if (logging())
        fprintf(stderr, "recomp: season finalissima %d: %.50s v %.50s\n", year_of(self), euro ? win_str(euro + 4u) : "none",
                copa ? win_str(copa + 4u) : "none");
    rt_ret_stdcall(list && euro && copa ? 0u : 0xffffffffu, 0);
}

/* Eur_Super_Cup round table (vtable +0x3c), thiscall (stage as a byte, short *rounds, short *code, int
 * *is_league), ret 0x10. Called first by the wrap of 0x57a3c0 in season_uefa_super_cup.c (slice 23), with the
 * original. The original makes one round of 0x68 bytes: draw 4 July (0x57a484), match 24 August with venue
 * flags 0x8000 (0x57a4a2), rule flags 0x83 (0x57a4be). The Finalissima: draw 1 May, match 1 June of +0x40, rule
 * flags 0x1 (penalties after 90 minutes: bit 0x2 is extra time, bit 0x80 the golden goal; sweep-international
 * section 7), venue flags 0. Returns 1 when it handled the call (the Finalissima), else 0 with nothing done. */
int season_finalissima_round_table(void (*original)(void))
{
    uint32_t self = cpu.ecx;
    if (!is_finalissima(self)) return 0;
    original();
    if (rt_unwinding) return 1;
    uint32_t rec = cpu.eax;
    if (!readable(rec)) return 1;
    Cpu after = cpu;
    int year = year_of(self);
    uint32_t f[7] = {rec, 0, 1, 4, 0, 0xffffffffu, (uint32_t)year};
    call_cdecl(CUP_FIRST_DATE, 7, f);
    if (rt_unwinding) return 1;
    /* 1 June 2022 is a Wednesday: flag 2 as the original's midweek round (0x57a492). */
    uint32_t l[9] = {rec, 0, 1, 5, 0, 0xffffffffu, 2, (uint32_t)year, 0};
    call_cdecl(CUP_LAST_DATE, 9, l);
    if (rt_unwinding) return 1;
    wr16(g_mem, rec + 0xdu, 0x1);
    cpu = after;
    return 1;
}

/* vtable +0x8 of the copy: the hooks of the season change and no new edition (the class's start_new_season
 * 0x57a750 steps the year and picks again at 0x57a780). */
static void finalissima_start(void)
{
    uint32_t self = cpu.ecx;
    season_change_start(self, 0);
    if (rt_unwinding) return;
    season_change_start(self, 1);
    if (rt_unwinding) return;
    rt_ret_stdcall(0, 0);
}

static uint32_t finalissima_make(const SeasonCompDef *d, uint32_t rec, int year, int loading)
{
    /* Decision: one edition, 1 June 2022, in both modes: the table holds no later edition (2026 cancelled). */
    if (!loading && year + 1 != FINALISSIMA_YEAR) return 0;
    uint32_t v = season_comps_vtable(d, SUPER_VTABLE, SUPER_SLOTS);
    if (!v) return 0;
    season_comps_set_slot(v, 2, "finalissima_start", finalissima_start);
    uint32_t na[1] = {SUPER_SIZE};
    uint32_t obj = call_cdecl(GAME_NEW, 1, na);
    if (rt_unwinding || !readable(obj)) return 0;
    making_rec = rec;
    uint32_t ca[2] = {loading ? (uint32_t)year : (uint32_t)FINALISSIMA_YEAR, rec};
    obj = call_this(SUPER_CTOR, obj, 2, ca);
    making_rec = 0;
    if (rt_unwinding || !readable(obj)) return 0;
    wr32(g_mem, obj, v);
    /* O322 (A4): the competition kind +0x42 of a match of national sides. The class's ctor writes 2, the kind of
     * a club cup (0x57a167), and the Cup load reads the saved byte back (0x51fc00). PlayerStats::record_match
     * 0x7c0d70 gives a match of kind 4 the national team stats (category 5, 0x7c0f26..0x7c0f2f) and of kind 2
     * the club cup stats (category 4, 0x7c0f34..0x7c0f38), and Club_Records::update_records_for_match 0x444930
     * asks a national side's players for category 5 (0x445f7b..0x445f96), with the box "club_records.cpp 794"
     * (0x44602d) when a player has none. With kind 2 a Finalissima player with no earlier national match had
     * none. Kind 4 is the byte of the other national competitions (copa_america 0x5e06bc, WorldCup 0x92bfab).
     * Written after the ctor, so a new object and a loaded one both have it. */
    wr8(g_mem, obj + 0x42u, 4);
    return obj;
}

/* 1 for the Finalissima object (season_nl_stats.c test log). */
int season_finalissima_object(uint32_t obj)
{
    return is_finalissima(obj);
}

/* Part b (Q368): the Finalissima on a new record with the ids of the Confederations Cup record (a competition of
 * two confederations). Level 18 (inference, as the UEFA Cup). */
const SeasonCompDef season_finalissima_comp = {"Finalissima", "new_finalissima", "Finalissima", ENTRY, finalissima_make,
                                               "Confederations Cup", "Finalissima", 18};
