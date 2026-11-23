/* The Croatian second tier at 18 clubs and the Greek divisions at a new game (0.7.2 league sizes, slice 11, D22; spec
 * docs/specs/2026-11-13-league-sizes-0.7.2-spec.md).
 *
 * CroA2a::create_tiebreak_playoff_stage 0x5160f0 runs when the table is complete. It walks the rows of the table
 * (+0x3e, 0x516114..0x516210) and puts both clubs of each pair of neighbours on the same points (+0x19 at 0x51614a,
 * 0x51614f) into a stack array of 16 pointers at esp+0x1c (stores 0x51616a and 0x516190; the next local, a text
 * buffer of the error path, is at esp+0x5c, 0x5162c4). It passes the array to Cup_Stage::Cup_Stage 0x522e00
 * (lea at 0x51629a, the count in ebp at 0x5162a0), which makes its own list of that size (0x52310a..0x523131). A
 * table of 18 can give 9 tied pairs, 18 pointers: two past the array.
 *
 * In a season game with a table of more than 16 rows the pointers go to a list in the game's memory of
 * TIEBREAK_MAX slots, made once. Each pointer of the walk takes the slot of its index, as the stack array, and
 * Cup_Stage reads the list. The list lives while the stage copies it (the call at 0x5162a4); the walk of the next
 * tie-break writes it again from slot 0. The base game, RECOMP_LEAGUE_SIZES=off and a table of 16 or fewer use the
 * stack array of the original (Q152).
 *
 * The regroup of the second tier. CroA1::start_new_season 0x5149a0 runs the chain 0x5150f0 (vtable +0xb0 at
 * 0x5149b3: the First Division's exchange with the North at 0x515158 and with the South at 0x515180), then 0x514ed0
 * (4 clubs of the lower divisions up to the North, the relegated rows of both groups to A Lower Division), then
 * 0x515190 (at 0x5149c2, before the edition +0x40 goes up at 0x514a08). 0x515190 puts every club of the North
 * [0x9cf8a8] and South [0x9cf8ac] divisions in a stack array of 32 at esp+0x14 (store 0x5151ff, at most 32:
 * 0x5151fc), sorts them by the latitude of their city (0x945333 at 0x51521f, comparator 0x53fd40), and gives the
 * first 16 the North and the others the South (0x515231..0x515238), for 32 slots (0x5152bb) whatever it found. In a
 * season game the regroup walks the clubs it found, in a list of the runtime of REGROUP_MAX slots, and takes the
 * rows of the season it prepares (the edition after the First Division's):
 * - Two groups (2001/02 to 2005/06; gap row 38): the North takes the cro_a2a row and the South the cro_a2b row (12
 *   each from 2002/03), by latitude as the original; the clubs that a smaller tier leaves out are the last by
 *   results of both groups.
 * - One group (the South row "not held", 2006/07 on; gap row 40): the real second tier is one division of 12 to 18
 *   clubs, the Druga HNL, which keeps its clubs by results. The North takes the cro_a2a row in order of results
 *   (sort_by_results, no sort by latitude): the clubs down from the First Division, the North's by their places,
 *   then the South's by theirs (its best fill the North's free places). The South is not held (open item 255):
 *   the clubs after the North go to A Lower Division, which stands for the third level (3. HNL) that the 2006/07
 *   restructure sent them to; the South does not start its season (season_sizes_cro_south_before) and keeps its
 *   last table with the year 2998; after such a season the chain and 0x514ed0 leave it out (rt_cro_south_idle).
 *   A new game in such a season sends the South's clubs to A Lower Division and makes the class (it is made with
 *   the nation: CroA1::start_new_season and 0x514ed0 read its object) with a table of no row, no prize fund, no
 *   TV money, no fixtures and no sort (rt_cro_south_empty; with no row calc_prize_fund 0x682f70 stops at
 *   "league..cpp 532" and create_fixtures 0x6835c0 builds a grid from the count).
 * The clubs after North + South go to A Lower Division [0x9cf764], the spare division that the club
 * moves and the size step take clubs from (season_clubs.c), through the same store and news of the regroup
 * (0x5152be, 0x515251..0x5152a1: the news reads the target's id, so the target is a record).
 * The base game and RECOMP_LEAGUE_SIZES=off keep the original. */
#include "win.h"
#include "../rt_host.h"
#include "season.h"
#include "season_sizes.h"
#include "season_cro_gre_places.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define COMP_CLUBS 0x3eu           /* s16: the rows of the table */
#define ARRAY_SLOTS 16             /* the stack array of the original (esp+0x1c..esp+0x5c) */
#define TIEBREAK_MAX 32            /* more than the largest cap of the class (league_sizes.py GROWN cro_a2a 18) */
#define REGROUP_MAX 64             /* North at its cap 18 and South at its class count 16 is 34 */
#define LEAGUES 0x00adadfcu        /* the league objects by division index (0x5150f0) */
#define CRO_A1 0x009cf8a4u         /* the division index of the First Division */
#define KEY_LOWER 0x009cf764u      /* the division index of A Lower Division (season_clubs.c) */
#define COMPS 0x00ae23d0u          /* the division records, 0x6b bytes each (0x5151b2..0x5151d2) */
#define COMP_SIZE 0x6bu
#define SOUTH_ROWS 16              /* CroA2b's table (0x516de2) */
#define COMP_YEAR 0x40u
#define COMP_TABLE 0xb1u           /* the table: 0x3b bytes a row, the club at +0 */
#define ROW_SIZE 0x3bu
#define ROW_POS 0x39u              /* s8: the place in the table, from 1 */
#define CRO_A2A 0x009cf8a8u        /* the division index of the North */
#define CRO_A2B 0x009cf8acu        /* the division index of the South */
#define KEY_GRE_LOWER 0x009cf874u  /* the division index of the Greek Lower Division (0x5eb1e0) */
#define PLACES_UP 0xbeu            /* s8: the promotion places (0x515e33) */
#define PLACES_DOWN 0xc1u          /* s8: the relegation places (0x515e45) */

static uint32_t list;              /* the list in the game's memory, made at the first table of more than 16 */
static uint32_t regroup;           /* the list of the regroup, made at the first regroup of a season game */
static int regroup_found;          /* the clubs that the regroup found */
static int regroup_north;          /* the North's clubs of this regroup, 0 for the original's 16 */
static int regroup_south;          /* the South's clubs of this regroup */
static int regroup_by_results;     /* 1: one group, the order by results */

static int readable(uint32_t p) { return rt_readable(p); }

static int uses_list(uint32_t obj)
{
    if (!season_sizes_on() || !readable(obj)) return 0;
    int rows = (int16_t)rd16(g_mem, obj + COMP_CLUBS);
    if (rows <= ARRAY_SLOTS || rows > TIEBREAK_MAX) return 0;
    if (!list) list = rt_alloc_zero(TIEBREAK_MAX * 4u);
    return list != 0;
}

uint32_t rt_cro_tiebreak_slot(uint32_t obj, uint32_t array, uint32_t index)
{
    int i = (int16_t)(index & 0xffffu);
    if (uses_list(obj) && i >= 0 && i < TIEBREAK_MAX) return list + (uint32_t)i * 4u;
    return array + index * 4u;
}

uint32_t rt_cro_tiebreak_list(uint32_t obj, uint32_t array)
{
    return uses_list(obj) ? list : array;
}

/* 1 when the regroup of a season game uses the runtime list (made once). */
static int regroup_on(void)
{
    if (!season_sizes_on()) return 0;
    if (!regroup) regroup = rt_alloc_zero(REGROUP_MAX * 4u);
    return regroup != 0;
}

uint32_t rt_cro_regroup_max(uint32_t original)
{
    return regroup_on() ? REGROUP_MAX : original;
}

uint32_t rt_cro_regroup_slot(uint32_t array, uint32_t index)
{
    int i = (int16_t)(index & 0xffffu);
    if (regroup_on() && i >= 0 && i < REGROUP_MAX) return regroup + (uint32_t)i * 4u;
    return array + index * 4u;
}


/* The season of the rules: the edition of the First Division plus offset (0: the season that ends, 1: the one that
 * the regroup prepares), or the start season's with changing rules off (the sizes of the start season stay, Q232). */
static int rules_year(int offset)
{
    if (!season_changing_rules()) return season_start_year();
    uint32_t leagues = rd32(g_mem, LEAGUES), a1 = readable(leagues) ? rd32(g_mem, leagues + rd32(g_mem, CRO_A1) * 4u) : 0;
    return readable(a1) ? (int16_t)rd16(g_mem, a1 + COMP_YEAR) + offset : season_start_year();
}

/* 1 when the second tier of a season is one group: the South row is not held (2006/07 on). With changing rules off
 * every season takes the start season's rows (Q232). */
static int one_group(int year)
{
    if (!season_changing_rules()) year = season_start_year();
    return season_sizes_clubs("cro_a2b", year) < 2 && season_sizes_clubs("cro_a2a", year) >= 2;
}

static uint32_t league_at(uint32_t key)
{
    uint32_t leagues = rd32(g_mem, LEAGUES);
    if (!readable(leagues)) return 0;
    uint32_t obj = rd32(g_mem, leagues + rd32(g_mem, key) * 4u);
    return readable(obj) ? obj : 0;
}

/* The rank of a club in the last table of a league (from 1), or 0 when the table does not hold it. The rank is the
 * row's index: League::update_positions_and_status 0x684cd0 sorts the table (0x6827d0 at 0x684d18) at each match
 * day; row +0x39 is the best place a club can still reach, which level clubs share (season_level_ties_game.c). */
static int place_in(uint32_t obj, uint32_t club)
{
    uint32_t t = obj ? rd32(g_mem, obj + COMP_TABLE) : 0;
    int n = obj ? (int16_t)rd16(g_mem, obj + COMP_CLUBS) : 0;
    if (!readable(t)) return 0;
    for (int r = 0; r < n && r < REGROUP_MAX; r++)
        if (rd32(g_mem, t + (uint32_t)r * ROW_SIZE) == club) return r + 1;
    return 0;
}

#define KEY_NEW 300                /* a club that 0x514ed0 brought up from the lower divisions */

/* The order of the found clubs by results. The chain 0x5150f0 and 0x514ed0 run before the regroup, so the tables of
 * the season that ends are still there (League::free_table 0x687970 of the First Division is at 0x5149ca; neither
 * 0x6830b0 nor 0x6831a0 rewrites a table row). First the clubs down from the First Division, then:
 * - one group: the North's clubs by rank, then the South's (the South holds the third level, so its best fill the
 *   North's free places);
 * - two groups: the clubs of both groups by rank together (the 5th of the North and the 5th of the South side by
 *   side), so the clubs that a smaller tier sends to A Lower Division are the last of both groups.
 * Then the clubs that 0x514ed0 brought up from the lower divisions (KEY_NEW; with one group they move up to the
 * South's part in plan, so the South's last clubs go down rather than these). */
static int merit(uint32_t club, uint32_t a1, uint32_t north, uint32_t south, int one)
{
    int p;
    if ((p = place_in(a1, club)) > 0) return p;
    if ((p = place_in(north, club)) > 0) return 100 + p;
    if ((p = place_in(south, club)) > 0) return (one ? 200 : 100) + p;
    return KEY_NEW;
}

static void sort_by_results(int found, int one, int north_count)
{
    uint32_t a1 = league_at(CRO_A1), north = league_at(CRO_A2A), south = league_at(CRO_A2B);
    int key[REGROUP_MAX];
    for (int i = 0; i < found; i++) {
        uint32_t club = rd32(g_mem, regroup + (uint32_t)i * 4u);
        int k = merit(club, a1, north, south, one), j = i;
        while (j > 0 && key[j - 1] > k) {          /* stable: the club table's order within a key */
            key[j] = key[j - 1];
            wr32(g_mem, regroup + (uint32_t)j * 4u, rd32(g_mem, regroup + (uint32_t)(j - 1) * 4u));
            j--;
        }
        key[j] = k;
        wr32(g_mem, regroup + (uint32_t)j * 4u, club);
    }
    if (!one) return;
    /* One group: the new clubs go just after the North, at the head of the South's part. */
    uint32_t moved[REGROUP_MAX];
    int nm = 0, at = north_count;
    for (int i = north_count; i < found; i++)
        if (key[i] == KEY_NEW) moved[nm++] = rd32(g_mem, regroup + (uint32_t)i * 4u);
    for (int i = found - 1, w = found - 1; i >= north_count; i--)
        if (key[i] != KEY_NEW) wr32(g_mem, regroup + (uint32_t)w-- * 4u, rd32(g_mem, regroup + (uint32_t)i * 4u));
    for (int i = 0; i < nm; i++) wr32(g_mem, regroup + (uint32_t)(at + i) * 4u, moved[i]);
}

/* The counts of a regroup of found clubs: the North takes the cro_a2a row, the South the cro_a2b row (two groups,
 * 2001/02 to 2005/06) or its table's 16 (one group, 2006/07 on: the South holds the third level), the others go to
 * A Lower Division. The order is by results (sort_by_results); with two groups the original's sort by latitude then
 * orders the clubs kept.
 * No cro_a2a row: the original's 16 and 16. */
static void plan(int found)
{
    regroup_north = regroup_south = regroup_by_results = 0;
    int year = rules_year(1), north = season_sizes_clubs("cro_a2a", year), south = season_sizes_clubs("cro_a2b", year);
    if (north < 2) return;
    if (south < 2) {
        south = 0;                  /* not held: no club (open item 255) */
        regroup_by_results = 1;
    }
    if (north > found) north = found;
    if (south > found - north) south = found - north;
    regroup_north = north;
    regroup_south = south;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: cro_a2a regroup %d: %d clubs, %d North, %d South, %d to A Lower "
                        "Division, %s\n", year, found, north, south, found - north - south,
                regroup_by_results ? "by results" : "by latitude");
}

/* The array for the sort, with the clubs found (0x515219), and for the walk, count -1 (0x51522d). */
uint32_t rt_cro_regroup_list(uint32_t array, uint32_t count)
{
    if (!regroup_on()) return array;
    if ((int32_t)count >= 0) {
        regroup_found = (int32_t)count;
        plan(regroup_found);
        /* By results always; with two groups the original's sort by latitude then orders the clubs kept (the
         * first North + South, rt_cro_regroup_sort_count), and those after them go to A Lower Division. */
        if (regroup_north > 0) sort_by_results(regroup_found, regroup_by_results, regroup_north);
    }
    return regroup;
}

/* The count of the original's sort by latitude (0x51521d): none with one group (the order is by results), the
 * clubs kept with two groups (North + South, the first of the order by results), all without a row. */
uint32_t rt_cro_regroup_sort_count(uint32_t count)
{
    if (!regroup_on() || regroup_north <= 0) return count;
    return regroup_by_results ? 0u : (uint32_t)(regroup_north + regroup_south);
}

uint32_t rt_cro_regroup_north(uint32_t original)
{
    return regroup_on() && regroup_north > 0 ? (uint32_t)regroup_north : original;
}

uint32_t rt_cro_regroup_end(uint32_t original)
{
    return regroup_on() ? (uint32_t)regroup_found : original;
}

/* The division of the club at a place of the walk (0x51523a, place [esp+0x10]): the North or South that the original
 * chose (ebx), or A Lower Division for a place after North + South. */
uint32_t rt_cro_regroup_target(uint32_t chosen, uint32_t place)
{
    if (!regroup_on() || regroup_north <= 0 || (int32_t)place < regroup_north + regroup_south) return chosen;
    int32_t key = (int32_t)rd32(g_mem, KEY_LOWER);
    uint32_t comps = rd32(g_mem, COMPS);
    return key >= 0 && readable(comps) ? comps + (uint32_t)key * COMP_SIZE : chosen;
}

/* 1 when the South waits (season_sizes_cro_south_before set its year to 2998): it did not play the season that
 * ends, so the chain 0x5150f0 leaves out its record_final_positions (vtable +0xa4 at 0x51512f) and 0x514ed0 its
 * relegated rows (0x515097), which would read its old table. */
uint32_t rt_cro_south_idle(void)
{
    uint32_t south = season_sizes_on() ? league_at(CRO_A2B) : 0;
    return south && ((int16_t)rd16(g_mem, south + COMP_YEAR) >= 2998 || (int16_t)rd16(g_mem, south + COMP_CLUBS) == 0);
}

/* 1 for the South with a table of no row: a new game in a season of one group (season_sizes_cro_new_game). Its
 * set-up leaves out the prize fund, the TV money, the fixtures and the sort (sizes_cro.py). */
uint32_t rt_cro_south_empty(uint32_t obj)
{
    if (!season_sizes_on() || !readable(obj) || (int16_t)rd16(g_mem, obj + COMP_CLUBS) != 0) return 0;
    uint32_t rec = rd32(g_mem, obj + 4u);
    int32_t key = (int32_t)rd32(g_mem, CRO_A2B);
    uint32_t comps = rd32(g_mem, COMPS);
    return key >= 0 && readable(comps) && rec == comps + (uint32_t)key * COMP_SIZE;
}

/* The exchange of the South with the First Division at 0x515180 (1: the original's call): none after a season of
 * one group, when the South was not held (it waited, or played the start season of a new game). */
uint32_t rt_cro_south_swap(void)
{
    if (!season_sizes_on()) return 1;
    int year = rules_year(0);
    if (!one_group(year)) return 1;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: cro_a2b %d: not held, no exchange with the First Division\n", year);
    return 0;
}

/* The before hook of a season start (season_divisions.c divisions_before): in a season of one group the South is
 * not held. Its clubs left at the regroup (plan: all but the North's to A Lower Division, as the 2006/07
 * restructure sent them to the third level), so CroA2b::start_new_season 0x516fc0 would make a table of no club
 * (CroA2b::create_table 0x516dd0 fails at 0x516eb9: "cro_a2b..cpp 446", line 0x1be at 0x517096). The South keeps
 * its last table and waits with the year 2998, as an object that waits (season_comps.h): 1 to skip the start. */
int season_sizes_cro_south_before(uint32_t comp, int season)
{
    if (!season_sizes_on() || !comp || comp != league_at(CRO_A2B) || !one_group(season)) return 0;
    wr16(g_mem, comp + COMP_YEAR, 2998u);
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: cro_a2b %d: not held, the South waits\n", season);
    return 1;
}

/* A new game in a season of one group (season_clubs.c, after the club moves and before the competitions are made):
 * the South is not held, so its clubs of the database go to A Lower Division, the third level that the 2006/07
 * restructure sent them to. The class is made with the nation, with a table of no row (season_sizes_cro_empty) and
 * no fixtures (rt_cro_south_empty), and its first season start waits (season_sizes_cro_south_before). */
#define CLUBS 0x00ae23b4u          /* the club table, 0x245 bytes a club */
#define CLUB_COUNT 0x00ae235cu
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u        /* club_comp pointer */

void season_sizes_cro_new_game(int start)
{
    if (!season_sizes_on() || !one_group(start)) return;
    uint32_t clubs = rd32(g_mem, CLUBS), comps = rd32(g_mem, COMPS);
    int count = (int)rd32(g_mem, CLUB_COUNT), key = (int32_t)rd32(g_mem, CRO_A2B), lower = (int32_t)rd32(g_mem, KEY_LOWER);
    if (!readable(clubs) || !readable(comps) || key < 0 || lower < 0) return;
    uint32_t south = comps + (uint32_t)key * COMP_SIZE, to = comps + (uint32_t)lower * COMP_SIZE;
    int moved = 0;
    for (int i = 0; i < count; i++) {
        uint32_t club = clubs + (uint32_t)i * CLUB_SIZE;
        if (rd32(g_mem, club + CLUB_DIVISION) != south) continue;
        wr32(g_mem, club + CLUB_DIVISION, to);
        moved++;
    }
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: cro_a2b %d: not held, the South's %d clubs start in A Lower Division\n",
                start, moved);
}

/* League::pay_prize_money 0x682e10 (thiscall, ret 0), which League::check_complete 0x689ad0 calls at 0x689ba4 when
 * the table is complete (a table of no row is complete at once): it shares the prize fund (+0x28) by the sum of the
 * places 1 to n (0x682ebf..0x682ed6) and divides by that sum (idiv at 0x682edf), 0 for no row. The empty South pays
 * nothing. */
void o_00682e10(void);
RT_WRAP(00682e10, league_pay_prize_money)
{
    if (rt_cro_south_empty(cpu.ecx)) {
        rt_ret_stdcall(cpu.eax, 0);
        return;
    }
    o_00682e10();
}

/* After a load: the South of a game made in a season of one group (season_sizes_cro_new_game) has no club, and
 * League::load 0x687b10 makes no table for a league of no row (+0x3e 0 at 0x6889db..0x6889e2 jumps to 0x688e06, which
 * stores +0xb1 = 0). League::check_complete 0x689ad0 is then false (+0xb1 0 at 0x689ad5..0x689add), and the season
 * start test of the First Division 0x48ce10 asks each division along +0x20 (0x48ce30..0x48ce4d): the North's +0x20 is
 * the South (CroA2a::set_up_rules 0x515e54..0x515e79). So after a load the Croatian leagues never started a new season
 * (release set sp2021: the HNL and the Prva NL held edition 2022 at the end of 2023/24). The South gets back the table
 * of no row that CroA2b::create_table 0x516dd0 made for it: SOUTH_ROWS empty rows, as its malloc at the new game. */
void season_sizes_cro_loaded(void)
{
    uint32_t south = season_sizes_on() ? league_at(CRO_A2B) : 0;
    if (!south || !rt_cro_south_empty(south) || rd32(g_mem, south + COMP_TABLE)) return;
    uint32_t table = season_sizes_game_malloc(SOUTH_ROWS * ROW_SIZE);
    if (!table) return;
    for (uint32_t i = 0; i < SOUTH_ROWS * ROW_SIZE; i++) g_mem[table + i] = 0;
    wr32(g_mem, south + COMP_TABLE, table);
    /* Always logged (once per load), so a test of a loaded game shows that the table came back. */
    fprintf(stderr, "recomp: league sizes: cro_a2b %d: no club, the load gives it back its table of no row\n",
            (int16_t)rd16(g_mem, south + COMP_YEAR));
}

/* 1 for the First Division of a game whose Croatian leagues stalled before this fix: a season of one group, the
 * South with its table (season_sizes_cro_loaded), and an edition before the game year after the season change's
 * update day (season_playoffs_hold tests the day). The change then needs only its gate 0x48ce10, which the South's
 * table opens. One season only: a game two seasons behind gets one change, then a table with no match played. */
int season_sizes_cro_behind(uint32_t comp, int year)
{
    uint32_t a1 = season_sizes_on() && readable(comp) ? league_at(CRO_A1) : 0, south = a1 ? league_at(CRO_A2B) : 0;
    if (!a1 || comp != a1 || !south || !rd32(g_mem, south + COMP_TABLE) || !one_group(year)) return 0;
    return (int16_t)rd16(g_mem, a1 + COMP_YEAR) < year;
}

/* 1 for the South in a season of one group: the size engine gives it a table of no row (rt_league_clubs). */
int season_sizes_cro_empty(uint32_t obj, const char *file, int season)
{
    return season_sizes_on() && file && !strcmp(file, "cro_a2b") && one_group(season) && readable(obj);
}

/* The places of the Druga HNL and the Greek second tier from their rows, with changing rules on (open items 281 and
 * 282; the rules are in season_cro_gre_places.c). With changing rules off the classes keep their places. */
void season_sizes_cro_rules(uint32_t obj, const char *file)
{
    if (!season_sizes_on() || !season_changing_rules() || !readable(obj) || !file) return;
    int up = (int8_t)g_mem[obj + PLACES_UP], down = (int8_t)g_mem[obj + PLACES_DOWN];
    int new_up = season_cro_gre_up(file, one_group(season_sizes_row_season(obj, file)),
                                   season_sizes_row_up(obj, file), up);
    int new_down = season_cro_gre_down(file, season_sizes_row_down(obj, file), down);
    if (new_up == up && new_down == down) return;
    g_mem[obj + PLACES_UP] = (uint8_t)new_up;
    g_mem[obj + PLACES_DOWN] = (uint8_t)new_down;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: %s %d: %d up and %d down from the row, the class gave %d and %d\n", file,
                (int16_t)rd16(g_mem, obj + COMP_YEAR), new_up, new_down, up, down);
}

/* The record of the Greek Lower Division [0x9cf874], the pool of GrePrm's exchange 0x5eb120 (0x5eb1e0), or 0. */
uint32_t season_sizes_gre_lower_record(void)
{
    int32_t id = (int32_t)rd32(g_mem, KEY_GRE_LOWER);
    uint32_t comps = rd32(g_mem, COMPS);
    return id >= 0 && readable(comps) ? comps + (uint32_t)id * COMP_SIZE : 0;
}

/* 1 for the Greek Lower Division: the small Greek clubs wait there, not in A Lower Division, so the size step takes
 * the clubs of a larger second tier from it (open item 282: 16 clubs in 2006/07 and 2012/13). */
int season_sizes_gre_lower(uint32_t record)
{
    return record && record == season_sizes_gre_lower_record();
}

/* The count that fix_division_club_count 0x839920 gives a Greek division at a new game (0x8283c0 National A,
 * 0x8283ce National B; the original pushes 14): in a season game with the sizes on, the row of the start season
 * (season_sizes_clubs: the first row of the file, the National B Division for gre_second). The base game,
 * RECOMP_LEAGUE_SIZES=off and a season with no row keep 14. */
uint32_t rt_gre_fix_count(uint32_t original, uint32_t second)
{
    const char *file = second ? "gre_second" : "gre_prm";
    int n = season_sizes_clubs(file, season_start_year());
    if (n < 2) return original;
    if (getenv("RECOMP_SIZE_LOG"))
        fprintf(stderr, "recomp: league sizes: %s %d: the new game fixes the division at %d clubs (the original %u)\n",
                file, season_start_year(), n, original);
    return (uint32_t)n;
}
