/* A Copa Libertadores winner from outside CONMEBOL (item 219, lead decision of 11 November 2026).
 *
 * The original's Libertadores has clubs of nations outside CONMEBOL: the Mexican guests. When one wins the final,
 * the original makes it CONMEBOL's side in three competitions:
 *   - the Intercontinental Cup: ConmebolLiber::update_staff_records_for_status 0x4c4080, for the winner of the
 *     final stage (stage 7, status 1, 0x4c4182), writes the winner (edi, the club argument, 0x4c418e) into the
 *     club list (+0xa7) of the Intercontinental Cup object ([0xadadfc] entry [0x9cf6e4], 0x4c41a0..0x4c41ab), into
 *     place 0 when the club there is South American (0x4c41b6..0x4c41cc), else into place 1 (0x4c41d0);
 *   - the Intercontinental Cup again: WorldClubCup::pick_qualifiers 0x92b8b0 (from the constructor 0x92b53e and
 *     start_new_season 0x92be50) writes place 0 of the latest Libertadores history row ([0x9cf63c], 0x4afd80 at
 *     0x92bb96 and 0x92bc81) into place 1 of the club list (+0xa7 + 6, 0x92bba4 and 0x92bc8f);
 *   - the FIFA Club World Championship: WorldClubChamp::pick_entrant 0x92ae60 takes for a continent place 0 of the
 *     latest history row of the continent's record (0x4afd80 at 0x92af59), and place 1 only when place 0 is
 *     already picked (0x92af7d); the record of South America is the Libertadores (season_kind_club_world.c);
 *   - our Recopa Sudamericana and FIFA Intercontinental Cup (season_club_cups.c, add_winner_of_year): the winner of
 *     the Libertadores history row.
 *
 * Real life: Mexican clubs played the Libertadores as guests in 1998 and from 2000 to 2016 and never won it. The
 * Intercontinental Cup was the match of the UEFA and the CONMEBOL champions, and the guests' entry rules gave the
 * Club World Cup and Recopa places to a CONMEBOL club. So a guest
 * that wins does not take CONMEBOL's place in any of the three: the best CONMEBOL club of that edition does, which
 * is the losing finalist. Each wrap below changes only the club that takes the place, at the point where the
 * original picks it. The guest keeps its title and its history row. Only with changing rules on (lead, 11 November
 * 2026): with them off the game keeps the original's choice, also in a season game of a later start. Each time the rule
 * acts, one log line names the cup, the guest and the club that takes its place. */
#include "win.h"
#include "season.h"
#include "season_liber_guest.h"
#include "season_names.h"

#include <stdio.h>

#define CONT_SOUTH_AMERICA 0x9cfa1cu    /* the continent id of South America (0x4c41c2) */
#define CLUB_NATION 0x53u
#define NATION_CONTINENT 0x71u
#define FIX_HOME 0x1cu
#define FIX_AWAY 0x20u
#define TODAY_YEAR 0xae2c92u
#define HISTORY 0xae23d4u               /* rows of 0x1a: +4 record, +8 year, +0xa winner, +0xe runner-up */
#define HISTORY_COUNT 0xae237cu
#define HISTORY_ROW 0x1au

static int in_game(uint32_t p) { return rt_readable(p); }

/* 1: of a CONMEBOL nation, 0: of another continent, -1: no nation or continent. */
static int conmebol_side(uint32_t club)
{
    uint32_t nat = in_game(club) ? rd32(g_mem, club + CLUB_NATION) : 0;
    uint32_t cont = in_game(nat) ? rd32(g_mem, nat + NATION_CONTINENT) : 0;
    if (!in_game(cont)) return -1;
    return rd32(g_mem, cont) == rd32(g_mem, CONT_SOUTH_AMERICA);
}

int season_liber_conmebol(uint32_t club) { return conmebol_side(club) == 1; }

int season_liber_rule_on(void) { return season_code_on() && season_changing_rules(); }

int season_liber_guest(uint32_t club) { return season_liber_rule_on() && conmebol_side(club) == 0; }

uint32_t season_liber_latest(uint32_t rec, int place)
{
    uint32_t rows = rd32(g_mem, HISTORY), best = 0;
    int32_t count = (int32_t)rd32(g_mem, HISTORY_COUNT);
    int best_year = -100000;
    for (int32_t i = 0; rec && in_game(rows) && i < count && i < 1000000; i++) {
        uint32_t row = rows + (uint32_t)i * HISTORY_ROW;
        int year = (int16_t)rd16(g_mem, row + 8u);
        if (rd32(g_mem, row + 4u) != rec || year < best_year) continue;
        best = rd32(g_mem, row + 0xau + 4u * (uint32_t)place);
        best_year = year;
    }
    return in_game(best) ? best : 0;
}

void season_liber_guest_log(const char *cup, int year, uint32_t guest, uint32_t club)
{
    /* A picker can run more than once for an edition (season_club_cups.c, the day before the first draw): one line
     * for each cup, year, guest and club. */
    static struct { const char *cup; int year; uint32_t guest, club; } seen[16];
    static int next;
    for (int i = 0; i < 16; i++)
        if (seen[i].cup == cup && seen[i].year == year && seen[i].guest == guest && seen[i].club == club) return;
    seen[next].cup = cup, seen[next].year = year, seen[next].guest = guest, seen[next].club = club;
    next = (next + 1) % 16;
    uint32_t nat = rd32(g_mem, guest + CLUB_NATION);
    fprintf(stderr, "recomp: libertadores guest: %s %d: the winner %.40s (%.30s) is not a CONMEBOL club, so %.40s "
                    "takes CONMEBOL's place\n", cup, year, win_str(guest + 4u), in_game(nat) ? win_str(nat + 4u) : "?",
            in_game(club) ? win_str(club + 4u) : "no club");
}

/* The Intercontinental Cup. 0x86a800 (staff records of the final's winner, thiscall [0xdd7f00] (club, fixture,
 * arg), ret 0xc) is called at 0x4c419b just before the write; edi still holds the winner when it returns to
 * 0x4c41a0, and the recompiled caller reloads its registers after the call. So the other club of the final fixture
 * goes into edi there, and 0x4c41cc or 0x4c41d0 writes it. The winner's staff records stay its own. */
void o_0086a800(void);

RT_WRAP(0086a800, liber_final_winner)
{
    uint32_t ret = rd32(g_mem, cpu.esp), club = rt_arg(0), fixture = rt_arg(1);
    o_0086a800();
    if (rt_unwinding || ret != 0x004c41a0u || cpu.edi != club || !in_game(fixture) || !season_liber_guest(club))
        return;
    uint32_t home = rd32(g_mem, fixture + FIX_HOME), away = rd32(g_mem, fixture + FIX_AWAY);
    uint32_t other = home == club ? away : away == club ? home : 0;
    int year = (int16_t)rd16(g_mem, TODAY_YEAR);
    if (!season_liber_conmebol(other)) {
        season_liber_guest_log("Intercontinental Cup", year, club, 0);
        return;
    }
    cpu.edi = other;
    season_liber_guest_log("Intercontinental Cup", year, club, other);
}
