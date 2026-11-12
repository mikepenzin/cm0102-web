/* International competitions in the original (0.6.0 slice 27): the host table and Australia's move to Asia
 * (season_intl.h). Research and evidence: docs/research/2026-10-18-international.md.
 *
 * Hosts. HostCountry::init_default_table 0x5f9970 fills the host table (global 0xb5e134: +0 the rows, +4 the
 * count as a word) with 57 rows of 0x22 bytes at a new game; the constructor 0x5f97c0 then sorts them by year
 * and date (0x5f995a, comparator 0x5fb3f0). A load reads the rows of the save instead (0x5fb160). Row: +0 the
 * key id of the competition (the value of its key global, as 0x9cf79c), +4 the year (word, 0 = every year),
 * +6/+0xa, +0xe/+0x12, +0x16/+0x1a three host pairs (nation ids, -1 for none), +0x1e the choice (0 to 2 the
 * chosen pair, 0xfd fixed, 0xfe not decided), +0x1f the decision day, +0x21 the years that the row moves on
 * 1 January once it lies in the past (HostCountry::daily_update 0x5fa850, 0x5faafa). find_hosts 0x5fa730 takes
 * the first row of the competition with the year, or with year 0, and returns pair 1 of a fixed row. In a
 * season game the wrap of 0x5f9970 writes the hosts of the table into the row of each edition from the start
 * year (a new row where the original has none): pair 1, choice fixed, no move. The other rows stay.
 *
 * Australia. The confederation of a nation is its continent pointer at nation +0x71 (sweep section 8).
 * Australia joined the AFC on 1 January 2006 (int-world row int-world-49). A season game from 2006/07 starts
 * with Australia in Asia; a game with changing rules on from an earlier season moves it on the first update
 * day from 1 January 2006. The fixed Oceania lists of the original then get New Zealand or another Oceania
 * nation in its place. */
#include "win.h"
#include "season.h"
#include "season_change.h"
#include "season_intl.h"
#include "season_wc48.h"
#include "season_nl.h"                     /* slice 28: the Gold Cup */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define HOSTS 0xb5e134u                 /* HostCountry *: the object has +0 the rows, +4 the count (word);
                                         * find_hosts gets ecx = [0xb5e134] (0x92daa2) */
#define ROW_SIZE 0x22u
#define ROW_COMP 0x0u
#define ROW_YEAR 0x4u
#define ROW_PAIRS 0x6u                  /* six dwords */
#define ROW_CHOICE 0x1eu
#define ROW_DAY 0x1fu
#define ROW_CYCLE 0x21u
#define CHOICE_FIXED 0xfdu
#define DEFAULT_DAY 200                 /* 20 July, the day of most rows of init (0x5f9970) */

#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u          /* continent record; its +0 is the continent id */
#define CONTINENT_ASIA 0x9cfa0cu        /* key continent ids (sweep section 8) */
#define CONTINENT_OCEANIA 0x9cfa18u
#define CONTINENT_EUROPE 0x9cfa10u
#define KEY_AUSTRALIA 0x9cf224u         /* key nation ids: OceaniaNations 0x799f9c, 0x5fab70 */
#define KEY_NEW_ZEALAND 0x9cf3f4u
#define KEY_NEW_CALEDONIA 0x9cf530u     /* left out of the Oceania section at 0x91fc89 */

#define GAME_DATE 0xae2c90u             /* s16 day of the year, s16 year */
#define AUSTRALIA_TO_ASIA 2006          /* 1 January 2006 (int-world-49) */

#define GAME_MALLOC 0x00944e46u         /* malloc, cdecl (size): the rows of init (0x5f98c3) */
#define GAME_FREE 0x009452cau           /* free, cdecl (block) */
#define GAME_QSORT 0x00945333u          /* qsort, cdecl (base, count, size, compare): 0x5f995a */
#define ROW_COMPARE 0x005fb3f0u
#define NATIONAL_TEAM 0x0053b3d0u       /* the national team club of a nation record, cdecl (nation) */

/* The key global of each class with host rows (the comp column of init, read by find_hosts). The Oceania
 * Nations Cup has no host row (it calls no find_hosts). */
static const struct { const char *file; uint32_t key; } host_comps[] = {
    {"world_cup", 0x9cf79cu}, {"euro_champ", 0x9cf7a4u}, {"african_nations", 0x9cf7acu},
    {"olympics", 0x9cf7a0u}, {"goldcup", 0x9cf7a8u}, {"asia_nations", 0x9cf790u}, {"fifa_confed", 0x9cf964u},
};
#define HOST_COMPS ((int)(sizeof host_comps / sizeof host_comps[0]))
#define KEY_CONFED 0x9cf964u
#define CONFED_ANY_YEAR 2200            /* the year that the Confederations Cup row of every year moves to */

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

/* ---- Nations ---- */

static int nation_count(void) { return (int)rd32(g_mem, NATION_COUNT); }
static uint32_t nation_at(int id) { return rd32(g_mem, NATIONS) + (uint32_t)id * NATION_SIZE; }

static int nation_valid(int id) { return id >= 0 && id < nation_count() && readable(rd32(g_mem, NATIONS)); }

/* The id of the first nation with one of the names ("China PR/China"), or -1. */
static int nation_by_names(const char *names)
{
    char one[64];
    const char *p = names;
    while (*p) {
        size_t n = strcspn(p, "/");
        snprintf(one, sizeof one, "%.*s", (int)n, p);
        for (int id = 0; id < nation_count(); id++)
            if (!strcmp(win_str(nation_at(id) + NATION_NAME), one)) return id;
        p += n;
        if (*p == '/') p++;
    }
    return -1;
}

static uint32_t continent_id(int id)
{
    uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
    return readable(c) ? rd32(g_mem, c) : 0xffffffffu;
}

/* The key nation id in a key global, or -1. */
static int key_nation(uint32_t global)
{
    int id = (int)rd32(g_mem, global);
    return nation_valid(id) ? id : -1;
}

/* ---- Australia ---- */

/* 1 when Australia's continent is Asia in the game's data. */
static int australia_in_asia(void)
{
    if (!season_intl_on()) return 0;
    int aus = key_nation(KEY_AUSTRALIA);
    return aus >= 0 && continent_id(aus) == rd32(g_mem, CONTINENT_ASIA);
}

/* Moves Australia from Oceania to Asia: its continent pointer becomes that of an Asian nation. Returns 1 when
 * it moved. The pointer is the data's own, so a save keeps it. */
static int australia_to_asia(void)
{
    int aus = key_nation(KEY_AUSTRALIA);
    if (aus < 0 || continent_id(aus) != rd32(g_mem, CONTINENT_OCEANIA)) return 0;
    uint32_t asia = rd32(g_mem, CONTINENT_ASIA);
    for (int id = 0; id < nation_count(); id++) {
        uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
        if (id != aus && readable(c) && rd32(g_mem, c) == asia) {
            wr32(g_mem, nation_at(aus) + NATION_CONTINENT, c);
            if (logging()) fprintf(stderr, "recomp: season intl: Australia moves to Asia\n");
            return 1;
        }
    }
    return 0;
}

/* The Oceania section leaves New Caledonia out (0x91fc89) and needs 10 entrants (0x91fbda). With Australia in
 * Asia, New Caledonia (a FIFA member from 2004, in the 2010 qualifying) takes its place, so the count stays. */
uint32_t rt_intl_oceania_left_out(uint32_t id)
{
    return australia_in_asia() ? 0xffffffffu : id;
}

/* The Oceania ruling body RbOceania (slot 35 of the Discipline object's 43 rule sets) at a load. The rule-set load
 * 0x55d420 makes it only when a selected nation of the key nation table has the continent Oceania
 * (key_nation_continent_has_selected_nation 0x6699d0 on [0x9cfa18] at 0x55deac, 0x55deb6), and then reads its data;
 * the save 0x555dd0 writes every rule set that exists (0x555f2e..0x555f4a). A game with changing rules on from
 * 2005/06 or before makes the body for a selected Australia, which moves to Asia on 1 January 2006 (intl_entry).
 * A later load then left the body out and read the rest of discipline.dat (the ban array, the nation index) from
 * the wrong place: the box discipline..cpp 1149 and a damaged heap. 1 when this game moved a selected Australia,
 * so the load makes the body that its save holds. The base game never moves Australia. */
uint32_t rt_discipline_oceania_body(void)
{
    if (!season_intl_on() || !season_changing_rules() || season_start_year() >= AUSTRALIA_TO_ASIA) return 0;
    int aus = key_nation(KEY_AUSTRALIA);
    return aus >= 0 && g_mem[nation_at(aus) + 0x11cu] != 0 && australia_in_asia();
}

/* ---- The host table ---- */

static int day_of(int month, int day)
{
    static const int first[12] = {0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334};
    return month >= 1 && month <= 12 && day >= 1 ? first[month - 1] + day - 1 : DEFAULT_DAY;
}

/* Writes the hosts of an edition into a row: pair 1 the first two hosts that the data holds, the other pairs
 * none, the choice fixed and no move. More hosts (three for the 2026 World Cup, six for 2030; slice 29) go into
 * the fields of pairs 2 and 3 (+0xe to +0x1a) in order: find_hosts 0x5fa730 reads pair 1 only for a fixed row,
 * so the original's code never sees them; season_intl_host_ids reads all six. The save keeps every row
 * (0x5fb3ce), so a load keeps them. */
static void row_write(uint32_t row, uint32_t key, const SeasonIntlEdition *e)
{
    int ids[6] = {-1, -1, -1, -1, -1, -1}, n = 0;
    char host[128];
    const char *p = e->hosts;
    while (*p && n < 6) {
        size_t len = strcspn(p, ";");
        snprintf(host, sizeof host, "%.*s", (int)len, p);
        int id = nation_by_names(host);
        if (id >= 0) ids[n++] = id;
        else if (logging()) fprintf(stderr, "recomp: season intl: host %s of %s %d is not in the data\n", host,
                                    e->file, e->year);
        p += len;
        if (*p == ';') p++;
    }
    wr32(g_mem, row + ROW_COMP, key);
    wr16(g_mem, row + ROW_YEAR, (uint16_t)e->year);
    for (int k = 0; k < 6; k++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k, 0xffffffffu);
    for (int k = 0; k < 6; k++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k, (uint32_t)ids[k]);
    wr8(g_mem, row + ROW_CHOICE, CHOICE_FIXED);
    wr8(g_mem, row + ROW_CYCLE, 0);
    if (logging()) {
        fprintf(stderr, "recomp: season intl: host %s %d: ", e->file, e->year);
        for (int k = 0; k < n; k++) fprintf(stderr, "%s%s", k ? ";" : "", win_str(nation_at(ids[k]) + NATION_NAME));
        fprintf(stderr, "%s\n", n ? "" : "-");
    }
}

static uint32_t row_find(uint32_t rows, int count, uint32_t key, int year)
{
    for (int i = 0; i < count; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        if (rd32(g_mem, row + ROW_COMP) == key && (int16_t)rd16(g_mem, row + ROW_YEAR) == year) return row;
    }
    return 0;
}

/* The hosts of a fixed row of the table (slice 29): up to six nation ids in the order of the table (row_write).
 * Gives the count; 0 for no fixed row of that competition and year. */
int season_intl_host_ids(uint32_t key, int year, int ids[6])
{
    uint32_t obj = rd32(g_mem, HOSTS), rows = readable(obj) ? rd32(g_mem, obj) : 0;
    int count = readable(obj) ? (int16_t)rd16(g_mem, obj + 4) : 0, n = 0;
    for (int k = 0; k < 6; k++) ids[k] = -1;
    if (!readable(rows) || count <= 0 || count > 1000) return 0;
    uint32_t row = row_find(rows, count, key, year);
    if (!row || rd8(g_mem, row + ROW_CHOICE) != CHOICE_FIXED) return 0;
    for (int k = 0; k < 6; k++) {
        int id = (int)rd32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k);
        if (nation_valid(id)) ids[n++] = id;
    }
    return n;
}

/* The rows of the editions from the start year. The table grows by the editions that have no row. */
static void hosts_apply(uint32_t self, int from_season, int sort)
{
    uint32_t rows = rd32(g_mem, self);
    int count = (int16_t)rd16(g_mem, self + 4);
    if (!readable(rows) || count <= 0 || count > 1000) return;
    /* The Confederations Cup row of every year (year 0, host redrawn by 0x5fab70) would come first in find_hosts,
     * before the rows of the table: it leaves for a year that no game reaches (2200, below). In its place each
     * year from the start that has no edition in the table gets its own copy, so find_hosts still finds a host
     * there: with changing rules off the original holds the cup every two years after 2017 (fifa_confed..cpp 870,
     * 0x58c7b5, in a 2016 start on 5 January 2019). */
    uint32_t confed = rd32(g_mem, KEY_CONFED), any = 0;
    if (confed != 0xffffffffu) {
        any = row_find(rows, count, confed, 0);
        if (!any) any = row_find(rows, count, confed, CONFED_ANY_YEAR);
    }
    int extra = 0;
    for (int y = from_season; any && y <= SEASON_LAST_YEAR + 4; y++)
        if (!season_intl_edition("fifa_confed", y) && !row_find(rows, count, confed, y)) extra++;
    for (int c = 0; c < HOST_COMPS; c++) {
        uint32_t key = rd32(g_mem, host_comps[c].key);
        if (key == 0xffffffffu) continue;
        for (int i = 0; i < season_intl_edition_count; i++) {
            const SeasonIntlEdition *e = &season_intl_editions[i];
            if (!strcmp(e->file, host_comps[c].file) && e->season >= from_season &&
                !row_find(rows, count, key, e->year))
                extra++;
        }
    }
    if (extra) {
        uint32_t size = (uint32_t)(count + extra) * ROW_SIZE;
        uint32_t args[1] = {size};
        uint32_t now = call_cdecl(GAME_MALLOC, 1, args);
        if (rt_unwinding || !readable(now)) return;
        memcpy(g_mem + now, g_mem + rows, (size_t)count * ROW_SIZE);
        memset(g_mem + now + (uint32_t)count * ROW_SIZE, 0, (size_t)extra * ROW_SIZE);
        uint32_t old[1] = {rows};
        call_cdecl(GAME_FREE, 1, old);
        if (rt_unwinding) return;
        if (any) any = now + (any - rows);
        rows = now;
        wr32(g_mem, self, rows);
    }
    int used = count;
    for (int y = from_season; any && y <= SEASON_LAST_YEAR + 4; y++) {
        if (season_intl_edition("fifa_confed", y) || row_find(rows, used, confed, y)) continue;
        uint32_t row = rows + (uint32_t)used++ * ROW_SIZE;
        memcpy(g_mem + row, g_mem + any, ROW_SIZE);
        wr16(g_mem, row + ROW_YEAR, (uint16_t)y);
        wr8(g_mem, row + ROW_CYCLE, 0);
    }
    for (int c = 0; c < HOST_COMPS; c++) {
        uint32_t key = rd32(g_mem, host_comps[c].key);
        if (key == 0xffffffffu) continue;
        if (key == confed && any && (int16_t)rd16(g_mem, any + ROW_YEAR) == 0)
            wr16(g_mem, any + ROW_YEAR, CONFED_ANY_YEAR);       /* see the top of the function */
        for (int i = 0; i < season_intl_edition_count; i++) {
            const SeasonIntlEdition *e = &season_intl_editions[i];
            if (strcmp(e->file, host_comps[c].file) || e->season < from_season) continue;
            uint32_t row = row_find(rows, used, key, e->year);
            if (!row) {
                row = rows + (uint32_t)used++ * ROW_SIZE;
                wr16(g_mem, row + ROW_DAY, (uint16_t)day_of(e->month, e->day));
            }
            row_write(row, key, e);
        }
    }
    /* A row of the original that lies in the past moves on by its cycle on 1 January (0x5faafa) and takes a
     * random host. A row whose move would land on an edition of the table (the 1999 Copa America row on 2011,
     * the 2010 African Cup of Nations row on 2026) stops where it is, so the row of the table stays the only
     * one of its year. */
    for (int i = 0; i < used; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        int year = (int16_t)rd16(g_mem, row + ROW_YEAR), cycle = rd8(g_mem, row + ROW_CYCLE);
        const char *file = NULL;
        for (int c = 0; c < HOST_COMPS; c++)
            if (rd32(g_mem, host_comps[c].key) == rd32(g_mem, row + ROW_COMP)) file = host_comps[c].file;
        if (!file || year <= 0 || cycle <= 0) continue;
        for (int y = year + cycle; y <= SEASON_LAST_YEAR + 4; y += cycle)
            if (season_intl_edition(file, y)) {
                wr8(g_mem, row + ROW_CYCLE, 0);
                break;
            }
    }
    wr16(g_mem, self + 4, (uint16_t)used);
    if (sort) {                                 /* as the constructor after init (0x5f994a..0x5f995a) */
        uint32_t qa[4] = {rows, (uint32_t)used, ROW_SIZE, ROW_COMPARE};
        call_cdecl(GAME_QSORT, 4, qa);
    }
}

void o_005f9970(void);

/* HostCountry::init_default_table, thiscall (): the rows of a new game. The constructor sorts them after the
 * return (0x5f994a..0x5f995a), with the count of +4. The original's 57 random draws (0x5fa640..0x5fa70b) stay
 * as they are. */
RT_WRAP(005f9970, host_init)
{
    uint32_t self = cpu.ecx;
    o_005f9970();
    if (rt_unwinding || !season_intl_on()) return;
    /* A game that starts in 2001/02 (changing rules on) keeps the original's host table at the new game, and
     * makes no game allocation there (Q152): the rows of the table come at the first season change
     * (intl_season). */
    if (season_start_year() == SEASON_FIRST_YEAR) return;
    hosts_apply(self, season_start_year(), 0);
}

void o_005fab70(void);

/* HostCountry::redraw_world_club_and_confed_hosts, thiscall: it writes a random nation into pair 1 of every
 * Confederations Cup row and swaps the host of the World Cup Oceania section between Australia and New
 * Zealand. In a season game the Confederations Cup rows of the table keep their hosts, and with Australia in
 * Asia the section's host is New Zealand. */
RT_WRAP(005fab70, host_redraw)
{
    uint32_t self = cpu.ecx;
    if (!season_intl_on()) {
        o_005fab70();
        return;
    }
    uint32_t rows = readable(self) ? rd32(g_mem, self) : 0;
    int count = readable(self) ? (int16_t)rd16(g_mem, self + 4) : 0;
    uint32_t key = rd32(g_mem, KEY_CONFED);
    enum { MAX_KEEP = 64 };
    uint32_t keep_at[MAX_KEEP], keep[MAX_KEEP];
    int kept = 0;
    for (int i = 0; readable(rows) && i < count && kept < MAX_KEEP; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        if (rd32(g_mem, row + ROW_COMP) == key && season_intl_edition("fifa_confed", (int16_t)rd16(g_mem, row + ROW_YEAR))) {
            keep_at[kept] = row + ROW_PAIRS;
            keep[kept++] = rd32(g_mem, row + ROW_PAIRS);
        }
    }
    cpu.ecx = self;
    o_005fab70();
    if (rt_unwinding) return;
    for (int k = 0; k < kept; k++) wr32(g_mem, keep_at[k], keep[k]);
    if (australia_in_asia()) {
        uint32_t section = rd32(g_mem, 0x9cf76cu);          /* World Cup Oceania Qualifying Section */
        int nz = key_nation(KEY_NEW_ZEALAND);
        for (int i = 0; nz >= 0 && i < count; i++) {
            uint32_t row = rows + (uint32_t)i * ROW_SIZE;
            if (rd32(g_mem, row + ROW_COMP) == section) wr32(g_mem, row + ROW_PAIRS, (uint32_t)nz);
        }
    }
}

void o_00799e30(void);

/* OceaniaNations::pick_entrants, thiscall (): six teams at +0xee: Australia and New Zealand by key id
 * (0x799f9c, 0x799fc1), then the two best Melanesian and the two best Polynesian nations (0x79a03c,
 * 0x79a04d). With Australia in Asia its place goes to the Oceania nation with the highest reputation (nation
 * +0x8e, inference from docs/research/2026-10-17-season-date-shifts.md) that is not in the six. */
RT_WRAP(00799e30, ofc_entrants)
{
    uint32_t self = cpu.ecx;
    o_00799e30();
    if (rt_unwinding || !cpu.eax || !australia_in_asia()) return;
    uint32_t pool = rd32(g_mem, self + 0xee);
    int aus = key_nation(KEY_AUSTRALIA);
    if (!readable(pool) || aus < 0) return;
    /* A team is found through its own nation (+0x53): see team_nation below. */
    int slot = -1;
    for (int k = 0; k < 6; k++) {
        uint32_t t = rd32(g_mem, pool + 4u * (uint32_t)k);
        if (readable(t) && rd32(g_mem, t + 0x53u) == nation_at(aus)) slot = k;
    }
    if (slot < 0) return;
    uint32_t oceania = rd32(g_mem, CONTINENT_OCEANIA), best = 0;
    int best_rep = -1;
    for (int id = 0; id < nation_count(); id++) {
        if (id == aus || continent_id(id) != oceania) continue;
        int in = 0;
        for (int k = 0; k < 6; k++) {
            uint32_t t = rd32(g_mem, pool + 4u * (uint32_t)k);
            in |= readable(t) && rd32(g_mem, t + 0x53u) == nation_at(id);
        }
        int rep = (int16_t)rd16(g_mem, nation_at(id) + 0x8eu);
        if (in || rep <= best_rep) continue;
        uint32_t args[1] = {nation_at(id)};
        uint32_t t = call_cdecl(NATIONAL_TEAM, 1, args);
        if (rt_unwinding) return;
        if (readable(t) && rd32(g_mem, t + 0x53u) == nation_at(id)) {
            best_rep = rep;
            best = t;
        }
    }
    if (best) wr32(g_mem, pool + 4u * (uint32_t)slot, best);
}

/* ---- Finals that start before their qualifying can end ---- */

#define SLOTS 0x14u                     /* team slots: 6 bytes each, +0 the team, +4 a flag */
#define SLOT_COUNT 0x36u                /* word: the teams in the slots */
#define SLOT_ROOM 0x38u                 /* word: 32 for the World Cup (0x92bffa), 16 for the Euro (0x57bd93) */
#define SLOT_SIZE 6u
#define NATION_REPUTATION 0x8eu         /* s16, 500 to 9500 in the 3.9.68 nation.dat (Brazil 9500) */
#define CONTINENT_EUROPE 0x9cfa10u

/* A game that starts in a later season meets finals whose qualifying began before the start: the sections of
 * a 2005 start play the 2004 campaign from July 2005 and cannot end before the World Cup draw of December 2005,
 * so WorldCup::seed_teams 0x92f230 finds fewer than 32 teams and stops at the box "world_cup.cpp 1438"
 * (0x92f24c, 0x92f2b1). The main build does the same (a run of the main build with September 2005 stops at that
 * box). EuroChamp::run_group_draw 0x57c1d0 of a 2003 start meets empty slots and stops at the box
 * "comp_stats.CPP 1664" (Comp_Stats::set_club_final_position 0x4a2540 with no club, from
 * EuroChamp::set_predicted_positions 0x57c9f0). In a season game the slots that no qualifier filled get the
 * class's default nations still in them (written by the team slot builders 0x92dbd5..0x92dfb3, 0x57bf2f),
 * then the nations with the highest reputation (European nations for the Euro) that are not in yet. */
/* The nation record of a team (+0x53), or 0 when the club has none. national_team 0x53b3d0 gives
 * club[nation id + clubs - 2 * nations] (docs/research/2026-10-17-club-moves-and-names.md), but in a season
 * database that row can be an empty club with no nation: a 2019 start (October 2019) gave an empty row for
 * Denmark while the qualified Denmark team was another club. So a team counts only through its own nation. */
static uint32_t team_nation(uint32_t club)
{
    if (!readable(club)) return 0;
    uint32_t nation = rd32(g_mem, club + 0x53u), base = rd32(g_mem, NATIONS);
    if (!readable(nation) || nation < base || (nation - base) % NATION_SIZE ||
        (int)((nation - base) / NATION_SIZE) >= nation_count()) return 0;
    return nation;
}

static int nation_in(const uint32_t *teams, int n, uint32_t nation)
{
    for (int k = 0; k < n; k++)
        if (teams[k] && team_nation(teams[k]) == nation) return 1;
    return 0;
}

static int dead_name(const char *name);

/* The nation of the first of the names ('/' between them) that the data holds: no case, not a dead record
 * (dead_name), with a continent. -1 for none. */
static int nation_by_alts(const char *names)
{
    char one[64];
    for (const char *p = names; p && *p;) {
        size_t n = strcspn(p, "/");
        snprintf(one, sizeof one, "%.*s", (int)n, p);
        if (!dead_name(one))
            for (int id = 0; id < nation_count(); id++)
                if (!strcasecmp(win_str(nation_at(id) + NATION_NAME), one) &&
                    readable(rd32(g_mem, nation_at(id) + NATION_CONTINENT)))
                    return id;
        p += n;
        if (*p == '/') p++;
    }
    return -1;
}

uint32_t season_intl_team_named(const char *names)
{
    int id = nation_by_alts(names);
    if (id < 0) return 0;
    uint32_t args[1] = {nation_at(id)};
    uint32_t c = call_cdecl(NATIONAL_TEAM, 1, args);
    return !rt_unwinding && team_nation(c) == nation_at(id) ? c : 0;
}

/* 1 when the nation is one of the real draw's (season_intl_real_draw). */
static int in_real_draw(const char *draw, uint32_t nation)
{
    char one[160];
    for (const char *p = draw; p && *p;) {
        size_t n = strcspn(p, ";");
        snprintf(one, sizeof one, "%.*s", (int)n, p);
        int id = nation_by_alts(one);
        if (id >= 0 && nation_at(id) == nation) return 1;
        p += n;
        if (*p == ';') p++;
    }
    return 0;
}

/* A start after the qualifying began (O259 A9): the slots become the real draw. A slot whose nation is in it
 * stays; the other real finalists that the data holds take the other slots in the table's order; a finalist
 * that the data lacks leaves its slot to the fill below (logged). Returns the count of slots set. */
static int real_draw_slots(uint32_t comp, const char *name, int year, uint32_t *teams, int room)
{
    const char *draw = season_intl_real_draw(name, year);
    uint32_t slots = rd32(g_mem, comp + SLOTS);
    if (!draw || !readable(slots)) return 0;
    uint32_t keep[64];
    uint8_t flag[64];
    int n = 0;
    for (int i = 0; i < room; i++) {
        uint32_t t = teams[i], nation = team_nation(t);
        if (nation && in_real_draw(draw, nation) && !nation_in(keep, n, nation)) {
            flag[n] = rd8(g_mem, slots + (uint32_t)i * SLOT_SIZE + 4u);
            keep[n++] = t;
        }
    }
    char one[160];
    for (const char *p = draw; *p && n < room;) {
        size_t len = strcspn(p, ";");
        snprintf(one, sizeof one, "%.*s", (int)len, p);
        p += len;
        if (*p == ';') p++;
        int id = nation_by_alts(one);
        if (id >= 0 && nation_in(keep, n, nation_at(id))) continue;
        uint32_t t = id >= 0 ? season_intl_team_named(one) : 0;
        if (rt_unwinding) return 0;
        if (!t) {
            if (logging()) fprintf(stderr, "recomp: season intl: %s %d: the finalist %s is not in the data\n", name,
                                   year, one);
            continue;
        }
        flag[n] = 0;
        keep[n++] = t;
    }
    for (int i = 0; i < room; i++) {
        teams[i] = i < n ? keep[i] : 0;
        wr32(g_mem, slots + (uint32_t)i * SLOT_SIZE, teams[i]);
        wr8(g_mem, slots + (uint32_t)i * SLOT_SIZE + 4u, i < n ? flag[i] : 0);
    }
    if (logging())
        fprintf(stderr, "recomp: season intl: %s %d: the real draw, %d of %d teams (qualifying began before the "
                        "game)\n", name, year, n, room);
    return n;
}

/* 1 when the finals of an edition year may take teams that no qualifier gave. The fill is for the slots that
 * qualifying cannot fill:
 * - a game that starts in the season of the finals or the one before: the qualifying began before the game
 *   (the 2006 World Cup draw of a 2005 start, Euro 2004 of a 2003 start);
 * - the Nations League path places of every Euro from 2020 (4 in 2020 and 2028, 3 in 2024 with its host), until
 *   slice 28 plays the paths: the qualifying of 10 groups gives only the 20 group places (season_intl_euroq.c,
 *   which keeps that format after 2024). Euro 2028 has no host places (table row int-continental-287), so its
 *   4 places stayed empty, and the squad pick of the finals stopped on an empty team (NationalTeams::select_squad
 *   0x76c120 passes the team's nation to national_team 0x53b3d0 at 0x76c266, which boxes "Database.cpp 13126"
 *   for none: 0x53b3da).
 * Else a full qualifying gives every team. A tie that ends after the World Cup draw gives its slot to its team
 * with the higher reputation, and any other gap is logged as a fault and filled (fill_slots), so the draw never
 * stops at its box. */
static int fill_allowed(const char *name, int year)
{
    if (season_start_year() >= year - 1) return 1;
    /* Slice 29 (Q563): after the table the 2026 shape of qualifying stays, and the hosts of 2030 do not change
     * its places, so a World Cup from 2030 may be short. */
    if (!strcmp(name, "world_cup") && year >= 2030) return 1;
    return !strcmp(name, "euro_champ") && year >= 2020;
}

static void fill_slots(uint32_t comp, uint32_t continent, const char *name)
{
    int year = (int16_t)rd16(g_mem, comp + 0x40u), allowed = fill_allowed(name, year);
    uint32_t slots = rd32(g_mem, comp + SLOTS);
    int count = (int16_t)rd16(g_mem, comp + SLOT_COUNT), room = (int16_t)rd16(g_mem, comp + SLOT_ROOM);
    if (!readable(slots) || count < 0 || room <= 0 || room > 64) return;
    if (count > room) count = room;
    uint32_t teams[64], spare[64];
    int open[64], n_open = 0, n_spare = 0, empty = 0;
    for (int i = 0; i < room; i++) {
        uint32_t t = rd32(g_mem, slots + (uint32_t)i * SLOT_SIZE), nation = team_nation(t);
        teams[i] = 0;
        if (i < count && nation && !nation_in(teams, i, nation)) {
            teams[i] = t;
        } else {
            open[n_open++] = i;
            empty += i < count;
            if (i >= count && nation) spare[n_spare++] = t;         /* a default nation still in the slot */
        }
    }
    if (season_intl_late_start(name, year)) {
        int set = real_draw_slots(comp, name, year, teams, room);
        if (rt_unwinding) return;
        n_open = n_spare = 0;
        for (int i = set; i < room; i++) open[n_open++] = i;
        if (set) wr16(g_mem, comp + SLOT_COUNT, (uint16_t)set);
        count = set;
    }
    if (!n_open) return;
    int filled = 0, s = 0, k = 0;
    if (!allowed) {
        /* A full qualifying whose inter-confederation tie ends after the draw (season_intl_qual.c, note_tie):
         * the slot of the tie goes to the tie's team with the higher reputation, the nearest form (Q247). The
         * tie's winner then stays out (wc_add_qualified). */
        uint32_t ties[16];
        int n_ties = !strcmp(name, "world_cup") ? season_intl_wc_ties(year, ties, 8) : 0;
        for (int i = 0; i < n_ties && k < n_open; i++) {
            uint32_t a = ties[2 * i], b = ties[2 * i + 1], na = team_nation(a), nb = team_nation(b);
            if (!na || !nb || nation_in(teams, room, na) || nation_in(teams, room, nb)) continue;
            uint32_t t = (int16_t)rd16(g_mem, nb + NATION_REPUTATION) > (int16_t)rd16(g_mem, na + NATION_REPUTATION)
                             ? b : a;
            teams[open[k]] = t;
            wr32(g_mem, slots + (uint32_t)open[k] * SLOT_SIZE, t);
            wr8(g_mem, slots + (uint32_t)open[k] * SLOT_SIZE + 4u, 0);
            if (logging())
                fprintf(stderr, "recomp: season intl: %s %d slot %d: %s (the tie %s v %s ends after the draw)\n",
                        name, year, open[k], win_str(team_nation(t) + NATION_NAME), win_str(na + NATION_NAME),
                        win_str(nb + NATION_NAME));
            k++;
            filled++;
        }
        /* Any other gap after a full qualifying is a fault to find (season_intl_run.py checks this line). The
         * original stops at its box (0x92f24c, 0x92f2ac) and then takes clubs at 0x92f2b1; the fill below gives
         * the slot as for a moved start (Q388), so a player meets no box. */
        if (k < n_open && logging()) {
            fprintf(stderr, "recomp: season intl: %s %d: %d qualified teams of %d, short after a full qualifying\n",
                    name, year, count, room);
            for (int i = 0; i < count && i < 64; i++) {
                uint32_t nation = team_nation(rd32(g_mem, slots + (uint32_t)i * SLOT_SIZE));
                fprintf(stderr, "recomp: season intl: %s %d team %d: %s\n", name, year, i,
                        nation ? win_str(nation + NATION_NAME) : "(none)");
            }
        }
    }
    for (; k < n_open; k++) {
        uint32_t t = 0;
        while (s < n_spare && !t) {
            uint32_t c = spare[s++];
            if (!nation_in(teams, room, team_nation(c))) t = c;
        }
        if (!t) {                                        /* the nation with the highest reputation */
            int best_rep = -1;
            for (int id = 0; id < nation_count(); id++) {
                int rep = (int16_t)rd16(g_mem, nation_at(id) + NATION_REPUTATION);
                if (rep <= best_rep || (continent && continent_id(id) != rd32(g_mem, continent)) ||
                    nation_in(teams, room, nation_at(id))) continue;
                uint32_t args[1] = {nation_at(id)};
                uint32_t c = call_cdecl(NATIONAL_TEAM, 1, args);
                if (rt_unwinding) return;
                if (team_nation(c) == nation_at(id)) {
                    best_rep = rep;
                    t = c;
                }
            }
        }
        if (!t) break;
        teams[open[k]] = t;
        if (logging())
            fprintf(stderr, "recomp: season intl: %s %d slot %d: %s\n", name, (int16_t)rd16(g_mem, comp + 0x40u),
                    open[k], win_str(team_nation(t) + NATION_NAME));
        wr32(g_mem, slots + (uint32_t)open[k] * SLOT_SIZE, t);
        wr8(g_mem, slots + (uint32_t)open[k] * SLOT_SIZE + 4u, 0);
        filled++;
    }
    if (filled == n_open) wr16(g_mem, comp + SLOT_COUNT, (uint16_t)room);
    if (logging()) fprintf(stderr, "recomp: season intl: %s %d: %d qualified teams of %d (%d empty), %d filled\n", name,
                           (int16_t)rd16(g_mem, comp + 0x40u), count, room, empty, filled);
}

void o_0092f230(void);

/* WorldCup::seed_teams, thiscall: the draw of the finals. The 2002 World Cup has its own draw (0x92f240). */
RT_WRAP(0092f230, wc_seed_teams)
{
    uint32_t self = cpu.ecx;
    if (season_intl_on() && (int16_t)rd16(g_mem, self + 0x40u) != 2002) fill_slots(self, 0, "world_cup");
    if (rt_unwinding) return;
    cpu.ecx = self;
    /* 48 teams (slice 29, season_wc48.c): no pots here. The pot array +0xee holds 32 entries of 5 bytes and
     * ends at +0x18e, the seeding day, in an object of 0x192 bytes (0x8307b5), so 48 would run past it. The
     * draw wrap sorts the teams itself; the 32 pots that the save writes (0x92f150) are cleared. */
    if (season_wc48(self)) {
        memset(g_mem + self + 0xeeu, 0, 32u * 5u);
        rt_ret_stdcall(cpu.eax, 0);
        return;
    }
    o_0092f230();
}

void o_0057c1d0(void);

/* EuroChamp::run_group_draw, thiscall: the draw of the finals. */
/* EuroChamp seeding 0x57c710 (from update_for_date at the date +0x13e/+0x140, 0x57c66c..0x57c698) copies the
 * team slots into the pot array +0xee before the draw: so the slots are filled there too (season_intl_euro.c,
 * euro_seeding). A slot left empty at the seeding put a club with no nation into a Euro 2020 group, and the
 * game stopped on 30 May 2020 in NationalTeams::select_squad 0x76c120 (box "Database.cpp 13126"). */
void season_intl_euro_fill(uint32_t self)
{
    if (season_intl_on()) season_euro_slots_after_load(self);    /* slice 28: no slot array after a load */
    if (rt_unwinding) return;
    if (season_intl_on()) fill_slots(self, CONTINENT_EUROPE, "euro_champ");
}

RT_WRAP(0057c1d0, euro_group_draw)
{
    uint32_t self = cpu.ecx;
    if (season_intl_on()) season_euro_slots_after_load(self);    /* slice 28 */
    if (rt_unwinding) return;
    if (season_intl_on()) fill_slots(self, CONTINENT_EUROPE, "euro_champ");
    if (rt_unwinding) return;
    cpu.ecx = self;
    o_0057c1d0();
}

void o_0092e740(void);
void o_0057c120(void);

/* A qualifier that ends after the fill finds no free slot: WorldCup::add_qualified_team 0x92e740 and
 * EuroChamp::add_qualified_team 0x57c120 (thiscall (team), ret 4) stop at a box when the slots are full
 * (0x92e74a, 0x57c12a). In a season game the team does not come in. */
/* A start after the qualifying began (O259 A9): a qualifier of the part of the campaign that the game plays goes
 * in only when it is one of the real draw's (the draw fill gives the others, real_draw_slots). */
static int late_out(uint32_t comp, const char *name)
{
    int year = (int16_t)rd16(g_mem, comp + 0x40u);
    uint32_t nation = team_nation(rt_arg(0));
    if (!season_intl_late_start(name, year) || !nation) return 0;
    if (!in_real_draw(season_intl_real_draw(name, year), nation)) {
        if (logging())
            fprintf(stderr, "recomp: season intl: %s %d: %s is not in the real draw, it stays out\n", name, year,
                    win_str(nation + NATION_NAME));
        return 1;
    }
    /* A team of the real play-offs (season_intl_real_tie) enters only from a play-off: the call of add_qualified_team
     * in the Asian tie's status (stage 12, return 0x91270b) or in Oceania's (0x91e93c: its tie, and the tournament of
     * 2026). Iraq, a 2026 play-off winner, came first from the AFC section of a December 2025 start and then won
     * the tournament: "in already" (O259 batch AP). */
    uint32_t ret = rd32(g_mem, cpu.esp);
    if (strcmp(name, "world_cup") || ret == 0x0091270bu || ret == 0x0091e93cu) return 0;
    for (int k = 0; k < 6; k++) {
        const char *tie = season_intl_real_tie(year, k);
        int id = tie ? nation_by_alts(tie) : -1;
        if (id >= 0 && nation_at(id) == nation) {
            if (logging())
                fprintf(stderr, "recomp: season intl: %s %d: %s qualifies through the real play-off, not here\n",
                        name, year, win_str(nation + NATION_NAME));
            return 1;
        }
    }
    return 0;
}

static int slots_full(uint32_t comp)
{
    return season_intl_on() && (int16_t)rd16(g_mem, comp + SLOT_COUNT) >= (int16_t)rd16(g_mem, comp + SLOT_ROOM);
}

RT_WRAP(0092e740, wc_add_qualified)
{
    /* Slice 29: a section can give a nation that is in already as a host, because find_hosts gives a section two
     * hosts (Spain and the three South American hosts of 2030 play their qualifying). Such a qualifier stays out
     * and its place goes to the fill (Q563). */
    if (season_intl_on() && team_nation(rt_arg(0))) {
        uint32_t slots = rd32(g_mem, cpu.ecx + SLOTS), nation = team_nation(rt_arg(0));
        int count = (int16_t)rd16(g_mem, cpu.ecx + SLOT_COUNT), in = 0;
        for (int k = 0; readable(slots) && k < count && k < 64 && !in; k++)
            in = team_nation(rd32(g_mem, slots + (uint32_t)k * SLOT_SIZE)) == nation;
        if (in) {
            if (logging()) fprintf(stderr, "recomp: season intl: world_cup: %s is in already, a qualifier stays out\n",
                                   win_str(nation + NATION_NAME));
            rt_ret_stdcall(0, 1);
            return;
        }
    }
    if (late_out(cpu.ecx, "world_cup")) {
        rt_ret_stdcall(0, 1);
        return;
    }
    if (slots_full(cpu.ecx)) {
        if (logging()) fprintf(stderr, "recomp: season intl: world_cup is full, a qualifier stays out\n");
        rt_ret_stdcall(0, 1);
        return;
    }
    /* Test log of slice 29 (tests/seasons/season_wc48_run.py counts the qualifiers of each confederation): the
     * team and the continent id of its nation (+0x71, sweep section 8). */
    if (logging() && season_intl_on()) {
        static const struct { uint32_t key; const char *name; } conf[] = {
            {0x9cfa10u, "UEFA"}, {0x9cfa1cu, "CONMEBOL"}, {0x9cfa14u, "CONCACAF"}, {0x9cfa08u, "CAF"},
            {0x9cfa0cu, "AFC"}, {0x9cfa18u, "OFC"},
        };
        uint32_t nation = team_nation(rt_arg(0)), c = nation ? rd32(g_mem, nation + NATION_CONTINENT) : 0;
        const char *name = "?";
        for (size_t k = 0; readable(c) && k < sizeof conf / sizeof conf[0]; k++)
            if (rd32(g_mem, c) == rd32(g_mem, conf[k].key)) name = conf[k].name;
        fprintf(stderr, "recomp: season intl: world_cup %d qualifier %s (%s)\n", (int16_t)rd16(g_mem, cpu.ecx + 0x40u),
                nation ? win_str(nation + NATION_NAME) : "none", name);
    }
    o_0092e740();
}

RT_WRAP(0057c120, euro_add_qualified)
{
    uint32_t self = cpu.ecx;
    if (season_intl_on()) season_euro_slots_after_load(self);    /* slice 28: no slot array after a load */
    if (rt_unwinding) return;
    cpu.ecx = self;
    /* A safety net (O260 A12): the cause, the club addresses of the 24-team draw where the group builders take
     * club indexes (0x57b5e4..0x57b5fd), is fixed in season_intl_euro.c. The line is always written, so a run
     * shows any qualifier with no nation. */
    if (season_intl_on() && !team_nation(rt_arg(0))) {
        fprintf(stderr, "recomp: season intl: euro_champ: a qualifier with no nation stays out\n");
        rt_ret_stdcall(0, 1);
        return;
    }
    if (late_out(cpu.ecx, "euro_champ")) {
        rt_ret_stdcall(0, 1);
        return;
    }
    if (slots_full(cpu.ecx)) {
        if (logging()) fprintf(stderr, "recomp: season intl: euro_champ is full, a qualifier stays out\n");
        rt_ret_stdcall(0, 1);
        return;
    }
    o_0057c120();
}

/* EuroChamp build_team_slots 0x57bd80 stops at the box "euro_champ.cpp 837" when find_hosts gives no host
 * (0x57be3d to 0x57becc). Euro 2020 and Euro 2028 have no host place (season_intl_table.c), so in a season game
 * the builder goes on with the default nations (0x57bf2f): seasons.py 0x57be3d. */
uint32_t rt_intl_euro_no_host(void)
{
    return (uint32_t)season_intl_on();
}

/* ---- Finals dates ---- */

/* The stage builder of each finals class (vtable +0x8c, thiscall: the vtables of sweep-international section 2
 * hold 0x92d200, 0x57b630, 0x58bdb0, 0x5e12d0, 0x401ab0, 0x40f4c0, 0x799690, 0x7a0230). It sets the dates of
 * the class's events with Date::Date 0x549f70 (thiscall (day 1 to 31, month from 0, year, flag), ret 0x10;
 * for example the Euro seeding and draw dates at 0x57b6b9 and 0x57b6e4) and calls the template function
 * (vtable +0x3c), whose round dates go through League_Stage 0x68a160 and the Cup round functions 0x521e60 and
 * 0x521eb0, which make Date::Date too. While a builder runs for an edition of the table with a first day, every
 * Date::Date moves by the days between the class's fixed first group day and the table's
 * (season_intl_date_shift). The original's weekday flag (the fourth argument) stays. */
static int date_shift;

/* The event dates that a builder keeps as a day word and a year offset from +0x40 (Date +0 is the day of the
 * year, +2 the year: the World Cup's update_for_date 0x92f1a0 seeds when today's day word equals +0x18e
 * (0x92f1b4) and today's year equals +0x40 plus the s16 at +0x190 (0x92f1b9..0x92f1ce)). Each builder writes
 * both words: the draw day +0xb6 and its offset +0xb8 in all eight (World Cup 0x92d2c9, 0x92d2e2; Euro
 * 0x57b6f0, 0x57b709; Confederations Cup 0x58be51, 0x58be4a; Copa America 0x5e1364, 0x5e137f; African Cup of
 * Nations 0x401b3e, 0x401b59; Asian Cup 0x40f54c, 0x40f567; Oceania Nations Cup 0x79971e, 0x799739; Olympics
 * 0x7a02be, 0x7a02d9), the Euro's seeding day +0x13e and offset +0x140 (0x57b6ca, 0x57b6dd) and the World
 * Cup's +0x18e and +0x190 (0x92d2a1, 0x92d2b4). A shift that moves such a date into another year must move
 * the offset too: the 2022 World Cup's seeding of 15 December 2021 (Date at 0x92d290 with year +0x40 and
 * offset -1) moves by 161 days to 25 May, and with the offset left at -1 the game seeded on 25 May 2021,
 * before its qualifying had played, and stopped at "world_cup.cpp 1438" (0x92f24c). */
static const struct { uint32_t cls, day, offset; } offset_pairs[] = {
    {INTL_CLASSES, 0xb6, 0xb8}, {INTL_EURO, 0x13e, 0x140}, {INTL_WORLD_CUP, 0x18e, 0x190},
};
static struct { uint16_t day; int delta; } moved[16];
static int n_moved, depth, in_builder;

static void fix_offsets(uint32_t cls, uint32_t self)
{
    for (size_t i = 0; i < sizeof offset_pairs / sizeof offset_pairs[0]; i++) {
        if (offset_pairs[i].cls != INTL_CLASSES && offset_pairs[i].cls != cls) continue;
        uint16_t day = rd16(g_mem, self + offset_pairs[i].day);
        for (int k = 0; k < n_moved; k++) {
            if (moved[k].day != day || !moved[k].delta) continue;
            int16_t off = (int16_t)rd16(g_mem, self + offset_pairs[i].offset);
            wr16(g_mem, self + offset_pairs[i].offset, (uint16_t)(off + moved[k].delta));
            if (logging())
                fprintf(stderr, "recomp: season intl: %s %d: the date at +0x%x moves %+d year\n",
                        season_intl_class_file(cls), (int16_t)rd16(g_mem, self + 0x40u), offset_pairs[i].day,
                        moved[k].delta);
            break;
        }
    }
}

static void stage_builder(uint32_t cls, void (*original)(void), int builder)
{
    uint32_t self = cpu.ecx;
    int saved = date_shift;
    date_shift = season_intl_date_shift(cls, (int16_t)rd16(g_mem, self + 0x40u));
    /* Slice 28 part b: the CONCACAF Gold Cup object of the class copa_america has its own dates. */
    if (cls == INTL_COPA) season_gold_cup_date_shift(self, &date_shift);
    if (date_shift && !saved && logging())
        fprintf(stderr, "recomp: season intl: %s %d moves by %d days\n", season_intl_class_file(cls),
                (int16_t)rd16(g_mem, self + 0x40u), date_shift);
    if (!depth++) {                     /* a template inside a builder keeps the builder's record */
        in_builder = builder;
        n_moved = 0;
    }
    original();
    if (!--depth) {
        if (in_builder && date_shift) fix_offsets(cls, self);
        in_builder = 0;
    }
    date_shift = saved;
}

void o_0092d200(void);
void o_0057b630(void);
void o_0058bdb0(void);
void o_005e12d0(void);
void o_00401ab0(void);
void o_0040f4c0(void);
void o_00799690(void);
void o_007a0230(void);
RT_WRAP(0092d200, wc_stages)
{
    uint32_t self = cpu.ecx;
    stage_builder(INTL_WORLD_CUP, o_0092d200, 1);
    if (!rt_unwinding) season_wc48_after_stages(self);     /* slice 29: the seeding and draw of 48 teams */
}
RT_WRAP(0057b630, euro_stages) { stage_builder(INTL_EURO, o_0057b630, 1); }
RT_WRAP(0058bdb0, confed_stages) { stage_builder(INTL_CONFED, o_0058bdb0, 1); }
RT_WRAP(005e12d0, copa_stages) { stage_builder(INTL_COPA, o_005e12d0, 1); }
RT_WRAP(00401ab0, acn_stages) { stage_builder(INTL_ACN, o_00401ab0, 1); }
RT_WRAP(0040f4c0, asian_stages) { stage_builder(INTL_ASIAN, o_0040f4c0, 1); }
RT_WRAP(00799690, ofc_stages) { stage_builder(INTL_OFC, o_00799690, 1); }
RT_WRAP(007a0230, olympics_stages) { stage_builder(INTL_OLYMPICS, o_007a0230, 1); }

/* The template functions (vtable +0x3c: 0x92c290, 0x57ab90, 0x58b560, 0x5e0900, 0x4012e0, 0x40eb00, 0x798b60,
 * 0x79f7d0) also run outside the stage builder: a knockout stage is made when the groups end (for example the
 * Euro's quarter-finals in 0x57bb40 at 0x57bc4b, the group builder 0x57b910). So they set the same shift. */
void o_0092c290(void);
void o_0057ab90(void);
void o_0058b560(void);
void o_005e0900(void);
void o_004012e0(void);
void o_0040eb00(void);
void o_00798b60(void);
void o_0079f7d0(void);
/* The World Cup of 48 teams makes its groups I to L and its knockout of 32 in season_wc48.c (slice 29). */
static void wc_template_body(void) { season_wc48_template(o_0092c290); }
RT_WRAP(0092c290, wc_template) { stage_builder(INTL_WORLD_CUP, wc_template_body, 0); }
/* The Euro of 24 teams makes its groups E and F and its knockout of 16 in season_intl_euro.c. */
static void euro_template_body(void) { season_intl_euro_template(o_0057ab90); }
RT_WRAP(0057ab90, euro_template) { stage_builder(INTL_EURO, euro_template_body, 0); }
RT_WRAP(0058b560, confed_template) { stage_builder(INTL_CONFED, o_0058b560, 0); }
static void copa_template_body(void) { season_intl_copa_template(o_005e0900); }
RT_WRAP(005e0900, copa_template) { stage_builder(INTL_COPA, copa_template_body, 0); }
static void acn_template_body(void) { season_intl_cup_template(INTL_ACN, o_004012e0); }
RT_WRAP(004012e0, acn_template) { stage_builder(INTL_ACN, acn_template_body, 0); }
static void asian_template_body(void) { season_intl_cup_template(INTL_ASIAN, o_0040eb00); }
RT_WRAP(0040eb00, asian_template) { stage_builder(INTL_ASIAN, asian_template_body, 0); }
RT_WRAP(00798b60, ofc_template) { stage_builder(INTL_OFC, o_00798b60, 0); }
RT_WRAP(0079f7d0, olympics_template) { stage_builder(INTL_OLYMPICS, o_0079f7d0, 0); }

void o_00549f70(void);

RT_WRAP(00549f70, date_make)
{
    uint32_t date = cpu.ecx;
    int delta = 0;
    if (date_shift) {
        int day = (int16_t)rd16(g_mem, cpu.esp + 4u), month0 = (int16_t)rd16(g_mem, cpu.esp + 8u);
        int year = (int16_t)rd16(g_mem, cpu.esp + 12u);
        if (day >= 1 && day <= 31 && month0 >= 0 && month0 <= 11 && year > 1900) {
            int was = year;
            season_intl_shift_date(date_shift, &day, &month0, &year);
            delta = year - was;
            wr32(g_mem, cpu.esp + 4u, (uint32_t)day);
            wr32(g_mem, cpu.esp + 8u, (uint32_t)month0);
            wr32(g_mem, cpu.esp + 12u, (uint32_t)year);
        }
    }
    o_00549f70();
    /* A date of the builder's own (not of a nested template) that moved into another year: its day word for
     * fix_offsets. */
    if (delta && depth == 1 && in_builder && n_moved < (int)(sizeof moved / sizeof moved[0])) {
        moved[n_moved].day = rd16(g_mem, date);
        moved[n_moved++].delta = delta;
    }
}

/* ---- Nation names ---- */

/* The renamed nations: season_intl_renames (season_intl_rules.c). */
#define nation_renames season_intl_renames
#define NATION_RENAMES season_intl_rename_count

static int nation_named(const char *name)
{
    for (int id = 0; id < nation_count(); id++)
        if (!strcmp(win_str(nation_at(id) + NATION_NAME), name)) return id;
    return -1;
}

static int held_any_case(const char *name, void *ctx)
{
    (void)ctx;
    for (int id = 0; id < nation_count(); id++)
        if (!strcasecmp(win_str(nation_at(id) + NATION_NAME), name)) return 1;
    return 0;
}

static void put_field(uint32_t at, const char *text, uint32_t room)
{
    memset(g_mem + at, 0, room);
    size_t n = strlen(text);
    if (n > room - 1) n = room - 1;
    memcpy(g_mem + at, text, n);
}

/* Applies every rename up to the season. Returns the count. news: 1 to add a sentence for each. */
static int nations_rename(int season, int news)
{
    int done = 0;
    if (!readable(rd32(g_mem, NATIONS))) return 0;
    for (int i = 0; i < NATION_RENAMES; i++) {
        if (nation_renames[i].season > season) continue;
        int id = nation_named(nation_renames[i].old);
        /* A database with a record of a later name of the chain (Serbia, eSwatini) keeps the old record as it
         * is: it is a dead record beside the live one (dead_records). */
        int later = season_intl_later_held(i, held_any_case, NULL);
        if (id < 0 || later) continue;
        uint32_t rec = nation_at(id);
        put_field(rec + NATION_NAME, nation_renames[i].name, 51);
        put_field(rec + 0x38u, nation_renames[i].short_name, 26);
        put_field(rec + 0x53u, nation_renames[i].code, 4);
        put_field(rec + 0x57u, nation_renames[i].adjective, 26);
        if (logging())
            fprintf(stderr, "recomp: season intl: nation %s is now %s (%s)\n", nation_renames[i].old,
                    nation_renames[i].name, nation_renames[i].row);
        if (news) season_change_news("%s is now called %s.", nation_renames[i].old, nation_renames[i].name);
        done++;
    }
    return done;
}

/* A dead record beside the live one loses its continent: the record of an old name of a rename chain when the
 * data holds a later name (the databases from April 2016 to May 2026 keep "Yugoslavia" with 0 to 2 players and
 * continent Europe beside Serbia). Every pool that takes the nations of a continent (the original's qualifying
 * sections, season_wcq26.c's European pool, season_intl.c's fill) then leaves it out, as the original's loader
 * leaves out a record with no continent (a continent id below 0 is NULL at +0x71, 0x5315a0). Returns the count. */
static int dead_records(void)
{
    int done = 0;
    for (int i = 0; i < NATION_RENAMES; i++) {
        int id = nation_named(nation_renames[i].old);
        if (id < 0 || !season_intl_later_held(i, held_any_case, NULL)) continue;
        uint32_t at = nation_at(id) + NATION_CONTINENT;
        if (!rd32(g_mem, at)) continue;
        wr32(g_mem, at, 0);
        if (logging())
            fprintf(stderr, "recomp: season intl: nation %s has no continent (a record of a later name is the live "
                            "side)\n", nation_renames[i].old);
        done++;
    }
    return done;
}

/* A UEFA member that the data holds with no continent (season_intl_uefa_from: Kosovo in the April 2015, October
 * 2015 and April 2016 databases) gets the continent pointer of a European nation from the season it joined, as
 * Australia moves to Asia. Returns the count. news: 1 to add a sentence. */
static int uefa_members(int season, int news)
{
    uint32_t europe = 0, key = rd32(g_mem, CONTINENT_EUROPE);
    for (int id = 0; id < nation_count() && !europe; id++) {
        uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
        if (readable(c) && rd32(g_mem, c) == key) europe = c;
    }
    if (!europe) return 0;
    int done = 0;
    for (int id = 0; id < nation_count(); id++) {
        const char *name = win_str(nation_at(id) + NATION_NAME);
        int from = season_intl_uefa_from(name);
        if (!from || from > season || readable(rd32(g_mem, nation_at(id) + NATION_CONTINENT))) continue;
        wr32(g_mem, nation_at(id) + NATION_CONTINENT, europe);
        if (logging()) fprintf(stderr, "recomp: season intl: nation %s joins UEFA (%d)\n", name, from);
        if (news) season_change_news("%s now plays in UEFA.", name);
        done++;
    }
    return done;
}

void o_0060d810(void);

/* match_nation_key_id, cdecl (name, index): set_key_ids 0x601a40 finds the key nations by their name at every
 * load of the data (the loop at 0x60421a passes the record +4), so a saved game with a renamed nation would
 * lose its key id (for "Yugoslavia" the assert "Unable to find the YUGOSLAVIA index", 0x606182). In a season
 * game a record with a name of the rename table, while no record holds the first old name of its chain or that
 * record is dead (dead_name: a record of a later name is in the data), is matched by that first name; the dead
 * record matches no key. So in the databases from 2015 the key nation Yugoslavia is Serbia, not the record
 * "Yugoslavia" with no players (O188: the original's default World Cup list of a 2022 start holds the key nation
 * Yugoslavia, 0x92f230 seeds it). The other caller (0x604081) passes names read from a file. */
static int dead_name(const char *name)
{
    for (int i = 0; i < NATION_RENAMES; i++)
        if (!strcmp(nation_renames[i].old, name)) return season_intl_later_held(i, held_any_case, NULL);
    return 0;
}

RT_WRAP(0060d810, match_nation_key)
{
    uint32_t name = rt_arg(0);
    if (!season_intl_on() || !readable(name)) {
        o_0060d810();
        return;
    }
    const char *now = win_str(name), *first = NULL;
    if (dead_name(now)) {
        rt_ret_cdecl(0);
        return;
    }
    for (int step = 0; step < NATION_RENAMES; step++) {
        const char *older = NULL;
        for (int i = 0; i < NATION_RENAMES; i++)
            if (!strcasecmp(nation_renames[i].name, first ? first : now)) older = nation_renames[i].old;
        if (!older) break;
        first = older;
    }
    if (!first || (nation_named(first) >= 0 && !dead_name(first))) {
        o_0060d810();
        return;
    }
    static uint32_t buf;
    if (!buf) buf = win_static_alloc(64);
    win_put_str(buf, first, 64);
    uint32_t slot = cpu.esp + 4;
    wr32(g_mem, slot, buf);
    o_0060d810();
    if (!rt_unwinding) wr32(g_mem, slot, name);
}

/* ---- The rule kind ---- */

/* A new game (season.c, before the original makes it): a season game from 2006/07 starts with Australia in
 * Asia, before the competitions take their entrants; the renames, dead records and UEFA members of the start
 * season too. */
void season_intl_at_create(void)
{
    if (!season_intl_on()) return;
    /* From a start in 2006/07, or in 2005/06 with changing rules off: the rule of the 2005/06 row
     * (int-world-49, AFC member from 1 January 2006) is the start season's, so it holds for the whole game
     * (Q232). With changing rules on a 2005/06 game moves on 1 January 2006 (intl_entry). */
    if (season_start_year() >= AUSTRALIA_TO_ASIA ||
        (season_start_year() == AUSTRALIA_TO_ASIA - 1 && !season_changing_rules()))
        australia_to_asia();
    nations_rename(season_start_year(), 0);
    dead_records();
    uefa_members(season_start_year(), 0);
}

/* The host news of a World Cup with more than two hosts (slice 29: 2026 and 2030), in the season of its table
 * row: a fixed host row gets no news of daily_update 0x5fa850 (post_host_news 0x5faec0 posts only for rows
 * that it opens or decides). The names are those of the data (season_intl_host_ids). */
static void host_news(int season)
{
    for (int i = 0; i < season_intl_edition_count; i++) {
        const SeasonIntlEdition *e = &season_intl_editions[i];
        int ids[6], n;
        if (e->season != season || strcmp(e->file, "world_cup")) continue;
        n = season_intl_host_ids(rd32(g_mem, host_comps[0].key), e->year, ids);
        if (n < 3) continue;
        char text[400];
        size_t len = 0;
        for (int k = 0; k < n && len < sizeof text; k++)
            len += (size_t)snprintf(text + len, sizeof text - len, "%s%s", k == 0 ? "" : k == n - 1 ? " and " : ", ",
                                    win_str(nation_at(ids[k]) + NATION_NAME));
        season_change_news("The %d World Cup plays in %s, its %d host nations.", e->year, text, n);
    }
}

/* The season point of a later season, with changing rules on: the renames of that season, with news. */
static void intl_season(int season)
{
    if (season_change_is_start()) return;
    nations_rename(season, 1);
    uefa_members(season, 1);
    host_news(season);
    /* The host rows of the table in a game that started in 2001/02: at its first season change, for the
     * editions from 2002/03 (host_init). The table is sorted as the constructor sorts it. */
    if (season_start_year() == SEASON_FIRST_YEAR && season == SEASON_FIRST_YEAR + 1) {
        uint32_t obj = rd32(g_mem, HOSTS);
        if (readable(obj)) hosts_apply(obj, season, 1);
    }
}

/* Each league table entry on its update day, with changing rules on: Australia moves on the first one from
 * 1 January 2006. */
static void intl_entry(const SeasonChange *c)
{
    (void)c;
    if ((int16_t)rd16(g_mem, GAME_DATE + 2) >= AUSTRALIA_TO_ASIA && australia_to_asia())
        season_change_news("Australia now plays in the Asian Football Confederation.");
}

const SeasonRuleKind season_kind_intl = {"international", NULL, NULL, intl_season, intl_entry, NULL, NULL};
