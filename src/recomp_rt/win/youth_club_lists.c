/* The English club list of Welsh and Irish new players in a season game (open item 262).
 *
 * PLAYER_REGEN::PLAYER_REGEN 0x7a5380 calls build_nation_club_lists 0x7acfc0 (at 0x7a54cf) at a new game and at each
 * load. The function makes one club list for each nation (this[0] is the table of lists by nation id). A club of a
 * nation with a continent (+0x71) that is no national side (0x53b900) and not one of the two clubs of the keys
 * 0x9d00a4 and 0x9d00a8 (0x7ad091..0x7ad09b) goes in the list of its nation (0x7ad04c..0x7ad0b0):
 *   - in a nation that is not loaded (league flags +0x11c are 0), when it has a division (+0x57);
 *   - in a loaded nation, only when it has a division and a reputation (+0x80) of 2 or less (0x7ad06b..0x7ad084).
 *
 * regenerate_as_youth_player 0x7abac0 gives a new player a club in one of two ways (0x7ace65..0x7acf5c). A new player
 * under 19 of a loaded nation gets, in three of four cases, the club of the best score of all clubs of his nation
 * (pick_best_club_for_player 0x7ad0e0). Any other new player goes to pick_club_from_nation_list 0x7ad590: one of a
 * nation that is not loaded when rand(15) is not 0, else when rand(10) is 0 (0x7acf1b..0x7acf46). So nearly every new
 * player of a nation that is not loaded takes this path. 0x7ad590 takes the list of England for Wales and the
 * Republic of Ireland, and for Northern Ireland when rand(5) < 2 (0x7ad5b6..0x7ad5f0). Every club of the list that can
 * sign him (0x8ce990) and has fewer than 50 squad players (0x53c590) gets a score and a random draw (0x7ad6ae..0x7ad713;
 * a player of the nation of key 0x9cf424 only at five clubs, 0x7ad63f..0x7ad67f). The best draw gets him.
 *
 * When England is loaded, its list holds only its clubs of reputation 2 or less. In the base data that is Guiseley
 * alone (reputation 0, "A Lower Division"). So nearly every Welsh and Irish new player goes to Guiseley, until it has
 * 50 squad players. In a 2026 start with Italy and changing rules on (seed 1), all 22 Welsh and Irish new players of
 * the first 300 days went to Guiseley, and 53 new players joined it from day 243 of 2026 to day 182 of 2027 (26 of the
 * Republic of Ireland, 20 of Wales, 1 of Northern Ireland, 6 of England). The youth pool (youth_pool.c) gives these
 * nations the young ages, and season_national_fill.c calls such players up for the under-21 sides. On day 268 of
 * 2027, 38 of Guiseley's 50 players were on international duty (NationalTeams::get_international_duty_club 0x7757d0)
 * on the day of a friendly at Angri. MATCH_MAN::pick_team 0x6dbc50 gave 10 players, and init_match 0x6b7f20 stopped
 * the game with match_eng.cpp 652 (0x6b8aca).
 *
 * The fix, in a season game only (season_game_changes), when England is loaded. At the first new player of Wales, the
 * Republic of Ireland or Northern Ireland that comes to 0x7ad590 after a new game or load, the wrap calls 0x7acfc0 a
 * second time with the league flags of England at 0 for the call. Of that second table it keeps only the list of
 * England: every English club with a division, the list that a nation that is not loaded gets. Then it puts the first
 * table back in this[0] (the second table stays allocated; at most one per new game or load). For that new player
 * and each later one of these nations, the wrap of 0x7ad590 puts that list in the English row of the table for the
 * call, and then the old list. For Northern Ireland the draw rand(5) still decides between its own list and the
 * English one. English new players and the lists of all other nations do not change. RECOMP_TEST_YOUTH_LIST=original
 * keeps the original lists.
 *
 * A 2001 start with changing rules on plays its first season as the base game (Q152, as the under-21 rule of
 * season_national_fill.c). So in that game the wrap changes nothing before 1 July 2002. The second call waits for
 * the first new player that the change takes, because its allocations (0x944e46, 0x944cf1, the list adds 0x68d0d0)
 * alone change the play: a call at the new game made the 14 days of the same-save check differ in that game. */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NATIONS 0xae23a8u           /* the nation table, 0x122 bytes a nation (the nation id is its row) */
#define NATION_COUNT 0xae2350u
#define NATION_SIZE 0x122u
#define NATION_LOADED 0x11cu        /* the league flags of a nation: 0 when not loaded */
#define KEY_WALES 0x9cf510u         /* the nation ids that 0x7ad590 maps to England (0x7ad5b6..0x7ad5d5) */
#define KEY_IRELAND 0x9cf2dcu
#define KEY_NORTHERN_IRELAND 0x9cf3e4u
#define KEY_ENGLAND 0x9cf2e4u
#define GAME_YEAR 0xae2c92u         /* the u16 game year, after the s16 day of the year from 0 */

static uint32_t regen, english;     /* the PLAYER_REGEN; the English list (0 not made, 1 none) */

static int original(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("RECOMP_TEST_YOUTH_LIST");
        v = e && !strcmp(e, "original");
    }
    return v;
}

/* The nation record of England when it is loaded, else 0. */
static uint32_t england_loaded(int32_t *row)
{
    *row = (int32_t)rd32(g_mem, KEY_ENGLAND);
    if (*row < 0 || *row >= (int32_t)rd32(g_mem, NATION_COUNT) || !rd32(g_mem, NATIONS)) return 0;
    uint32_t rec = rd32(g_mem, NATIONS) + (uint32_t)*row * NATION_SIZE;
    return g_mem[rec + NATION_LOADED] ? rec : 0;
}

/* 1 when the change acts now: a start after 2001/02, or a 2001 start from the 2002/03 season (day 181 is 1 July). */
static int rule_now(void)
{
    if (season_start_year() != SEASON_FIRST_YEAR) return 1;
    int day = (int16_t)rd16(g_mem, GAME_YEAR - 2u), year = (int16_t)rd16(g_mem, GAME_YEAR);
    return (day >= 181 ? year : year - 1) > SEASON_FIRST_YEAR;
}

void o_007acfc0(void);

/* PLAYER_REGEN::build_nation_club_lists 0x7acfc0, thiscall, no arguments: a new game or load forgets the list. */
RT_WRAP(007acfc0, youth_club_lists)
{
    uint32_t self = cpu.ecx;
    regen = english = 0;
    o_007acfc0();
    if (!rt_unwinding) regen = self;
}

/* The list of every English club with a division, from a second call of 0x7acfc0 with England not loaded. 0 when
 * the change does not act in this game. */
static uint32_t english_list(uint32_t self)
{
    int32_t row;
    uint32_t rec;
    if (!season_game_changes() || original() || !(rec = england_loaded(&row))) return 0;
    uint32_t first = rd32(g_mem, self), flags = g_mem[rec + NATION_LOADED];
    Cpu saved = cpu;
    g_mem[rec + NATION_LOADED] = 0;
    cpu.ecx = self;
    rt_callback(0x7acfc0u, 0, NULL);
    if (!rt_unwinding) cpu = saved;     /* an unwind keeps the esp of the frame it resumes */
    g_mem[rec + NATION_LOADED] = (uint8_t)flags;
    uint32_t second = rd32(g_mem, self);
    wr32(g_mem, self, first);
    if (rt_unwinding || !second || second == first) return 0;
    uint32_t all = rd32(g_mem, second + 4u * (uint32_t)row), own = rd32(g_mem, first + 4u * (uint32_t)row);
    if (!all) return 0;
    fprintf(stderr, "recomp: youth club lists: Welsh and Irish new players take the English list of %d clubs, not %d\n",
            (int16_t)rd16(g_mem, all), own ? (int16_t)rd16(g_mem, own) : 0);
    return all;
}

void o_007ad590(void);

/* PLAYER_REGEN::pick_club_from_nation_list 0x7ad590, thiscall (ret 4), the new player on the stack: his club or 0. */
RT_WRAP(007ad590, youth_english_list)
{
    uint32_t self = cpu.ecx, nation = rd32(g_mem, rt_arg(0) + 0x1a), table = rd32(g_mem, self);
    int32_t row = (int32_t)rd32(g_mem, KEY_ENGLAND);
    uint32_t id = nation ? rd32(g_mem, nation) : 0xffffffffu;
    if (self != regen || english == 1 || !table || row < 0 || original() || !rule_now() ||
        (id != rd32(g_mem, KEY_WALES) && id != rd32(g_mem, KEY_IRELAND) && id != rd32(g_mem, KEY_NORTHERN_IRELAND))) {
        o_007ad590();
        return;
    }
    if (!english) {
        english = english_list(self);
        if (rt_unwinding) return;
        if (!english) english = 1;          /* no change in this game: no second try until the next load */
        if (english == 1) {
            o_007ad590();
            return;
        }
    }
    uint32_t slot = table + 4u * (uint32_t)row, own = rd32(g_mem, slot);
    wr32(g_mem, slot, english);
    o_007ad590();
    wr32(g_mem, slot, own);
}
