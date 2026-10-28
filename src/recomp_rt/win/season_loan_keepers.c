/* An AI club keeps at least 3 goalkeepers when it loans players out (season games only).
 *
 * A loan out ends in TRANSFER_MANAGER::handle_player_accepted_terms 0x8c2180 (staff*, offer index, contract*, byte,
 * byte), thiscall on [0xde1f54], ret 0x14. For a loan offer (offer +0x1f 1) it tests the loan club's squad count
 * (0x53c590 at 0x8c23fd: one more than 50 is "squad full", reason 4) and the loan club's nation rules (rule object
 * +0x20, can_loan_player). With a reason it takes the rejection path (0x8e23d0 with the reason at 0x8c24a4, offer
 * +0x2c 0x10, 0x8c6db0, the shortlist removals 0x84aeb0). With none it signs the loan (CONTRACT_MANAGER::
 * sign_loan_contract 0x4d01e0 at 0x8c2684). Nothing in the parent club's side of the deal counts the players it keeps
 * in a position.
 *
 * In gate (a) run 8b (2022 start, October 2022 database, on ec579543), HNK Rijeka had 4 goalkeepers (player data
 * +0x0f 20) at the start and loaned 3 out: 18957 on day 198 of 2022, 70598 on day 313, 75682 on day 17 of 2023, all
 * from 0x8c2689. For its reserve match at NK Varazdin on day 95 of 2023 the pick (0x6dbc50, +0x19 1) kept the first
 * team's 14 back with the only keeper. The selection (0x6e2000) puts no player with a keeper rating below 10 in goal,
 * so it found 10 players and init_match stopped at match_eng.cpp 652 (0x6b8aca).
 *
 * So in a season game, a loan out of a goalkeeper (rating KEEPER_FROM or more) by an AI club that has 3 or fewer
 * goalkeepers at the club (staff +0x39) takes the original's own rejection: the squad count at 0x8c23fd reads as
 * full. The parent club is the club of the player's main contract (CONTRACT_MANAGER::get_contract 0x4cf5a0, +4).
 * Logged once a staff member (AI clubs make the same offer again). The base game keeps the original (Q152). */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>

#define SQUAD_SLOTS 50
#define KEEPERS_KEPT 3
#define KEEPER_FROM 15                  /* goalkeeper rating (player data +0x0f, 1 to 20) of a goalkeeper */
#define CLUBS 0xae23b4u
#define CONTRACTS 0xae1050u
#define GET_CONTRACT 0x4cf5a0u          /* thiscall [0xae1050] (staff), the main contract or 0 */
#define IS_HUMAN 0x600a50u              /* cdecl (club, 0, 1, 0, 0), nonzero for a human club */
#define SQUAD_COUNT_RET 0x8c2402u       /* the return address of the squad count in 0x8c2180's loan path */

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static int keeper(uint32_t staff)
{
    uint32_t pr = rd32(g_mem, staff + 0x61);
    return readable(pr) && g_mem[pr + 0x0f] >= KEEPER_FROM;
}

static uint32_t parent_club(uint32_t staff)
{
    Cpu saved = cpu;
    uint32_t a[1] = {staff};
    cpu.ecx = rd32(g_mem, CONTRACTS);
    uint32_t c = rt_callback(GET_CONTRACT, 1, a);
    if (rt_unwinding) return 0;
    cpu = saved;
    if (!readable(c) || (g_mem[c + 0x35] & 0x3f) == 5) return 0;
    return rd32(g_mem, CLUBS) + rd32(g_mem, c + 4) * 0x245u;
}

static int human(uint32_t club)
{
    Cpu saved = cpu;
    uint32_t a[5] = {club, 0, 1, 0, 0};
    uint32_t r = rt_callback(IS_HUMAN, 5, a);
    if (rt_unwinding) return 1;
    cpu = saved;
    return r != 0;
}

static int keepers_at(uint32_t club)
{
    int n = 0;
    for (uint32_t j = 0; j < SQUAD_SLOTS; j++) {
        uint32_t sp = rd32(g_mem, club + 0xd7 + 4 * j);
        if (readable(sp) && rd32(g_mem, sp + 0x39) == club && keeper(sp)) n++;
    }
    return n;
}

static int full_squad_once;
static unsigned char logged[1u << 17];  /* staff ids already logged */             /* the next squad count at 0x8c23fd reads as full */

void o_008c2180(void);

RT_WRAP(008c2180, loan_keepers)
{
    uint32_t staff = rt_arg(0), idx = rt_arg(1), tm = cpu.ecx;
    full_squad_once = 0;
    if (season_game_changes() && readable(staff) && readable(tm) && (int32_t)idx >= 0 &&
        (int32_t)idx < (int32_t)rd32(g_mem, tm + 4)) {
        uint32_t offer = rd32(g_mem, rd32(g_mem, tm) + 4 * idx);
        if (readable(offer) && g_mem[offer + 0x1f] == 1 && rd32(g_mem, staff) == rd32(g_mem, offer + 0xc) &&
            keeper(staff)) {
            Cpu saved = cpu;
            uint32_t parent = parent_club(staff);
            int n = readable(parent) && !rt_unwinding && !human(parent) ? keepers_at(parent) : KEEPERS_KEPT + 1;
            if (rt_unwinding) return;
            cpu = saved;
            if (n <= KEEPERS_KEPT) {
                full_squad_once = 1;
                uint32_t id = (staff - rd32(g_mem, 0xae23bcu)) / 0x6eu;
                if (id < sizeof logged && !logged[id]++)
                    fprintf(stderr, "recomp: loan keepers: %.30s keeps its goalkeeper, staff %u (%d at the club): the "
                                "loan to %.30s is turned down\n", win_str(parent + 4),
                        id, n,
                        win_str(rd32(g_mem, CLUBS) + rd32(g_mem, offer + 8) * 0x245u + 4));
            }
        }
    }
    o_008c2180();
    full_squad_once = 0;
}

void o_0053c590(void);

RT_WRAP(0053c590, loan_keepers_squad_count)
{
    uint32_t ret = rd32(g_mem, cpu.esp);
    o_0053c590();
    if (rt_unwinding || !full_squad_once || ret != SQUAD_COUNT_RET) return;
    full_squad_once = 0;
    cpu.eax = (cpu.eax & 0xffffff00u) | SQUAD_SLOTS;  /* one more than 50: "squad full", reason 4 */
}
