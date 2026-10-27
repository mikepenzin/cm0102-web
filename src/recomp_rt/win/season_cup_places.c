/* Domestic cups with a fixed number of places that a season's divisions leave short (0.6.0 release test, O260 row
 * A6: the causes of the cup place fills of season_cup_entrants.c, cup by cup).
 *
 * The Irish provincial cups.
 *
 * IreLeinsterCup::select_clubs 0x636a80 fills 16 places: four clubs drawn from the Leinster Senior League (the
 * division of [0x9cf854], 0x636b51; shuffle 0x4afbd0 at 0x636bdc, places 0 to 3 at 0x636bea..0x636c08), then the
 * clubs of the Premier and First Divisions ([0x9cf80c], [0x9cf810], 0x636b6d..0x636b77) whose stadium is in one of
 * seven towns (ire_club_is_in_leinster 0x6399c0: Dublin, Bray, Longford, Dundalk, Drogheda, Athlone, Kilkenny) into
 * places 4 to 15 (0x636b8c..0x636ba4). IreMunsterCup::collect_entrants 0x638e30 fills 20: sixteen drawn from the
 * Munster Senior League ([0x9cf850], 0x638f0b; 0x639081..0x6390b5) and four Premier or First Division clubs found by
 * name (Cork City, Cobh Ramblers, Waterford United, Limerick F.C., strings 0x9e9ff4..0x9e9fc0; places 16 to 19 at
 * 0x63902e..0x639046). The season databases from 2008/09 have fewer League of Ireland clubs in those seven towns and
 * other names for some of the four Munster clubs (Waterford FC, Limerick FC), or not the club at all, so places
 * stayed empty (gate (a) run 7: Leinster 1 or 2 of 16, Munster 1 or 2 of 20, ag-cup-fills.md).
 *
 * The real cups are open to the senior league clubs of the province, so after the class's pick an empty place takes
 * the next club of the cup's own provincial league that is not in the list, by reputation (+0x80): no random draw is
 * added, so the rest of the game is as before when no place is empty. Only in a season game (season_game_changes);
 * the base game's lists are full, so it is never reached there (Q152). */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>

#define CUP_TEAMS 0x3eu
#define CUP_TEAM_LIST 0xa7u
#define ENTRY 6u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_COUNT_LESS 0xae2350u
#define CLUB_SIZE 0x245u
#define CLUB_DIVISION 0x57u
#define CLUB_REPUTATION 0x80u
#define KEY_LEINSTER_LEAGUE 0x9cf854u
#define KEY_MUNSTER_LEAGUE 0x9cf850u
#define KEY_POL_TOP 0x9cf858u

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

/* Fills the empty places from..to - 1 of cup with the clubs of the division of key not in the list, by
 * reputation; byte4 is the entry's round byte (+4). */
static void top_up(uint32_t cup, uint32_t key, int from, int to, int places, int byte4, const char *name,
                   const char *pool)
{
    if (!season_game_changes() || !readable(cup)) return;
    uint32_t list = rd32(g_mem, cup + CUP_TEAM_LIST), table = rd32(g_mem, CLUBS);
    int32_t div = (int32_t)rd32(g_mem, key);
    if (!readable(list) || !readable(table) || div < 0) return;
    int clubs = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, CLUB_COUNT_LESS), empty = 0, filled = 0;
    for (int i = from; i < to; i++) {
        uint32_t at = list + ENTRY * (uint32_t)i;
        if (readable(rd32(g_mem, at))) continue;
        empty++;
        uint32_t best = 0;
        for (int k = 0; k < clubs && k < 200000; k++) {
            uint32_t c = table + CLUB_SIZE * (uint32_t)k, d = rd32(g_mem, c + CLUB_DIVISION);
            if (!readable(d) || (int32_t)rd32(g_mem, d) != div) continue;
            int in = 0;
            for (int j = 0; j < places && !in; j++) in = rd32(g_mem, list + ENTRY * (uint32_t)j) == c;
            int better = !best || (int16_t)rd16(g_mem, c + CLUB_REPUTATION) >
                                      (int16_t)rd16(g_mem, best + CLUB_REPUTATION);
            if (!in && better) best = c;
        }
        if (!best) continue;
        wr32(g_mem, at, best);
        if (byte4 >= 0) g_mem[at + 4] = (uint8_t)byte4;
        g_mem[at + 5] = 0;
        filled++;
    }
    if (empty)
        fprintf(stderr, "recomp: cup places: %s %d: %d of %d places empty, %d from %s\n", name,
                (int16_t)rd16(g_mem, cup + 0x40), empty, places, filled, pool);
}

/* The new list of IreLeinsterCup::select_clubs (0x60 bytes, 0x636a98), IreMunsterCup::collect_entrants (0x78,
 * 0x638e4b) and PolCup::collect_entrants (0x138, 0x7c856f), at its store to +0xa7 (0x636aa2, 0x638e55, 0x7c8579).
 * The allocator 0x944e46 does not clear the block, and each class writes only the places it finds clubs for:
 * Leinster places 4 to 15 for the Premier and First Division clubs in its seven towns (0x636b9a), Munster places
 * 16 to 19 for the four clubs it finds by name (0x639038), the Polish top places for the clubs of the top division
 * (0x7c885b). Each start_new_season makes a new list, and the block is often the freed list of the last edition,
 * so a place the class did not write still held last edition's club. top_up took such a place as full: in a 2008
 * game (September 2008 database, gate (a) pre-run on 53fba30a) the 2009/10 Leinster Cup had place 15 empty and
 * top_up gave it Arklow Town; in 2010/11 the four senior league clubs drawn into places 0 to 3 held Arklow Town
 * again, place 15 still held it from the old block, so Arklow Town came in twice, a tie of round 2 lost its club
 * and Cup::process_fixture 0x51a150 stopped at Cup.cpp 1187 (find_tie 0x519c90 found no tie, 0x51a336). In a
 * season game the list now starts cleared. The base game writes every place, so it keeps the original's block
 * (Q152). */
uint32_t rt_cup_places_list(uint32_t list, uint32_t bytes)
{
    if (season_game_changes() && readable(list) && readable(list + bytes - 1u))
        for (uint32_t i = 0; i < bytes; i++) g_mem[list + i] = 0;
    return list;
}

void o_00636a80(void);
void o_00638e30(void);

/* IreLeinsterCup::select_clubs, thiscall () ret 0 (constructor and start_new_season 0x636c20). */
RT_WRAP(00636a80, regional_leinster_clubs)
{
    uint32_t cup = cpu.ecx;
    o_00636a80();
    if (rt_unwinding) return;
    uint32_t eax = cpu.eax;
    top_up(cup, KEY_LEINSTER_LEAGUE, 0, 16, 16, -1, "ire_leinster_cup", "its senior league");
    cpu.eax = eax;
}

/* IreMunsterCup::collect_entrants, thiscall () ret 0 (constructor and start_new_season 0x6390d0). */
RT_WRAP(00638e30, regional_munster_clubs)
{
    uint32_t cup = cpu.ecx;
    o_00638e30();
    if (rt_unwinding) return;
    uint32_t eax = cpu.eax;
    top_up(cup, KEY_MUNSTER_LEAGUE, 0, 20, 20, -1, "ire_munster_cup", "its senior league");
    cpu.eax = eax;
}

/* The Polish Cup. PolCup::collect_entrants 0x7c8550 fills 52 places (0x7c8569): the Second Division ([0x9cf85c])
 * by last place 13 to 20 and 1 to 12 (0x7c86d7..0x7c871a, 0x7c8759..0x7c87ae), Lower Division clubs up to 36 places
 * (0x7c87b4..0x7c880a), then every club whose last division (+0x5b) is the top division ([0x9cf858], 0x7c8846..
 * 0x7c8870, round byte 1) in the places from 36. With the October 2003 database 15 clubs have the top division as
 * their last division, so place 51 stayed empty (2003 start, gate (a) run 6; match_eng..cpp 652). The top places
 * then also take the clubs whose division of this season (+0x57) is the top division and that are not in the list,
 * by reputation: the cup gets the top division of the season it is played in.
 *
 * With changing rules on, the 2003 club moves leave no club of this season's top division outside the list (gate (a)
 * run 8 on eee3e71a, 2003 start): the clubs that went down from the top division are in the top places by their last
 * division, the clubs that came up are in the Second Division places, and the 16th club of 2002/03 has another last
 * division in the database. So "cup places: pol_cup 2003: 1 of 52 places empty, 0 from this season's top division",
 * and the cup entrants guard (season_cup_entrants.c) filled place 51. The empty top places then take the next clubs
 * of the cup's own pools, as the original fills the list: this season's Second Division ([0x9cf85c]), then the Lower
 * Division ([0x9cf868], the pool of 0x7c87b4..0x7c880a), by reputation, with the round byte of the top places (1), so
 * the rounds keep their counts. In the 2003 start every Second Division club is in the list, so a Lower Division club
 * takes place 51. */
#define KEY_POL_SECOND 0x9cf85cu
#define KEY_POL_LOWER 0x9cf868u
void o_007c8550(void);

RT_WRAP(007c8550, places_pol_cup)
{
    uint32_t cup = cpu.ecx;
    o_007c8550();
    if (rt_unwinding) return;
    uint32_t eax = cpu.eax;
    int count = readable(cup) ? (int16_t)rd16(g_mem, cup + CUP_TEAMS) : 0;
    if (count > 36) {
        top_up(cup, KEY_POL_TOP, 36, count, count, 1, "pol_cup", "this season's top division");
        top_up(cup, KEY_POL_SECOND, 36, count, count, 1, "pol_cup", "this season's Second Division");
        top_up(cup, KEY_POL_LOWER, 36, count, count, 1, "pol_cup", "the Lower Division");
    }
    cpu.eax = eax;
}
