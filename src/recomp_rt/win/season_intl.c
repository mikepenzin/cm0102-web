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
#include "season_rules.h"
#include "season_wc48.h"
#include "season_nl.h"                     /* slice 28: the Gold Cup */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define HOSTS 0xb5e134u                 /* HostCountry *: the object has +0 the rows, +4 the count (word);
                                         * find_hosts gets ecx = [0xb5e134] (0x92daa2) */
#define ROW_SIZE SEASON_INTL_ROW_SIZE
#define ROW_COMP 0x0u
#define ROW_YEAR 0x4u
#define ROW_PAIRS SEASON_INTL_ROW_PAIRS   /* six dwords */
#define ROW_CHOICE SEASON_INTL_ROW_CHOICE
#define ROW_DAY 0x1fu
#define ROW_CYCLE 0x21u
#define CHOICE_FIXED SEASON_INTL_CHOICE_FIXED
#define DEFAULT_DAY 200                 /* 20 July, the day of most rows of init (0x5f9970) */

#define NATIONS 0xae23a8u               /* nation records, 0x122 bytes */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_NAME 0x4u
#define NATION_CONTINENT 0x71u          /* continent record; its +0 is the continent id */
#define NATION_SHORT 0x38u              /* short name, 26 bytes */
#define CLUBS 0xae23b4u                 /* club records, 0x245 bytes (0x53b4c2..0x53b4e0) */
#define CLUB_COUNT 0xae235cu
#define CLUB_SIZE 0x245u
#define CLUB_NAME_AT 0x4u               /* long name, 51 bytes */
#define CLUB_SHORT_AT 0x38u             /* short name, 26 bytes */
#define CONTINENT_ASIA 0x9cfa0cu        /* key continent ids (sweep section 8) */
#define CONTINENT_OCEANIA 0x9cfa18u
#define CONTINENT_EUROPE 0x9cfa10u
#define CONTINENT_NORTH_AMERICA 0x9cfa14u   /* the CONCACAF key that WcConcacafCup::select_entrants 0x916250 compares */
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
#define GAME_RANDOM 0x0090cfc0u         /* utils_random, cdecl (n): 0 to n - 1 (the draw of 0x5faaa0) */
#define DAY_NEVER 0x7fffu               /* a decision day that daily_update never meets (it compares the word
                                         * +0x1f with the day of the year, 0x5faa1f, 0x5faa75) */
#define KEY_OFC 0x9cf8a0u               /* the key of the OFC Nations Cup (no host row: it calls no find_hosts) */
#define KEY_EURO 0x9cf7a4u
#define COMP_OBJECTS 0xadadfcu       /* the competition objects by key id (read at 0x777b29) */
#define CHOICE_OPEN 0xfeu               /* not decided: drawn 5 years before (0x5faa9b..0x5faaae) */

/* The key global of each class with host rows (the comp column of init, read by find_hosts). The Oceania
 * Nations Cup has no host row (it calls no find_hosts). */
static const struct { const char *file; uint32_t key; } host_comps[] = {
    {"world_cup", 0x9cf79cu}, {"euro_champ", 0x9cf7a4u}, {"african_nations", 0x9cf7acu},
    {"olympics", 0x9cf7a0u}, {"goldcup", 0x9cf7a8u}, {"asia_nations", 0x9cf790u}, {"fifa_confed", 0x9cf964u},
};
#define HOST_COMPS ((int)(sizeof host_comps / sizeof host_comps[0]))
#define KEY_CONFED 0x9cf964u
/* The last year of the host rows that hosts_apply writes and checks: 4 years after the last rules row, as up to
 * 0.6.1. The hosts after the table are slice 5 of 0.6.2. */
#define HOST_ROWS_LAST (SEASON_RULE_LAST + 4)
/* The year that the Confederations Cup row of every year moves to: the never year of the ended class
 * (season_intl_rules.c NEVER_YEAR, 0.6.2 open item 13). Up to 0.6.1 it was 2200, which a game to 2300 reaches.
 * hosts_apply runs only at a new game and at the first season change of a 2001 game, so a row that a save of 0.6.1
 * holds at 2200 stays there (slice 4b). */
#define CONFED_ANY_YEAR 2999

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

/* The Oceania entry (open item 82). Its creator (key_nation_create_all_comps 0x669180 calls it for each entry at a
 * new game and at a load; tools/recomp/oceania_entry.py) makes the OFC Champions League (OceaniaClubChamp, constructor 0x796fd0 at 0x83207e) only
 * when a selected nation of the key nation table has the continent Oceania (0x6699d0 on entry +4 at 0x831f7a, je
 * 0x8320aa at 0x831f84). The save 0x6692f0 writes every object of every entry (vtable +0x1c, 0x669314). A game made
 * from 2005/06 or before with Australia selected makes the object, and Australia moves to Asia on 1 January 2006: a
 * later load made no object, so the OFC Champions League stopped after the season of the first load (2011 or 2012 in
 * the run set), and every save after packed the old part file again. 1 in that game, so the load makes the object
 * whose part its save holds. The same test as the Oceania ruling body above; the base game never moves Australia. */
uint32_t rt_oceania_entry_kept(void)
{
    uint32_t kept = rt_discipline_oceania_body();
    if (kept) fprintf(stderr, "recomp: season intl: the Oceania entry keeps its OFC Champions League (Australia moved)\n");
    return kept;
}

/* The Asian entry's creator has the same test (0x6699d0 on entry +4 at 0x831c6b, je 0x831e5b at 0x831c75). In that
 * game with no Asian nation selected but Australia, the new game made no Asian object, and a load after the move made
 * them with no part in the save (new editions, so a game with a load played other competitions than one without).
 * 1 there, so the load keeps the entry as the new game made it. The walk of 0x6699d0: the key nation entries of 0x48
 * bytes from 0xb63d60 to 0xb64738, entry +0 the nation, nation +0x71 the continent, +0x11c selected. */
uint32_t rt_asia_entry_left_out(uint32_t continent)
{
    if (!rt_discipline_oceania_body()) return 0;
    int aus = key_nation(KEY_AUSTRALIA);
    uint32_t australia = aus >= 0 ? nation_at(aus) : 0;
    for (uint32_t e = 0xb63d60u; e < 0xb64738u; e += 0x48u) {
        uint32_t nation = rd32(g_mem, e);
        if (nation && nation != australia && rd32(g_mem, nation + 0x71u) == continent && g_mem[nation + 0x11cu])
            return 0;
    }
    return 1;
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
    /* Open item 40: daily_update puts every row of the year 5 years ahead on its decision day on the news list,
     * fixed rows too (0x5faa7f), and the news of a row with no host has no text (NewsItem::write_text 0x77bc80
     * skips the body for host -1, 0x77cb91). A row with no host (the Euros of 2020 and 2028) gets no such day. */
    if (!n) wr16(g_mem, row + ROW_DAY, DAY_NEVER);
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
    int choice = row ? rd8(g_mem, row + ROW_CHOICE) : -1;
    if (choice != CHOICE_FIXED && (choice < 0 || choice > 2)) return 0;
    int from = choice == CHOICE_FIXED ? 0 : 2 * choice, to = choice == CHOICE_FIXED ? 6 : from + 2;
    for (int k = from; k < to; k++) {
        int id = (int)rd32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k);
        if (nation_valid(id)) ids[n++] = id;
    }
    return n;
}

/* 0.6.2 item 181: the edition of the table of a class and year that a 2001 game plays with the original's host row
 * (its season 2001/02: the rows of the table come at the first season change, host_init), with changing rules on;
 * else NULL. Its hosts that the data holds go to ids (at most 6), their count to *n. */
static const SeasonIntlEdition *first_season_edition(const char *file, int year, int32_t ids[6], int *n)
{
    const SeasonIntlEdition *e = file ? season_intl_table_edition(file, year) : NULL;
    *n = 0;
    if (!e || e->season != SEASON_FIRST_YEAR || !season_intl_on() || season_start_year() != SEASON_FIRST_YEAR)
        return NULL;
    char one[128];
    for (const char *p = e->hosts; *p && *n < 6;) {
        size_t len = strcspn(p, ";");
        snprintf(one, sizeof one, "%.*s", (int)len, p);
        int id = nation_by_names(one);
        if (id >= 0) ids[(*n)++] = id;
        p += len;
        if (*p == ';') p++;
    }
    return e;
}

/* 0.6.2 item 181: at the draw of an edition of 2001/02 in a 2001 game with changing rules on, its host row takes
 * the hosts of the table when the original's row names others (season_intl_row_draw_hosts). Of the editions of
 * 2001/02 only the Africa Cup of Nations 2002 differs: the original names Egypt (0x5f9f19), the table Mali. The
 * World Cup 2002 row names South Korea and Japan (0x5f9a21, 0x5f9a2c, pair 1 chosen at 0x5f9a1b) and the Copa
 * America 2001 row Colombia (0x5fa1d3), as the table does. The new game stays the base game (Q152); the row
 * changes first at the draw, so the entrants, the venues (0x85c61c) and the history host (rt_intl_history_host)
 * all take the table's host. */
void season_intl_draw_hosts(const char *file, int year)
{
    int32_t ids[6];
    int n, count;
    if (!first_season_edition(file, year, ids, &n)) return;
    uint32_t key = 0xffffffffu, obj = rd32(g_mem, HOSTS), rows = readable(obj) ? rd32(g_mem, obj) : 0;
    for (int c = 0; c < HOST_COMPS; c++)
        if (!strcmp(host_comps[c].file, file)) key = rd32(g_mem, host_comps[c].key);
    count = readable(obj) ? (int16_t)rd16(g_mem, obj + 4) : 0;
    if (key == 0xffffffffu || !readable(rows) || count <= 0 || count > 1000) return;
    uint32_t row = row_find(rows, count, key, year);
    if (!row || !season_intl_row_draw_hosts(g_mem + row, ids, n)) return;
    if (logging()) fprintf(stderr, "recomp: season intl: host row %s %d at the draw: %s, the table's host\n", file, year,
                           win_str(nation_at(ids[0]) + NATION_NAME));
}

/* ---- The host rows after the table (0.6.2 slice 5, decisions Q4, Q5, Q6, Q32) ---- */

/* A nation that no longer exists has no continent (the data load links an id of -1 as null; the old record of a
 * renamed nation, dead_records): original_faults.c rt_national_vacancy. */
static int nation_alive(int id) { return nation_valid(id) && readable(rd32(g_mem, nation_at(id) + NATION_CONTINENT)); }

/* Q32: a host of the rotation must still exist, and the OFC Nations Cup leaves out the nations that left Oceania
 * (Australia, its 2004 host, in the AFC from 2006). */
static int host_alive(const char *file, const char *names)
{
    int id = nation_by_names(names);
    if (!nation_alive(id)) return 0;
    return strcmp(file, "oceania_nations") || continent_id(id) == rd32(g_mem, CONTINENT_OCEANIA);
}

static const char *file_of_key(uint32_t key)
{
    for (int c = 0; c < HOST_COMPS; c++)
        if (rd32(g_mem, host_comps[c].key) == key) return host_comps[c].file;
    return NULL;
}

/* The last table edition year of a class, or 0. */
static int table_last_year(const char *file)
{
    int last = 0;
    for (int i = 0; i < season_intl_edition_count; i++)
        if (!strcmp(season_intl_editions[i].file, file) && season_intl_editions[i].year > last)
            last = season_intl_editions[i].year;
    return last;
}

/* Q6: a pair is valid when it names a nation and every nation it names still exists. */
static int pair_valid(uint32_t row, int k)
{
    int a = (int)rd32(g_mem, row + ROW_PAIRS + 8u * (uint32_t)k), b = (int)rd32(g_mem, row + ROW_PAIRS + 8u * (uint32_t)k + 4u);
    if (a == -1 && b == -1) return 0;
    return (a == -1 || nation_alive(a)) && (b == -1 || nation_alive(b));
}

/* Q32 for one row: the rotation host of its class and year as a fixed row (all three pairs name nations that no
 * longer exist). 0 when the class has no live host in its table. */
static int row_rotate(uint32_t row)
{
    const char *file = file_of_key(rd32(g_mem, row + ROW_COMP));
    int year = (int16_t)rd16(g_mem, row + ROW_YEAR);
    const char *host = file ? season_intl_rotation_host(file, season_intl_rotation_step(file, year), host_alive) : NULL;
    int id = host ? nation_by_names(host) : -1;
    if (id < 0) return 0;
    for (int k = 0; k < 6; k++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k, 0xffffffffu);
    wr32(g_mem, row + ROW_PAIRS, (uint32_t)id);
    if (logging()) fprintf(stderr, "recomp: season intl: host %s %d: no pair of live nations, %s (Q32)\n", file, year, host);
    return 1;
}

/* The pair of a drawn row: the original's draw r, else (Q6) a pair drawn again from the valid others, else (Q32)
 * the rotation host, fixed. Gives the choice byte. */
static uint32_t row_pick(uint32_t row, uint32_t r)
{
    int valid[3];
    for (int k = 0; k < 3; k++) valid[k] = pair_valid(row, k);
    int n = 0;
    for (int k = 0; k < 3; k++) n += valid[k] && k != (int)r;
    int redraw = 0;
    if (!(r < 3 && valid[r]) && n > 1) {
        uint32_t a[1] = {(uint32_t)n};
        redraw = (int)call_cdecl(GAME_RANDOM, 1, a);
        if (rt_unwinding) return r;
    }
    int k = season_intl_pick_pair(valid, (int)r, redraw);
    if (k >= 0) {
        if (logging() && k != (int)r)
            fprintf(stderr, "recomp: season intl: host %s %d: pair %u names a nation that no longer exists, pair %d (Q6)\n",
                    file_of_key(rd32(g_mem, row + ROW_COMP)), (int16_t)rd16(g_mem, row + ROW_YEAR), r + 1, k + 1);
        return (uint32_t)k;
    }
    return row_rotate(row) ? CHOICE_FIXED : r;
}

/* HostCountry::daily_update 0x5fa850, at 0x5faaae: the draw of a row 5 years before its finals stores random(3)
 * (0x5faaa0) as the choice. In a season game a pair that names a nation that no longer exists is drawn again
 * (Q6, Q32). The news of the draw (post_host_news 0x5faec0) reads the choice afterwards. */
uint32_t rt_intl_host_pick(uint32_t row, uint32_t r)
{
    if (!season_intl_on() || !readable(row)) return r;
    return row_pick(row, r);
}

typedef struct {
    uint32_t key;
    int year, cycle, choice, day;
    int pairs[6];
    const char *hosts;          /* a fixed row of these hosts (announced, rotation), else the pairs */
    const char *file;
} PlannedRow;

#define MAX_PLANNED 96

/* The rows after the table of each class from the season `from` (Q4, Q5, Q32): the announced editions, fixed;
 * for the World Cup, Euro, Olympics and Africa Cup of Nations a copy of each of the original's rows (`taken`, by
 * slot) on the first edition after the table that takes its slot, moving on as the original's row does; for the
 * Copa America (the file goldcup, key 0x9cf7a8) and the Asian Cup one fixed row for each live host of the table,
 * in order, moving on by all of them.
 * The original's daily_update moves each row on 1 January once past (0x5faafa), and draws the host of a row that
 * is not fixed 5 years before (0x5faaa0), so the rows hold for every later year. */
static int plan_rows(int from, const uint8_t taken[HOST_COMPS][8][ROW_SIZE], const int have[HOST_COMPS][8],
                     PlannedRow *plan)
{
    int n = 0;
    for (int c = 0; c < HOST_COMPS; c++) {
        const char *file = host_comps[c].file;
        uint32_t key = rd32(g_mem, host_comps[c].key);
        int last = table_last_year(file);
        const SeasonIntlEdition *e = last ? season_intl_next(file, last) : NULL;
        if (key == 0xffffffffu || !e || key == rd32(g_mem, KEY_CONFED)) continue;
        int cycle = e->year - last, start = last + cycle;
        while (start < from) start += cycle;
        /* Q5: the announced editions. */
        for (int y = last + cycle; y <= last + 8 * cycle && n < MAX_PLANNED; y += cycle) {
            const char *a = season_intl_announced_hosts(file, y);
            if (a && y >= from) plan[n++] = (PlannedRow){key, y, 0, CHOICE_FIXED, DEFAULT_DAY, {-1, -1, -1, -1, -1, -1}, a, file};
        }
        int base, step, rows;
        if (season_intl_host_slots(file, &base, &step, &rows)) {
            int roll = rows * step;
            for (int k = 0; k < rows && n < MAX_PLANNED; k++) {
                if (!have[c][k]) continue;
                int y = start, tries = 0;
                while ((season_intl_host_slot(file, y) != k || season_intl_announced_hosts(file, y)) && tries++ < 4 * rows)
                    y += cycle;
                if (tries >= 4 * rows) continue;            /* the cycle never reaches this slot */
                const uint8_t *t = taken[c][k];
                PlannedRow r = {key, y, roll, t[ROW_CHOICE] == CHOICE_FIXED ? CHOICE_FIXED : CHOICE_OPEN,
                                (int)(t[ROW_DAY] | t[ROW_DAY + 1] << 8), {0}, NULL, file};
                for (int j = 0; j < 6; j++)
                    r.pairs[j] = (int)(t[ROW_PAIRS + 4 * j] | t[ROW_PAIRS + 4 * j + 1] << 8 |
                                       t[ROW_PAIRS + 4 * j + 2] << 16 | (uint32_t)t[ROW_PAIRS + 4 * j + 3] << 24);
                plan[n++] = r;
            }
        } else {
            int hosts = season_intl_rotation_count(file, host_alive);
            for (int k = 0; k < hosts && n < MAX_PLANNED; k++) {
                int y = last + cycle * (k + 1);
                while (y < from) y += cycle * hosts;
                static char keep[MAX_PLANNED][64];
                snprintf(keep[n], sizeof keep[0], "%s", season_intl_rotation_host(file, k, host_alive));
                plan[n] = (PlannedRow){key, y, cycle * hosts <= 127 ? cycle * hosts : 0, CHOICE_FIXED, DEFAULT_DAY,
                                       {-1, -1, -1, -1, -1, -1}, keep[n], file};
                n++;
            }
        }
    }
    return n;
}

static void plan_write(uint32_t row, const PlannedRow *p, int from)
{
    if (p->hosts) {
        SeasonIntlEdition e = {p->file, 0, p->year, 0, 0, 0, 0, 0, p->hosts, "after-table"};
        row_write(row, p->key, &e);
        if ((int)rd32(g_mem, row + ROW_PAIRS) != -1) wr16(g_mem, row + ROW_DAY, (uint16_t)p->day);   /* item 40 */
    } else {
        wr32(g_mem, row + ROW_COMP, p->key);
        wr16(g_mem, row + ROW_YEAR, (uint16_t)p->year);
        for (int k = 0; k < 6; k++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k, (uint32_t)p->pairs[k]);
        wr8(g_mem, row + ROW_CHOICE, (uint8_t)p->choice);
        wr16(g_mem, row + ROW_DAY, (uint16_t)p->day);
        /* A fixed row of the original (the Africa Cup of Nations, the Olympics up to 2028): find_hosts reads its
         * pair 1 only (0x5fa730), while season_intl_host_ids reads all six fields of a fixed row (more hosts of the
         * table, slice 29), so the original's other pairs go (they gave Afghanistan as a third ACN host). A new game
         * has cleared them already (hosts_unset_clean); an older save may still hold them. A host that
         * no longer exists takes the rotation host (Q6, Q32). */
        if (p->choice == CHOICE_FIXED) {
            for (int k = 2; k < 6; k++) wr32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k, 0xffffffffu);
            if (!pair_valid(row, 0)) row_rotate(row);
        }
        /* A row whose draw day lies before the start (a date jump) is drawn now, as daily_update would have. */
        if (p->choice == CHOICE_OPEN && p->year - 5 <= from) {
            uint32_t a[1] = {3};
            uint32_t r = call_cdecl(GAME_RANDOM, 1, a);
            if (rt_unwinding) return;
            wr8(g_mem, row + ROW_CHOICE, (uint8_t)row_pick(row, r));
        }
        if (logging()) fprintf(stderr, "recomp: season intl: host row %s %d: the original's row, %s\n", p->file, p->year,
                               rd8(g_mem, row + ROW_CHOICE) == CHOICE_OPEN ? "drawn 5 years before" : "drawn");
    }
    wr8(g_mem, row + ROW_CYCLE, (uint8_t)p->cycle);
}

/* The rows of the editions from the start year. The table grows by the editions that have no row, and by the rows
 * after the table (slice 5). */
static void hosts_apply(uint32_t self, int from_season, int sort)
{
    uint32_t rows = rd32(g_mem, self);
    int count = (int16_t)rd16(g_mem, self + 4);
    if (!readable(rows) || count <= 0 || count > 1000) return;
    season_intl_set_alive(host_alive);
    /* The original's rows of the classes with slots (Q4), by slot, before the rows of the table take their years. */
    static uint8_t taken[HOST_COMPS][8][ROW_SIZE];
    int have[HOST_COMPS][8];
    memset(have, 0, sizeof have);
    for (int c = 0; c < HOST_COMPS; c++) {
        uint32_t key = rd32(g_mem, host_comps[c].key);
        for (int i = 0; key != 0xffffffffu && i < count; i++) {
            uint32_t row = rows + (uint32_t)i * ROW_SIZE;
            int year = (int16_t)rd16(g_mem, row + ROW_YEAR), k = season_intl_host_slot(host_comps[c].file, year);
            if (rd32(g_mem, row + ROW_COMP) != key || year <= 0 || k < 0 || have[c][k]) continue;
            memcpy(taken[c][k], g_mem + row, ROW_SIZE);
            have[c][k] = 1;
            /* Item 181: a row that the draw of 2001/02 gave the table's host (season_intl_draw_hosts) is copied in
             * the original's form, as a game that starts in 2002/03 copies it. The row itself keeps its host. */
            int32_t ids[6];
            int n;
            if (from_season == SEASON_FIRST_YEAR + 1 && first_season_edition(host_comps[c].file, year, ids, &n))
                season_intl_row_undo_draw_hosts(taken[c][k], ids, n);
        }
    }
    PlannedRow plan[MAX_PLANNED];
    int planned = plan_rows(from_season, (const uint8_t (*)[8][ROW_SIZE])taken, (const int (*)[8])have, plan);
    /* The Confederations Cup row of every year (year 0, host redrawn by 0x5fab70) would come first in find_hosts,
     * before the rows of the table: it leaves for a year that no game reaches (CONFED_ANY_YEAR). In its place each
     * year from the start that has no edition in the table gets its own copy, so find_hosts still finds a host
     * there: with changing rules off the original holds the cup every two years after 2017 (fifa_confed..cpp 870,
     * 0x58c7b5, in a 2016 start on 5 January 2019). */
    uint32_t confed = rd32(g_mem, KEY_CONFED), any = 0;
    if (confed != 0xffffffffu) {
        any = row_find(rows, count, confed, 0);
        if (!any) any = row_find(rows, count, confed, CONFED_ANY_YEAR);
    }
    int extra = 0;
    for (int y = from_season; any && y <= HOST_ROWS_LAST; y++)
        if (!season_intl_table_edition("fifa_confed", y) && !row_find(rows, count, confed, y)) extra++;
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
    for (int p = 0; p < planned; p++)
        if (!row_find(rows, count, plan[p].key, plan[p].year)) extra++;
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
    for (int y = from_season; any && y <= HOST_ROWS_LAST; y++) {
        if (season_intl_table_edition("fifa_confed", y) || row_find(rows, used, confed, y)) continue;
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
    /* The original's rows of the classes that continue no longer move: the rows after the table take their
     * place (a moved row would hold a second row of an edition after the table). */
    for (int i = 0; i < used; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        const char *file = file_of_key(rd32(g_mem, row + ROW_COMP));
        if (file && strcmp(file, "fifa_confed")) wr8(g_mem, row + ROW_CYCLE, 0);
    }
    for (int p = 0; p < planned; p++) {
        uint32_t row = row_find(rows, used, plan[p].key, plan[p].year);
        if (!row) row = rows + (uint32_t)used++ * ROW_SIZE;
        plan_write(row, &plan[p], from_season);
        if (rt_unwinding) return;
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
        for (int y = year + cycle; y <= HOST_ROWS_LAST; y += cycle)
            if (season_intl_table_edition(file, y)) {
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

/* Cup::record_season_honours 0x519a90 asks for hosts only when slot +0x88 of the competition gives 1 (0x519b84):
 * the World Cup, Euro and Olympics (0x404480). The Africa Cup of Nations, Asian Cup, Copa America, Gold Cup,
 * Confederations Cup and OFC Nations Cup give 0 (0x5dce00), so the original writes no host into their history rows
 * (+0x16, 0x519c3b; open item 18). At 0x519c2f, with no host found (ebx 0 from 0x519a98), a season game takes the
 * first host of the edition's host row (season_intl_host_ids), else of the edition (the OFC Nations Cup, which has
 * no row), else the history host of a Euro with no host place (2020, 2028). The Club World Cup keeps none: its row
 * of every year holds a random nation (0x5fab70), or the edition's host while it plays (season_kind_club_world.c). */
uint32_t rt_intl_history_host(uint32_t cup, uint32_t club)
{
    if (!season_intl_on() || !readable(cup) || !readable(rd32(g_mem, cup + 4u))) return club;
    uint32_t key = rd32(g_mem, rd32(g_mem, cup + 4u));
    int year = (int16_t)rd16(g_mem, cup + 0x40u), ids[6], id = -1;
    /* Issue 145: the World Cup of 2026 names the nation of the final over the host that 0x519bdc..0x519beb chose. */
    const char *final_host = key == rd32(g_mem, host_comps[0].key) ? season_intl_history_host("world_cup", year) : NULL;
    if (club && !final_host) return club;
    int n = final_host ? 0 : season_intl_host_ids(key, year, ids);
    if (n > 0) id = ids[0];
    /* Two hosts: the one of the higher reputation (+0x8e), the first on a tie, as the original (0x519bdc..0x519beb). */
    if (n > 1 && (int16_t)rd16(g_mem, nation_at(ids[1]) + 0x8eu) > (int16_t)rd16(g_mem, nation_at(ids[0]) + 0x8eu))
        id = ids[1];
    const char *names = NULL;
    if (id < 0 && key == rd32(g_mem, KEY_OFC)) {
        const SeasonIntlEdition *e = season_intl_edition("oceania_nations", year);
        names = e ? e->hosts : NULL;
    }
    if (id < 0 && key == rd32(g_mem, KEY_EURO)) names = season_intl_history_host("euro_champ", year);
    if (final_host) names = final_host;
    if (id < 0 && names && *names) {
        char one[64];
        snprintf(one, sizeof one, "%.*s", (int)strcspn(names, ";"), names);
        id = nation_by_names(one);
    }
    if (id < 0) return club;
    uint32_t a[1] = {nation_at(id)};
    uint32_t side = call_cdecl(NATIONAL_TEAM, 1, a);
    if (logging() && !rt_unwinding)
        fprintf(stderr, "recomp: season intl: history host of %s %d: %s\n", win_str(rd32(g_mem, cup + 4u) + NATION_NAME),
                year, win_str(nation_at(id) + NATION_NAME));
    return rt_unwinding ? club : side;
}

/* Issue 146. NationalTeams::post_qualified_nations_news 0x7779e0 asks find_next_hosts 0x5fa7d0 for the hosts (0x777b02).
 * That lookup takes no year: it gives the first decided row of the competition, so an edition could get the hosts of
 * an older one, or the empty row of a Euro with no host place (2020, 2028), whose text (0x777e0d) then read the name
 * of nation row -1. In a season game the news takes the hosts of its own edition. Host k (0 or 1) of the edition, or
 * -1. The empty rows stay empty, because a host of the row takes a place at the finals (0x57be34). */
static int news_host(uint32_t key, int year, int k)
{
    int ids[6];
    int n = season_intl_host_ids(key, year, ids);
    if (n > 0) return k < n ? ids[k] : -1;
    const char *names = key == rd32(g_mem, KEY_EURO) ? season_intl_news_hosts("euro_champ", year) : NULL;
    for (int i = 0; names && i < k; i++) names = strchr(names, ';') ? strchr(names, ';') + 1 : NULL;
    if (!names) return -1;
    char one[64];
    snprintf(one, sizeof one, "%.*s", (int)strcspn(names, ";"), names);
    return nation_by_names(one);
}

/* When the news is made (0x777b07): parts 4 and 5 of the item take the hosts of the edition at +0x40 of the
 * competition (the year of part 6, 0x777b31). */
void rt_intl_news_hosts(uint32_t key, uint32_t first_at, uint32_t second_at)
{
    if (!season_intl_on()) return;
    uint32_t comp = rd32(g_mem, rd32(g_mem, COMP_OBJECTS) + key * 4u);
    if (!readable(comp)) return;
    int year = (int16_t)rd16(g_mem, comp + 0x40u), first = news_host(key, year, 0), second = news_host(key, year, 1);
    if (first < 0) return;
    wr32(g_mem, first_at, (uint32_t)first);
    wr32(g_mem, second_at, (uint32_t)second);
    if (logging())
        fprintf(stderr, "recomp: season intl: qualified news of %s %d names the hosts %s%s%s\n",
                win_str(rd32(g_mem, comp + 4u) + NATION_NAME), year, win_str(nation_at(first) + NATION_NAME),
                second >= 0 ? ";" : "", second >= 0 ? win_str(nation_at(second) + NATION_NAME) : "");
}

/* When the text is written (0x777d07): part 0 is the competition (0x777a56), so a save with a news item of an older
 * version shows the same hosts. With no host known, the first host is nation row 0, the value that 0x777ae3 sets
 * before the lookup, so the text never reads row -1. */
uint32_t rt_intl_news_text_host(uint32_t key, uint32_t year, uint32_t stored, uint32_t k)
{
    if (!season_intl_on()) return stored;
    int first = news_host(key, (int16_t)year, 0);
    if (first >= 0) return (uint32_t)(k ? news_host(key, (int16_t)year, 1) : first);
    return !k && (int32_t)stored < 0 ? 0u : stored;
}

/* A save made before 0.6.2 holds no rows after the table (a 0.6.0 or 0.6.1 game, or a game past 2030 whose
 * editions slice 4b starts again): at a load, a season game whose World Cup has no row after its table gets the
 * rows from the game's season on. A 2001 game before its first season change gets them there (intl_season). */
static void hosts_after_table_at_load(void)
{
    uint32_t obj = rd32(g_mem, HOSTS), rows = readable(obj) ? rd32(g_mem, obj) : 0;
    int count = readable(obj) ? (int16_t)rd16(g_mem, obj + 4) : 0, year = (int16_t)rd16(g_mem, GAME_DATE + 2);
    uint32_t key = rd32(g_mem, host_comps[0].key);
    if (!season_intl_on() || !readable(rows) || count <= 0 || count > 1000 || key == 0xffffffffu) return;
    if (season_start_year() == SEASON_FIRST_YEAR && year < SEASON_FIRST_YEAR + 1) return;
    int last = table_last_year("world_cup");
    for (int i = 0; i < count; i++)
        if (rd32(g_mem, rows + (uint32_t)i * ROW_SIZE + ROW_COMP) == key &&
            (int16_t)rd16(g_mem, rows + (uint32_t)i * ROW_SIZE + ROW_YEAR) > last)
            return;
    if (logging()) fprintf(stderr, "recomp: season intl: a save with no host rows after the table: rows from %d\n", year);
    hosts_apply(obj, year, 1);
}

/* Open item 103: the host fields after the first two of the original's fixed rows hold what the heap held at the
 * new game (season_intl_rows.c). A new game clears them in host_init. A save made before 0.6.2 holds them as they
 * were. While the table holds only the original's 57 rows (a 2001 game before its first season change, Q152) the
 * load clears them too. hosts_apply always adds rows, and after it every row of an edition to come is a row of
 * the table; season_intl_host_ids finds a row by its exact year, so it never reads the rows of year 0. */
static void hosts_unset_clean(uint32_t obj)
{
    uint32_t rows = readable(obj) ? rd32(g_mem, obj) : 0;
    int count = readable(obj) ? (int16_t)rd16(g_mem, obj + 4) : 0;
    if (!readable(rows) || count != SEASON_INTL_ORIGINAL_ROWS || !readable(rows + (uint32_t)count * ROW_SIZE - 1))
        return;
    int n = season_intl_rows_clean(g_mem + rows, count);
    if (n && logging()) fprintf(stderr, "recomp: season intl: %d host rows of the original: unset hosts cleared\n", n);
}

/* Open item 40 in a save made before 0.6.2: a fixed row with no host keeps its news day; it gets none. */
static void hosts_no_news(void)
{
    /* A 2001 game holds rows of the table only from its first season change (Q152: its start is the base game). */
    if (!season_intl_on() || (season_start_year() == SEASON_FIRST_YEAR &&
                              (int16_t)rd16(g_mem, GAME_DATE + 2) < SEASON_FIRST_YEAR + 1))
        return;
    uint32_t obj = rd32(g_mem, HOSTS), rows = readable(obj) ? rd32(g_mem, obj) : 0;
    int count = readable(obj) ? (int16_t)rd16(g_mem, obj + 4) : 0;
    for (int i = 0; readable(rows) && i < count && i < 1000; i++) {
        uint32_t row = rows + (uint32_t)i * ROW_SIZE;
        int none = rd8(g_mem, row + ROW_CHOICE) == CHOICE_FIXED && (int16_t)rd16(g_mem, row + ROW_YEAR) > 0 &&
                   file_of_key(rd32(g_mem, row + ROW_COMP));
        for (int k = 0; none && k < 6; k++) none = (int)rd32(g_mem, row + ROW_PAIRS + 4u * (uint32_t)k) == -1;
        if (none) wr16(g_mem, row + ROW_DAY, DAY_NEVER);
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
    /* Open item 103: the host fields that the original leaves unset in its fixed rows read as no host. The
     * original never reads them, so the 2001 game stays the base game (Q152). */
    hosts_unset_clean(self);
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
        if (rd32(g_mem, row + ROW_COMP) == key &&
            season_intl_table_edition("fifa_confed", (int16_t)rd16(g_mem, row + ROW_YEAR))) {
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

static uint32_t team_nation(uint32_t club);
static void ofc_australia(uint32_t self);
static void ofc_hosts(uint32_t self);

/* OceaniaNations::pick_entrants, thiscall (): six teams at +0xee: Australia and New Zealand by key id
 * (0x799f9c, 0x799fc1), then the two best Melanesian and the two best Polynesian nations (0x79a03c,
 * 0x79a04d). With Australia in Asia its place goes to the Oceania nation with the highest reputation (nation
 * +0x8e, inference from docs/research/2026-10-17-season-date-shifts.md) that is not in the six (ofc_australia).
 * An edition of 8 then gets two more (season_intl_ofc_draw). Then the hosts come in (ofc_hosts). */
RT_WRAP(00799e30, ofc_entrants)
{
    uint32_t self = cpu.ecx;
    o_00799e30();
    if (rt_unwinding || !cpu.eax) return;
    ofc_australia(self);
    if (!rt_unwinding) season_intl_ofc_draw(self);       /* open item 45: 8 teams (season_intl_ofc.c) */
    if (!rt_unwinding) ofc_hosts(self);
}

static void ofc_australia(uint32_t self)
{
    if (!australia_in_asia()) return;
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

/* 0.6.2 open item 45: the original never calls find_hosts for this cup, so a host plays only when it is one of
 * the six (Papua New Guinea 2016, Vanuatu 2024 did not). In a season game each host of the edition at +0x40
 * (the table hosts, after the table the rotation host) that is not in the draw takes the place of the team with
 * the lowest reputation (club +0x80, the field of the sort compare 0x4b07d0 at 0x4b07f0) that is not a host. The
 * count stays (+0xf2: 6 at 0x799e42, 8 after season_intl_ofc_draw). A team of a qualifying place is kept (0.7.0,
 * season_intl_ofc_kept): the split of direct and qualifying places already holds every Oceania host that the data has,
 * so this matters only for a host that the split leaves out (no national team, or not of Oceania). */
static void ofc_hosts(uint32_t self)
{
    if (!season_intl_on()) return;
    const SeasonIntlEdition *e = season_intl_edition("oceania_nations", (int16_t)rd16(g_mem, self + 0x40u));
    uint32_t pool = rd32(g_mem, self + 0xee);
    int count = (int16_t)rd16(g_mem, self + 0xf2);
    if (!e || !e->hosts || !readable(pool) || count <= 0 || count > 16) return;
    uint32_t hosts[6];
    int n = 0;
    char one[128];
    for (const char *p = e->hosts; *p && n < 6;) {
        size_t len = strcspn(p, ";");
        snprintf(one, sizeof one, "%.*s", (int)len, p);
        p += len;
        if (*p == ';') p++;
        uint32_t t = season_intl_team_named(one);
        if (rt_unwinding) return;
        if (t) hosts[n++] = t;
    }
    for (int h = 0; h < n; h++) {
        uint32_t nation = team_nation(hosts[h]);
        int in = 0, low = -1, low_rep = 0;
        for (int k = 0; k < count; k++) {
            uint32_t t = rd32(g_mem, pool + 4u * (uint32_t)k);
            in |= readable(t) && team_nation(t) == nation;
        }
        if (in) continue;
        for (int k = 0; k < count; k++) {
            uint32_t t = rd32(g_mem, pool + 4u * (uint32_t)k);
            int host = 0;
            for (int j = 0; j < n; j++) host |= readable(t) && team_nation(t) == team_nation(hosts[j]);
            host |= readable(t) && season_intl_ofc_kept(t);    /* a qualifying place stays (above) */
            int rep = readable(t) ? (int16_t)rd16(g_mem, t + 0x80u) : -0x8000;
            if (!host && (low < 0 || rep <= low_rep)) low = k, low_rep = rep;
        }
        if (low < 0) continue;
        wr32(g_mem, pool + 4u * (uint32_t)low, hosts[h]);
        if (logging())
            fprintf(stderr, "recomp: season intl: oceania_nations %d host %s plays\n", e->year,
                    win_str(nation + NATION_NAME));
    }
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
 * (season_intl_date_shift; an edition of another year's date moves up to 3 days more, to the first group day's
 * weekday code). The original's weekday flag (the fourth argument) stays. */
static int date_shift;

void season_intl_shift_more(int days) { date_shift += days; }
int season_intl_shift_now(void) { return date_shift; }

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

/* A draw that lies before today (open item 19). Each class's update_for_date draws on the day that equals its draw
 * date, the day word and the year offset from +0x40 (the World Cup at 0x92f1b4..0x92f1ce, with its update flag and +0xb1
 * at 0, 0x92f1fe..0x92f20a; the note above), so a draw date in the past never comes, the finals are never drawn and the
 * class waits for good. The table moves the draw with the finals (date_shift): in a 2002/03 game the OFC Nations Cup
 * of 5 July 2002 moves its first group day from 10 November by -128 days, and its draw (OceaniaNations::set_up_rules
 * 0x799690: Date::Date with day 5, month 2 from 0, so 5 March, at 0x7996b3..0x799712) with it, to 28 October 2001.
 * The game is made on 1 January 2002 (the log of the test: today day 0 of 2002), so the draw never came and the class
 * waited to 2016 (run set pass 2, a2002s1). Inference: the finals of 5 July come in the run to the start date
 * (game_run_to_start_date 0x5cc850 plays the days from the game's day to the start date), as they did in the test. So
 * when a builder of the new game (create_game, season_change_is_creating) leaves a draw date before today and the
 * finals' first day after tomorrow, the draw comes tomorrow. A draw on time is not touched, the Gold Cup object (its own
 * dates) is left out, and so are the builders of a later season and of a load (the repair of old saves, slice 4b, takes
 * the first edition whose draw is still to come; a World Cup or Euro seeded there would miss its running qualifying). */
static void late_draw(uint32_t cls, uint32_t self)
{
    if (!season_intl_on() || !season_change_is_creating() || (cls == INTL_COPA && season_gold_cup_of(self))) return;
    int year = (int16_t)rd16(g_mem, self + 0x40u), ty = (int16_t)rd16(g_mem, GAME_DATE + 2),
        td = (int16_t)rd16(g_mem, GAME_DATE);
    long today = season_intl_abs_day(ty, td), first;
    if (ty < 1900 || !season_intl_finals_first_day(cls, year, &first) || first <= today + 1) return;
    for (size_t i = 0; i < sizeof offset_pairs / sizeof offset_pairs[0]; i++) {
        if (offset_pairs[i].cls != INTL_CLASSES && offset_pairs[i].cls != cls) continue;
        int day = (int16_t)rd16(g_mem, self + offset_pairs[i].day), off = (int16_t)rd16(g_mem, self + offset_pairs[i].offset);
        if (day < 0 || day > 365 || season_intl_abs_day(year + off, day) >= today) continue;
        int ny = ty, nd = td + 1;
        if (nd >= (int)(season_intl_abs_day(ty + 1, 0) - season_intl_abs_day(ty, 0))) nd = 0, ny++;
        wr16(g_mem, self + offset_pairs[i].day, (uint16_t)nd);
        wr16(g_mem, self + offset_pairs[i].offset, (uint16_t)(ny - year));
        if (logging())
            fprintf(stderr, "recomp: season intl: %s %d: the date at +0x%x (day %d of %d) lies before today, it comes "
                            "tomorrow (day %d of %d)\n", season_intl_class_file(cls), year, offset_pairs[i].day, day,
                    year + off, nd, ny);
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
        if (in_builder && !rt_unwinding) late_draw(cls, self);
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
/* The OFC Nations Cup's edition of one group (2008) takes its stage count and draw date before the late draw. */
static void ofc_stages_body(void)
{
    uint32_t self = cpu.ecx;
    o_00799690();
    if (!rt_unwinding) season_intl_ofc_rules(self);
}
RT_WRAP(00799690, ofc_stages) { stage_builder(INTL_OFC, ofc_stages_body, 1); }
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
static void ofc_template_body(void) { season_intl_ofc_template(o_00798b60); }
RT_WRAP(00798b60, ofc_template) { stage_builder(INTL_OFC, ofc_template_body, 0); }
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
        put_field(rec + NATION_SHORT, nation_renames[i].short_name, 26);
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

static int same_name(const char *name, void *now) { return !strcasecmp(name, (const char *)now); }

/* 1 when old is an earlier name of now in a chain of the rename table (Yugoslavia for Serbia and Montenegro and
 * for Serbia), without case (season_intl_later_held). */
static int earlier_name(const char *old, const char *now)
{
    for (int i = 0; i < NATION_RENAMES; i++)
        if (!strcasecmp(nation_renames[i].old, old)) return season_intl_later_held(i, same_name, (void *)now);
    return 0;
}

int season_intl_renamed_from(const char *old, const char *now)
{
    return old && now && earlier_name(old, now) && !held_any_case(old, NULL);
}

/* The national sides of a renamed nation (open item 42 of 0.6.2): the senior and under-21 sides (the rows from
 * clubs - 2 * nations, 0x53b4c2) keep their own name at club +4 (51 bytes) and short name at +0x38 (26 bytes),
 * which the fixtures, results and history screens show. A side whose name is an earlier name of its nation's name
 * takes the nation's name and short name: after nations_rename, and in the data that already holds the new name
 * beside an old side (December 2025: the nations Czechia and Türkiye, the sides "Czech Republic" and "Turkey").
 * These rows are the nat_club.dat part of a save; a side that the data puts elsewhere (team_nation: Denmark in
 * October 2019) is not renamed. A dead record beside the live one (Yugoslavia beside Serbia) keeps its side,
 * which has its own name. No key club
 * of set_key_ids 0x601a40 (the club chain of 0x6146b0) is a national side, so a load keeps every key. Returns the
 * count. */
static int sides_rename(void)
{
    int done = 0, clubs = (int)rd32(g_mem, CLUB_COUNT), nations = nation_count();
    uint32_t base = rd32(g_mem, CLUBS);
    if (!readable(base) || !readable(rd32(g_mem, NATIONS)) || clubs < 2 * nations) return 0;
    for (int i = clubs - 2 * nations; i < clubs; i++) {
        uint32_t club = base + (uint32_t)i * CLUB_SIZE, n = team_nation(club);
        if (!n) continue;
        const char *side = win_str(club + CLUB_NAME_AT), *name = win_str(n + NATION_NAME);
        if (!strcmp(side, name) || !earlier_name(side, name)) continue;
        if (logging()) fprintf(stderr, "recomp: season intl: national side %s is now %s\n", side, name);
        char now[51], shown[26];
        snprintf(now, sizeof now, "%s", name);
        snprintf(shown, sizeof shown, "%s", win_str(n + NATION_SHORT));
        put_field(club + CLUB_NAME_AT, now, 51);
        put_field(club + CLUB_SHORT_AT, shown, 26);
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

/* A UEFA member that the data holds with no continent or in another confederation (season_intl_uefa_from: Kosovo
 * in the April 2015, October 2015 and April 2016 databases, with no continent; Kazakhstan in Asia in the 3.9.68
 * data and the 2002 databases, open item 43 of 0.6.2) gets the continent pointer of a European nation from the
 * season it joined, as Australia moves to Asia. Returns the count. news: 1 to add a sentence. */
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
        if (!from || from > season || continent_id(id) == key) continue;
        wr32(g_mem, nation_at(id) + NATION_CONTINENT, europe);
        if (logging()) fprintf(stderr, "recomp: season intl: nation %s joins UEFA (%d)\n", name, from);
        if (news) season_change_news("%s now plays in UEFA.", name);
        done++;
    }
    return done;
}

/* An AFC member that the data holds with no continent (season_intl_afc_from: Timor in the 3.9.68 data, item 200)
 * gets the continent pointer of an Asian nation from the season it joined, as concacaf_members below. The
 * pointer is the data's own, so a save keeps it. With a continent the nation enters every pool of the continent
 * (the AFC qualifying pool 0x913f60 and season_afc_first.c, the Asian Cup qualifying, the Asian Games) where its
 * national team exists, and the national vacancy of original_faults.c rt_national_vacancy (which leaves out a
 * nation with no continent) treats it as a live nation. dead_records clears only old names of a rename chain, not
 * this one. Timor has no player in the 3.9.68 data (nation +0x88 0), as Bhutan, Guam and Bangladesh, which the
 * original's own pool takes: VIRTUAL_STAFF::fill_fixture_squads 0x90d2f0 tops up a side with virtual players
 * (0x90dae0 at 0x90d3bd, 0x90d3f4) unless its nation has more than 500 players (0x90dd5d), so the side fields a
 * squad and the 15-slot check (0x90d45b..0x90d469, box virtual_staff.cpp 195 at 0x90d4bc) passes. Returns the
 * count. news: 1 to add a sentence. */
static int afc_members(int season, int news)
{
    uint32_t asia = 0, key = rd32(g_mem, CONTINENT_ASIA);
    for (int id = 0; id < nation_count() && !asia; id++) {
        uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
        if (readable(c) && rd32(g_mem, c) == key) asia = c;
    }
    if (!asia) return 0;
    int done = 0;
    for (int id = 0; id < nation_count(); id++) {
        const char *name = win_str(nation_at(id) + NATION_NAME);
        int from = season_intl_afc_from(name);
        if (!from || from > season || readable(rd32(g_mem, nation_at(id) + NATION_CONTINENT))) continue;
        wr32(g_mem, nation_at(id) + NATION_CONTINENT, asia);
        if (logging()) fprintf(stderr, "recomp: season intl: nation %s plays in the AFC (%d)\n", name, from);
        if (news) season_change_news("%s now plays in the AFC.", name);
        done++;
    }
    return done;
}

/* A CONCACAF member that the data holds with no continent (season_intl_concacaf_from: the six members that are not
 * FIFA members, in the databases from October 2016, open item 141) gets the continent pointer of a North American nation, as a UEFA member gets
 * Europe's. It then plays the CONCACAF Nations League and the Gold Cup qualifying (season_nl_nation_can_play,
 * season_gold_cup.c), and its national team gets squad sizes. The FIFA World Cup qualifying is not changed by it:
 * WcConcacafCup::select_entrants 0x916250 takes a nation whose continent is null or North America alike (0x9169bc:
 * a null continent jumps to the pick at 0x9169d2, another must equal the key at 0x9169c5..0x9169cc). Returns the
 * count. news: 1 to add a sentence. */
static int concacaf_members(int season, int news)
{
    uint32_t america = 0, key = rd32(g_mem, CONTINENT_NORTH_AMERICA);
    for (int id = 0; id < nation_count() && !america; id++) {
        uint32_t c = rd32(g_mem, nation_at(id) + NATION_CONTINENT);
        if (readable(c) && rd32(g_mem, c) == key) america = c;
    }
    if (!america) return 0;
    int done = 0;
    for (int id = 0; id < nation_count(); id++) {
        const char *name = win_str(nation_at(id) + NATION_NAME);
        int from = season_intl_concacaf_from(name);
        if (!from || from > season || readable(rd32(g_mem, nation_at(id) + NATION_CONTINENT))) continue;
        wr32(g_mem, nation_at(id) + NATION_CONTINENT, america);
        if (logging()) fprintf(stderr, "recomp: season intl: nation %s plays in CONCACAF (%d)\n", name, from);
        if (news) season_change_news("%s now plays in CONCACAF.", name);
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
    concacaf_members(season_start_year(), 0);
    afc_members(season_start_year(), 0);
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
    sides_rename();
    uefa_members(season, 1);
    concacaf_members(season, 1);
    afc_members(season, 1);
    rt_referee_ranges_repair();         /* a new member's referees (referees.c) */
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

/* A new game and a load: the national sides of the renamed nations (sides_rename). In a new game the club records
 * point at their nations only once the original has made the game; a save of 0.6.1 or before holds the old side
 * names, which the load gives the nation's name. A load moves no nation to UEFA: a 0.6.1 save with Kazakhstan in
 * the AFC keeps it there to its next season point, so no competition loses an entrant in the middle of a season. */
static void intl_sides(int start_year)
{
    (void)start_year;
    season_intl_set_alive(host_alive);
    sides_rename();
    if (season_intl_on()) hosts_unset_clean(rd32(g_mem, HOSTS));
    hosts_no_news();
    hosts_after_table_at_load();
}

/* ---- Saves of 0.6.0 and 0.6.1 with finals at the idle year (0.6.2 slice 4b, Q8) ---- */

/* Up to 0.6.1 rt_intl_next_year and rt_intl_first_year gave the year 2200 to a class after its table (0.6.2 open
 * items 1, 13 and 34): the object built its rounds for 2200 and waited. At the load of such a save each waiting
 * object moves to its next edition after the save's date: its year is set to the one before that edition, and
 * the class's own start_new_season (vtable +8) steps it there and makes the edition, as a season change does.
 * The classes by that function: world_cup 0x92d330, euro_champ 0x57b750, fifa_confed 0x58bff0, copa_america
 * 0x5e1530, african_nations 0x401d20, asia_nations 0x40f720, oceania_nations 0x7998e0, olympics 0x7a0450 (the
 * step sites of tools/recomp/seasons.py). */
#define OLD_IDLE_YEAR 2200
#define NEVER_YEAR 2999                 /* season_intl_rules.c: the year of the ended Confederations Cup */
#define COMP_SLOTS 0xadadf8u            /* the slots of the competition object table [0xadadfc] */
#define COMP_TABLE 0xadadfcu
#define DRAW_DAY 0xb6u                  /* the draw day word and its s16 year offset from +0x40 (fix_offsets) */
#define DRAW_OFFSET 0xb8u
#define FINALS_DAY 59                   /* 1 March: the qualifying's finals are still to come before it. Their last
                                           ties and the draw come later in March and April (World Cup of 48: seeding
                                           1 April and draw 2 April, season_wc48.c; Euro: the path finals of late
                                           March, season_nl_euro.c), so a load from 1 March of the finals year takes
                                           the next edition (inference: a draw day in the past is not made up) */
static const struct { uint32_t cls, new_season; int step; } repair_classes[] = {
    {INTL_WORLD_CUP, 0x0092d330u, 4}, {INTL_EURO, 0x0057b750u, 4}, {INTL_CONFED, 0x0058bff0u, 2},
    {INTL_COPA, 0x005e1530u, 2}, {INTL_ACN, 0x00401d20u, 2}, {INTL_ASIAN, 0x0040f720u, 4},
    {INTL_OFC, 0x007998e0u, 2}, {INTL_OLYMPICS, 0x007a0450u, 4},
};

/* The first edition year of a class from year `from` on, in this game: with changing rules on an edition of the
 * table or after it; with changing rules off the start season's first edition and the original's step after it
 * (rt_intl_first_year, rt_intl_next_year). 0 for none (the Confederations Cup with changing rules on). */
static int repair_target(int k, int from)
{
    const char *file = season_intl_class_file(repair_classes[k].cls);
    if (season_changing_rules()) {
        const SeasonIntlEdition *e = season_intl_next(file, from - 1);
        return e ? e->year : 0;
    }
    int first = (int)rt_intl_first_year(repair_classes[k].cls, (uint32_t)from), step = repair_classes[k].step;
    if (first >= NEVER_YEAR) return 0;
    while (first < from) first += step;
    return first;
}

static void new_season(uint32_t obj, uint32_t fn)
{
    Cpu saved = cpu;
    cpu.ecx = obj;
    rt_callback(fn, 0, NULL);
    if (!rt_unwinding) cpu = saved;
}

/* 1 when the draw day of an object (+0xb6 and its offset +0xb8 from the year +0x40) is today or before. */
static int draw_passed(uint32_t obj)
{
    int day = (int16_t)rd16(g_mem, obj + DRAW_DAY);
    int year = (int16_t)rd16(g_mem, obj + 0x40u) + (int16_t)rd16(g_mem, obj + DRAW_OFFSET);
    int today = (int16_t)rd16(g_mem, GAME_DATE), today_year = (int16_t)rd16(g_mem, GAME_DATE + 2u);
    return year < today_year || (year == today_year && day <= today);
}

/* The Confederations Cup row of every year: up to 0.6.1 at 2200 (season_intl.c hosts_apply), now at
 * CONFED_ANY_YEAR. The rows stay sorted as the constructor sorts them (0x5f994a..0x5f995a). */
static void repair_confed_row(void)
{
    uint32_t self = rd32(g_mem, HOSTS), confed = rd32(g_mem, KEY_CONFED);
    uint32_t rows = readable(self) ? rd32(g_mem, self) : 0;
    int count = readable(self) ? (int16_t)rd16(g_mem, self + 4) : 0;
    if (!readable(rows) || count <= 0 || count > 1000 || confed == 0xffffffffu) return;
    uint32_t row = row_find(rows, count, confed, OLD_IDLE_YEAR);
    if (!row) return;
    wr16(g_mem, row + ROW_YEAR, CONFED_ANY_YEAR);
    uint32_t qa[4] = {rows, (uint32_t)count, ROW_SIZE, ROW_COMPARE};
    call_cdecl(GAME_QSORT, 4, qa);
    fprintf(stderr, "recomp: season intl: a save of 0.6.1 or older: the Confederations Cup row of every year moves "
                    "from %d to %d\n", OLD_IDLE_YEAR, CONFED_ANY_YEAR);
}

/* start_new_season makes the team slots of the World Cup and the Euro again (+0x14, 0x92d3ca..0x92d3d3 for the
 * World Cup). Teams that the running campaign had qualified before the load are not put back: add_qualified_team
 * has no year test (EuroChamp 0x57c120 adds while the count +0x36 is below the room +0x38), so a waiting object can
 * hold teams of earlier campaigns as well (the World Cup of the 0.6.1 fixture held 3 in January 2099, before its
 * campaign of 2100 played), and the two cannot be told apart. The finals' fill gives those places by reputation. */

/* 1 for a save of 0.6.0 or 0.6.1: its ended Confederations Cup waits at 2200 (0.6.1 rt_intl_next_year). In 0.6.2 it
 * waits at NEVER_YEAR, and with changing rules off it plays odd years only (2005 + 2k from the start edition), so
 * no 0.6.2 game holds it at 2200. A game without the Confederations Cup gives 0 (nothing to repair is known). */
static int old_build_save(uint32_t table, int slots)
{
    for (int i = 0; i < slots; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i), vt = readable(obj) ? rd32(g_mem, obj) : 0;
        if (readable(vt) && rd32(g_mem, vt + 8u) == 0x0058bff0u)
            return (int16_t)rd16(g_mem, obj + 0x40u) == OLD_IDLE_YEAR;
    }
    return 0;
}

static void repair_idle(void)
{
    int today_year = (int16_t)rd16(g_mem, GAME_DATE + 2u);
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int slots = (int)rd32(g_mem, COMP_SLOTS);
    if (!readable(table) || slots <= 0 || slots > 100000 || today_year >= OLD_IDLE_YEAR) return;
    /* A class at the idle year moves only in a save of an old build. With changing rules off 2200 can be a real
     * edition of the original's step (the Euro 2004 + 4k): there only a load before 2196 moves it (its 2200
     * edition is not made yet). A class stuck in an edition more than a year past moves in any save: a class
     * starts its next edition at the season pass of 15 August after its finals (start_new_season of a complete
     * competition), so no class of 0.6.2 holds a past edition for more than about a year. */
    int idle_ok = old_build_save(table, slots) && (season_changing_rules() || today_year < OLD_IDLE_YEAR - 4);
    for (int i = 0; i < slots && !rt_unwinding; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i), vt = readable(obj) ? rd32(g_mem, obj) : 0;
        /* A class at the idle year, or one stuck in an edition more than a year past: up to 0.6.1 a final could
         * stall after its round of 16 or quarter-finals and never end (0.6.2 open items 2, 19 and 54), so the
         * class kept that edition for good (the forum save of August 2142: the ACN since 2012, the Asian Cup and
         * the Copa America since 2015). The repair moves both to the next edition. */
        int held_year = readable(vt) ? (int16_t)rd16(g_mem, obj + 0x40u) : 0;
        if (!readable(vt) || (held_year == OLD_IDLE_YEAR ? !idle_ok : held_year >= today_year - 1)) continue;
        int k = -1;
        for (int c = 0; c < (int)(sizeof repair_classes / sizeof repair_classes[0]); c++)
            if (rd32(g_mem, vt + 8u) == repair_classes[c].new_season) k = c;
        if (k < 0 || season_gold_cup_of(obj)) continue;     /* the Gold Cup has its own editions, never 2200 */
        uint32_t cls = repair_classes[k].cls;
        const char *file = season_intl_class_file(cls);
        int step = repair_classes[k].step;
        /* The World Cup and the Euro restart their qualifying in start_new_season (the six sections at 0x92d34f
         * to 0x92d3ae, the Euro qualifying 0x9cf784 at 0x57b76f). A qualifying object is a competition of its own:
         * it starts its next campaign when it is complete, and steps its campaign by 4 until it reaches the
         * game's year (0x919499..0x9194c2, 0x57eba3..0x57ebb9). In a save of 0.6.1 it can be in the middle of a
         * campaign: a restart now would leave that campaign's matches, which then reach a stage that is gone
         * (EuroChampQual::add_result 0x57ffd0, box "league..cpp 3214" in March 2099 of the 0.6.1 fixture). So
         * the qualifying objects are held while the finals restart, and the finals take the edition of the
         * running campaign (two years after it) while its finals are still to come, else the edition after the
         * next campaign. The other classes take the first edition from the save's year whose draw is still to
         * come. */
        int with_qual = cls == INTL_WORLD_CUP || cls == INTL_EURO, target = 0, n_quals = 0;
        uint32_t quals[6];
        if (with_qual) {
            static const uint32_t wc_keys[6] = {0x9cf770u, 0x9cf76cu, 0x9cf774u, 0x9cf778u, 0x9cf77cu, 0x9cf780u};
            for (int q = 0; q < (cls == INTL_WORLD_CUP ? 6 : 1); q++) {
                uint32_t key = cls == INTL_WORLD_CUP ? wc_keys[q] : 0x9cf784u, id = rd32(g_mem, key);
                uint32_t qo = id < (uint32_t)slots ? rd32(g_mem, table + 4u * id) : 0;
                if (readable(qo)) quals[n_quals++] = qo;
            }
            int campaign = n_quals ? (int16_t)rd16(g_mem, quals[n_quals - 1] + 0x40u) : 0;
            int today = (int16_t)rd16(g_mem, GAME_DATE);
            if (campaign && (campaign + 2 > today_year || (campaign + 2 == today_year && today < FINALS_DAY)))
                target = repair_target(k, campaign + 2) == campaign + 2 ? campaign + 2 : 0;
            /* The running campaign was made by the old build: for an edition after the table it has the
             * original's shape (open item 58). The Euro qualifying's rules bytes, which the load's constructor
             * set for the new shape (0x57d03b), go back to the original's. */
            if (target && !season_intl_table_edition(file, target))
                for (int q = 0; q < n_quals; q++) {
                    season_intl_mark_old(quals[q]);
                    if (cls == INTL_EURO) rt_euroq_rules(quals[q]);
                }
            if (!target) target = repair_target(k, today_year + 2);
        } else {
            target = cls == INTL_CONFED ? NEVER_YEAR : repair_target(k, today_year);
        }
        if (!target) continue;
        int before = season_changing_rules() && cls != INTL_CONFED ? target - 1 : target - step;
        wr16(g_mem, obj + 0x40u, (uint16_t)before);
        for (int q = 0; q < n_quals; q++) season_change_hold(quals[q], 1);
        new_season(obj, repair_classes[k].new_season);
        for (int q = 0; q < n_quals; q++) season_change_hold(quals[q], 0);
        if (rt_unwinding) return;
        for (int more = 0; more < 3 && !rt_unwinding && !with_qual && cls != INTL_CONFED && draw_passed(obj); more++)
            new_season(obj, repair_classes[k].new_season);
        if (rt_unwinding) return;
        fprintf(stderr, "recomp: season intl: a save of 0.6.1 or older: %s held %d, now its edition of %d\n",
                file, held_year, (int16_t)rd16(g_mem, obj + 0x40u));
    }
    if (!rt_unwinding) repair_confed_row();
}

/* A load: the side names of intl_sides, then the repair of finals at the idle year. The rules bytes of a Euro
 * qualifying (+0xbe, +0xbf, +0xff to +0x101) are not in the save: the load's constructor sets them at 0x57d03b
 * (rt_euroq_rules), before the competition table holds the object, so a marked campaign of a version 5 save got
 * the new shape's bytes there. They are set again now that the marks find their objects. */
#define EUROQ_VTABLE 0x0096a0d4u        /* the Euro qualifying class (season_intl_euroq.c QUAL_VTABLE) */

/* The OFC Champions League of a save made before the fix of open item 82 (rt_oceania_entry_kept above). In a game
 * that moved a selected Australia to Asia, every load after 1 January 2006 made no OceaniaClubChamp object, and
 * each save packed again the part file of the last load before the move. A load in this build makes the object from
 * that part, so it holds an edition long past (2011 or 2012 in the run set) and never plays again: a 2021 save of
 * the Tier A game a2001s1 loaded and played 230 days with no edition (run-logs/0.6.2-s4b82/red). The edition of
 * year Y draws its clubs on its draw day (+0xb6, year +0x40 plus +0xb8; update_for_date 0x797b40 compares it with
 * today at 0x797b75..0x797bf3: about day 231 of Y in that game) and plays in January of Y + 1 (5 to 16 January).
 * The object takes the next edition whose draw is still to come: its year is set to the one before (the step of
 * rt_oceania_step) and its own start_new_season (0x797d60, vtable +8) makes the edition, as a season change does,
 * once more when that draw day has passed. The rules table holds the cup in every season
 * (tests/seasons/future/competitions.tsv, oceania_club_champ, cycle 1), so that edition is one the table holds.
 * Only a game that moved a selected Australia (rt_discipline_oceania_body) is repaired. There a class holds a past
 * edition for about a year at most (the season pass of 15 August starts its next one) unless its save came from
 * before the fix. */
#define OFC_CLUB_NEW_SEASON 0x00797d60u
static void repair_ofc_club(void)
{
    uint32_t table = rd32(g_mem, COMP_TABLE);
    int slots = (int)rd32(g_mem, COMP_SLOTS), today_year = (int16_t)rd16(g_mem, GAME_DATE + 2u);
    if (!readable(table) || slots <= 0 || slots > 100000 || !rt_discipline_oceania_body()) return;
    for (int i = 0; i < slots && !rt_unwinding; i++) {
        uint32_t obj = rd32(g_mem, table + 4u * (uint32_t)i), vt = readable(obj) ? rd32(g_mem, obj) : 0;
        if (!readable(vt) || rd32(g_mem, vt + 8u) != OFC_CLUB_NEW_SEASON) continue;
        int held = (int16_t)rd16(g_mem, obj + 0x40u);
        if (held >= today_year - 1) return;
        wr16(g_mem, obj + 0x40u, (uint16_t)(today_year - (int)rt_oceania_step(2u)));
        new_season(obj, OFC_CLUB_NEW_SEASON);
        if (!rt_unwinding && draw_passed(obj)) new_season(obj, OFC_CLUB_NEW_SEASON);
        if (rt_unwinding) return;
        fprintf(stderr, "recomp: season intl: the OFC Champions League held %d (a save before the fix of item 82), "
                        "now its edition of %d, draw day %d of %d\n", held, (int16_t)rd16(g_mem, obj + 0x40u),
                (int16_t)rd16(g_mem, obj + DRAW_DAY),
                (int16_t)rd16(g_mem, obj + 0x40u) + (int16_t)rd16(g_mem, obj + DRAW_OFFSET));
        return;
    }
}

static void intl_loaded(int start_year)
{
    intl_sides(start_year);
    rt_referee_ranges_repair();         /* a save whose members joined after their referees were made (referees.c) */
    if (!rt_unwinding) repair_idle();
    if (!rt_unwinding) repair_ofc_club();
    if (!rt_unwinding) season_intl_ofc_loaded();
    for (int i = 0; i < season_intl_marks_count() && !rt_unwinding; i++) {
        uint32_t obj = season_intl_mark_object(i);
        if (obj && rd32(g_mem, obj) == EUROQ_VTABLE) rt_euroq_rules(obj);
    }
}

const SeasonRuleKind season_kind_intl = {"international", intl_sides, intl_loaded, intl_season, intl_entry, NULL,
                                         NULL};
