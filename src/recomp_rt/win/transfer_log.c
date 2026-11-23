/* Test switches for the offers to human clubs (item 132, tests/seasons/holiday_offers_run.py). Both are off by default.
 *
 * RECOMP_TRANSFER_LOG=1 logs, and changes no game state: each reply to an offer for a human club and its result
 * (TRANSFER_MANAGER::ai_respond_to_bid 0x8c8090, accept_transfer_bid 0x8c0080, reject_transfer_offer 0x8bf680,
 * accept_loan_offer 0x8c0660, reject_loan_offer 0x8bfa30) and each completed move (finalise_transfer 0x8c9390), all
 * thiscall on [0xde1f54]. The offer: +8 buying club id, +0xc player id, +0x1b fee, +0x1f 1 for a loan, +0x2c state.
 * The player: staff +0x52 the asking price (set_player_squad_status 0x8ce1e9). His main contract
 * (CONTRACT_MANAGER::get_contract 0x4cf5a0): +4 club id, +0x4e bit 0 transfer listed
 * (shortlist_manager_remove_from_transfer_list 0x844280), a release clause in force by
 * STAFF_CONTRACT::has_active_release_clause 0x867b50. A club is human when its manager (+0xcf) is one of the last 16
 * staff rows (human_manager_is_human_club 0x600a50). The human record is [[0xb63c98]] + (id - [0xae2364] + 16) *
 * 0xa6ef: +0x9a01 the holiday flag, +0x9b23 the holiday settings.
 *
 * RECOMP_TEST_LIST changes the game. "odd": when a holiday starts, Shortlist_Manager::rebuild_club_shortlist 0x84abe0
 * runs for the human's club (call at 0x762513). Before it, every odd squad slot (club +0xd7) of that club goes on the
 * transfer list through Shortlist_Manager::transfer_list_player 0x842320 (player, post_news 0), as the transfer
 * status screen does. "census" (reads only): at that point, or at the first completed move of a run that loads a game
 * on holiday, it counts the players with a contract, those with no asking price and those with contract bit 0x4
 * clear, all and transfer-listed. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define STAFF 0xae23bcu
#define CONTRACTS 0xae1050u
#define GET_CONTRACT 0x4cf5a0u
#define RELEASE_CLAUSE 0x867b50u
#define LIST_PLAYER 0x842320u
#define STAFF_COUNT 0xae2364u
#define HUMANS 0xb63c98u
#define DATE 0xae2c90u
#define HOLIDAY_START_RET 0x762518u   /* the return address of the call of 0x84abe0 at the start of a holiday */

static int log_on(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("RECOMP_TRANSFER_LOG");
        v = e && strcmp(e, "1") == 0;
    }
    return v;
}

enum { LIST_OFF, LIST_ODD, LIST_CENSUS };

static int list_mode(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("RECOMP_TEST_LIST");
        v = !e ? LIST_OFF : !strcmp(e, "odd") ? LIST_ODD : !strcmp(e, "census") ? LIST_CENSUS : LIST_OFF;
    }
    return v;
}

static int readable(uint32_t p) { return rt_readable(p); }

static uint32_t contract_of(uint32_t staff)
{
    Cpu saved = cpu;
    uint32_t a[1] = {staff};
    cpu.ecx = rd32(g_mem, CONTRACTS);
    uint32_t c = rt_callback(GET_CONTRACT, 1, a);
    if (rt_unwinding) return 0;
    cpu = saved;
    return readable(c) ? c : 0;
}

static int has_clause(uint32_t contract)
{
    Cpu saved = cpu;
    cpu.ecx = contract;
    uint32_t r = rt_callback(RELEASE_CLAUSE, 0, NULL);
    if (rt_unwinding) return 0;
    cpu = saved;
    return (r & 0xffu) != 0;
}

/* The human record of a club's manager, or 0. */
static uint32_t human_record(uint32_t club)
{
    if (!readable(club)) return 0;
    uint32_t m = rd32(g_mem, club + 0xcf);
    if (!readable(m)) return 0;
    int idx = (int)rd32(g_mem, m) - (int)rd32(g_mem, STAFF_COUNT) + 16;
    uint32_t h = rd32(g_mem, HUMANS);
    if (idx < 0 || idx >= (int)rd32(g_mem, h + 4)) return 0;
    return rd32(g_mem, h) + (uint32_t)idx * 0xa6efu;
}

/* One line for the offer at offer; only offers for a human club, but every completed move. */
static void line(const char *what, uint32_t offer, uint32_t ret)
{
    if (!readable(offer)) return;
    uint32_t pid = rd32(g_mem, offer + 0xc), buyer = rd32(g_mem, CLUBS) + rd32(g_mem, offer + 8) * 0x245u;
    uint32_t staff = rd32(g_mem, STAFF) + pid * 0x6eu;
    uint32_t c = contract_of(staff);
    if (rt_unwinding) return;
    uint32_t seller = c ? rd32(g_mem, CLUBS) + rd32(g_mem, c + 4) * 0x245u : 0;
    uint32_t hr = human_record(seller);
    if (!hr && strcmp(what, "final") != 0) return;
    int clause = c ? has_clause(c) : 0;
    if (rt_unwinding) return;
    fprintf(stderr,
            "recomp: transfer log: %s day %d/%d from 0x%x: player %u from %.25s to %.25s, flags 0x%x, fee %d, "
            "asking %d, loan %d, state %d, clause %d, seller human %d, holiday %d, options %d %d\n",
            what, (int16_t)rd16(g_mem, DATE), (int16_t)rd16(g_mem, DATE + 2), ret, pid,
            seller ? win_str(seller + 4) : "-", win_str(buyer + 4), c ? g_mem[c + 0x4e] : -1,
            (int32_t)rd32(g_mem, offer + 0x1b), (int32_t)rd32(g_mem, staff + 0x52), g_mem[offer + 0x1f],
            g_mem[offer + 0x2c], clause, hr != 0, hr ? (int32_t)rd32(g_mem, hr + 0x9a01) : -1,
            hr ? g_mem[hr + 0x9b23] : -1, hr ? g_mem[hr + 0x9b27] : -1);
}

static void census(void)
{
    Cpu s = cpu;
    uint32_t n = rd32(g_mem, STAFF_COUNT), base = rd32(g_mem, STAFF);
    unsigned all = 0, zero = 0, clear4 = 0, listed = 0, lzero = 0, lclear4 = 0;
    for (uint32_t i = 0; i < n; i++) {
        uint32_t p = base + i * 0x6eu;
        if (!rd32(g_mem, p + 0x61)) continue;
        uint32_t c = contract_of(p);
        if (rt_unwinding) return;
        if (!c || (g_mem[c + 0x35] & 0x3f) != 2) continue;
        all++;
        int z = (int32_t)rd32(g_mem, p + 0x52) <= 0, b = !(g_mem[c + 0x4e] & 4);
        zero += z;
        clear4 += b;
        if (g_mem[c + 0x4e] & 1) {
            listed++;
            lzero += z;
            lclear4 += b;
        }
    }
    cpu = s;
    fprintf(stderr, "recomp: transfer log: census day %d/%d: %u players with a contract, %u asking 0, %u bit4 clear; "
                    "%u listed, %u asking 0, %u bit4 clear\n", (int16_t)rd16(g_mem, DATE), (int16_t)rd16(g_mem, DATE + 2),
            all, zero, clear4, listed, lzero, lclear4);
}

/* The functions whose first argument is an offer slot (TRANSFER_OFFER **): log before the original. */
static int slot_line(const char *what)
{
    if (!log_on()) return 1;
    Cpu s = cpu;
    uint32_t slot = rt_arg(0);
    line(what, readable(slot) ? rd32(g_mem, slot) : 0, rd32(g_mem, cpu.esp));
    if (rt_unwinding) return 0;
    cpu = s;
    return 1;
}

void o_008c0080(void);
void o_008bf680(void);
void o_008c8090(void);
void o_008c0660(void);
void o_008bfa30(void);

RT_WRAP(008c0080, transfer_log_accept)
{
    if (slot_line("accept")) o_008c0080();
}

RT_WRAP(008bf680, transfer_log_reject)
{
    if (slot_line("reject")) o_008bf680();
}

RT_WRAP(008c8090, transfer_log_respond)
{
    if (slot_line("respond")) o_008c8090();
}

RT_WRAP(008c0660, transfer_log_loan_accept)
{
    if (slot_line("loan-accept")) o_008c0660();
}

RT_WRAP(008bfa30, transfer_log_loan_reject)
{
    if (slot_line("loan-reject")) o_008bfa30();
}

void o_008c9390(void);

/* finalise_transfer takes the offer itself. */
RT_WRAP(008c9390, transfer_log_final)
{
    static int counted;
    if (!counted && list_mode() == LIST_CENSUS) {
        counted = 1;
        census();
        if (rt_unwinding) return;
    }
    if (log_on()) {
        Cpu s = cpu;
        line("final", rt_arg(0), rd32(g_mem, cpu.esp));
        if (rt_unwinding) return;
        cpu = s;
    }
    o_008c9390();
}

void o_0084abe0(void);

RT_WRAP(0084abe0, transfer_log_list)
{
    int mode = list_mode();
    uint32_t club = rt_arg(0), sm = cpu.ecx;
    if (mode != LIST_OFF && rd32(g_mem, cpu.esp) == HOLIDAY_START_RET) {
        Cpu s = cpu;
        if (mode == LIST_CENSUS) census();
        for (uint32_t j = 1; mode == LIST_ODD && readable(club) && j < 50; j += 2) {
            uint32_t p = rd32(g_mem, club + 0xd7 + 4 * j);
            if (!readable(p) || rd32(g_mem, p + 0x39) != club) continue;
            uint32_t a[2] = {p, 0};
            cpu.ecx = sm;
            rt_callback(LIST_PLAYER, 2, a);
            if (rt_unwinding) return;
            fprintf(stderr, "recomp: transfer log: listed player %u\n", (p - rd32(g_mem, STAFF)) / 0x6eu);
        }
        if (rt_unwinding) return;
        cpu = s;
    }
    o_0084abe0();
}
