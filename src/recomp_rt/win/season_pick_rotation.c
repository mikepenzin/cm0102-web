/* The pick's rotation keeps enough players for the match it picks for (season games only).
 *
 * MATCH_MAN::pick_team 0x6dbc50 rotates an AI club of reputation 6000 or more (club +0x80, 0x6dbdc7) with more than
 * 30 players (0x53c590, 0x6dbe0c): it looks for the club's next fixture in the 7 days after the match
 * (FIXTURES search 0x5acbe0, called at 0x6dbe7e) and weighs the two matches (0x6dbe8c..0x6dc518). When the next match
 * counts more, it sets the pick's mode +0x1a to 0, 1 or 2 (0x6dc544, 0x6dc572, 0x6dc593). Its first loop
 * (0x6dc6b9..0x6dc88a) takes the players who can play the next match (bans 0x55d270 and eligibility 0x8cf5e0 with the
 * next fixture, 0x6dc74e and 0x6dc75e), and 0x6e2000 keeps the best 14 of them back. The second loop (0x6dc9f1) picks
 * the match from the rest: a player kept back is skipped (0x6dcae7), and the current match's eligibility comes only
 * there (0x8cf5e0 with the current fixture, 0x6dcbe6). Nothing checks that the rest can field 11.
 *
 * In gate (a) run 8 (2005 start, September 2005 database, on ec579543), AJ Auxerre (45 players, 25 registered for the
 * UEFA Cup) played its UEFA Cup group match at Siroki Brijeg on day 348 of 2005, with a Coupe de France tie on day 350.
 * The pick took mode 0 and kept 14 registered players back for the cup tie. Of the rest, 7 were registered (code 0 of
 * EUROPE_RULES::get_ineligibility_code 0x5832e0) and 24 were not (code 8). With 7 players, init_match 0x6b7f20 stopped
 * at match_eng.cpp 652 (0x6b8aca).
 *
 * The original keeps back at most 14 players who can play the current match too. So in a season game, when fewer than
 * 25 squad players can play the current match (not injured, 0x62c400 not 2, and eligible, 0x8cf5e0 code 0), the pick
 * finds no next fixture and does not rotate: the search at 0x6dbe7e returns 0 fixtures. The base game keeps the
 * original (Q152). */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>

#define SQUAD_SLOTS 50
#define KEPT_BACK 14
#define INJURY 0x62c400u                /* thiscall [0xb63d28] (staff), 2 = cannot play */
#define INJURIES 0xb63d28u
#define ELIGIBILITY 0x8cf5e0u           /* thiscall [0xde1f54] (staff, fixture), 0 = can play */
#define TRANSFERS 0xde1f54u
#define NEXT_SEARCH_RET 0x6dbe83u       /* the return address of the next-fixture search in 0x6dbc50 */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static uint32_t no_rotation_pick;       /* the pick (0x6dbc50 this) whose next-fixture search finds nothing */
static int no_rotation_free;

/* The squad players who can play the pick's fixture, as the second loop of 0x6dbc50 tests them. */
static int can_play(uint32_t pick)
{
    uint32_t squad = rd32(g_mem, pick + 0x27), fixture = rd32(g_mem, pick + 0x37);
    int n = 0;
    for (uint32_t i = 0; i < SQUAD_SLOTS && !rt_unwinding; i++) {
        uint32_t p = rd32(g_mem, squad + 4 * i);
        if (!readable(p) || !rd32(g_mem, p + 0x61)) continue;
        uint32_t a[2] = {p, fixture};
        if ((call_this(INJURY, rd32(g_mem, INJURIES), 1, a) & 0xff) == 2 || rt_unwinding) continue;
        if ((call_this(ELIGIBILITY, rd32(g_mem, TRANSFERS), 2, a) & 0xff) == 0 && !rt_unwinding) n++;
    }
    return n;
}

void o_006dbc50(void);

RT_WRAP(006dbc50, pick_rotation)
{
    uint32_t pick = cpu.ecx, fixture = rd32(g_mem, pick + 0x37), club = rd32(g_mem, pick + 0x2f);
    if (!season_game_changes() || g_mem[pick + 0x19] || g_mem[pick + 0x1a] != 3 || !readable(fixture) ||
        rd32(g_mem, fixture) == 0xfffffffeu || !readable(club) || (int16_t)rd16(g_mem, club + 0x80) < 6000 ||
        !readable(rd32(g_mem, pick + 0x27))) {
        o_006dbc50();
        return;
    }
    Cpu saved = cpu;
    int n = can_play(pick);
    if (rt_unwinding) return;
    cpu = saved;
    if (n >= KEPT_BACK + 11) {
        o_006dbc50();
        return;
    }
    no_rotation_pick = pick;
    no_rotation_free = n;
    o_006dbc50();
    no_rotation_pick = 0;
}

void o_005acbe0(void);

RT_WRAP(005acbe0, pick_rotation_next)
{
    uint32_t ret = rd32(g_mem, cpu.esp), self = no_rotation_pick;
    o_005acbe0();
    if (rt_unwinding || !self || ret != NEXT_SEARCH_RET || !(cpu.eax & 0xffff)) return;
    uint32_t fixture = rd32(g_mem, self + 0x37), club = rd32(g_mem, self + 0x2f), rec = rd32(g_mem, fixture + 0x14);
    fprintf(stderr, "recomp: pick rotation: %.30s keeps its players for the %.30s match on day %d of %d: %d players can "
                    "play it, fewer than %d\n", win_str(club + 4), readable(rec) ? win_str(rec + 4) : "?",
            (int16_t)rd16(g_mem, fixture + 0x2a), (int16_t)rd16(g_mem, fixture + 0x28), no_rotation_free, KEPT_BACK + 11);
    cpu.eax &= 0xffff0000u;             /* no fixture in the next 7 days: no rotation */
}
