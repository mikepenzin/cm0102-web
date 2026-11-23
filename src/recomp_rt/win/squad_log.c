/* Test log of the national squad picks (open item 143). RECOMP_SQUAD_LOG=1 logs and only reads memory.
 *
 * After each senior pick of NationalTeams::select_squad 0x76c120 (slot 0), one line: the nation, the fixture's key
 * (fixture +0: the friendly 0x9cf668, 'B' 0x9cf794, Under 21 0x9cf798, else another record), the reputation of
 * the fixture's competition (fixture +0x14, +0x69) and of the opponent (club +0x80), the squad slots now set
 * (national side +0xd7, 50), and the nation's players with a club, those at a club on another continent (club
 * +0x53 nation, nation +0x71 continent, as 0x76cc3d..0x76cc59 tests) and those of them the release rule refuses:
 * for the friendly a club reputation over the opponent's / 0.8 (0x76ccdd..0x76cd3e, 0x9689b0), for 'B' and
 * Under 21 every one (0x76ccc3), else a division (club +0x57) reputation +0x69 x 0.7 over the competition's
 * (0x76cc61..0x76cc9a, 0x968e40). The count is the original's rule: with the fix of group callups
 * (original_faults.py) the senior pick takes them all the same. RECOMP_SQUAD_LOG_NATION=<name> also logs, for that
 * nation, the gate that stops each player with a club who is not in the squad (gate_of). It calls five queries of the
 * original that the pick calls too (Injury::get_player_injury_status 0x62c400, player_regen_get_retirement_status
 * 0x7a7eb0, staff_dislikes_club 0x547d30, CONTRACT_MANAGER::get_contract 0x4cf5a0, Discipline::is_player_banned
 * 0x55d270). That they change nothing is not proven, so a run with it is a diagnosis, never a same-save run. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define STAFF 0xae23bcu
#define STAFF_COUNT 0xae2364u
#define DATE 0xae2c90u
#define KEY_FRIENDLY 0x9cf668u
#define KEY_B 0x9cf794u
#define KEY_U21 0x9cf798u

static int readable(uint32_t p) { return rt_readable(p); }

int squad_log_on(void)
{
    static int v = -1;
    if (v < 0) {
        const char *e = getenv("RECOMP_SQUAD_LOG");
        v = e && strcmp(e, "1") == 0;
    }
    return v;
}

static uint32_t senior_side(uint32_t nation)
{
    int clubs = (int)rd32(g_mem, CLUB_COUNT), nations = (int)rd32(g_mem, NATION_COUNT);
    uint32_t base = rd32(g_mem, CLUBS);
    for (int i = clubs - 2 * nations; i >= 0 && i < clubs - nations; i++) {
        uint32_t c = base + (uint32_t)i * 0x245u;
        if (rd32(g_mem, c + 0x53) == nation) return c;
    }
    return 0;
}

static uint32_t continent(uint32_t club)
{
    uint32_t n = readable(club) ? rd32(g_mem, club + 0x53) : 0;
    return readable(n) ? rd32(g_mem, n + 0x71) : 0;
}

/* The first gate of the candidate loop of 0x76c120 that stops a player who is not in the squad after a senior pick,
 * by the pick's own queries: injured (0x62c400 on [0xb63d28], 0x76cba7), 0x7a7eb0 on [0xb7434c] (0x76cbbf), 0x547d30
 * (player, senior side, 0x76cbe2), in another national squad (+0x24 another side, 0x76cbf2..0x76cc0a), the club table
 * at NationalTeams +0x24 by club id with contract (0x4cf5a0) +0x3d over -10 (0x76cd44..0x76cd73), 0x55d270 on
 * [0xae28f0] (player, competition, 1, 0; 0x76cd8f). "none" when none of these stops him (a later gate: 0x76db00, the
 * window tests from 0x76ce40, the second nationality, the band or 0x6e2000). */
static uint32_t call(uint32_t fn, uint32_t self, int n, uint32_t *a)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, a);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static const char *gate_of(uint32_t sp, uint32_t side, uint32_t teams, uint32_t comp)
{
    uint32_t a[4] = {sp, 0, 0, 0};
    uint32_t r = call(0x62c400u, rd32(g_mem, 0xb63d28u), 1, a);
    if (rt_unwinding) return "-";
    if (r & 0xffu) return "injured";
    uint32_t m = rd32(g_mem, 0xb7434cu);
    r = m ? call(0x7a7eb0u, m, 1, a) : 0;
    if (rt_unwinding) return "-";
    if (r & 0xffu) return "0x7a7eb0";
    a[1] = side;
    r = call(0x547d30u, 0, 2, a);
    if (rt_unwinding) return "-";
    if (r) return "0x547d30";
    uint32_t other = rd32(g_mem, sp + 0x24);
    if (other && other != side) return "other squad";
    uint32_t table = readable(teams) ? rd32(g_mem, teams + 0x24) : 0, club = rd32(g_mem, sp + 0x39);
    if (readable(table) && g_mem[table + rd32(g_mem, club)]) {
        uint32_t c = call(0x4cf5a0u, rd32(g_mem, 0xae1050u), 1, a);
        if (rt_unwinding) return "-";
        if (readable(c) && (int8_t)g_mem[c + 0x3d] > -10) return "club table";
    }
    a[1] = comp, a[2] = 1, a[3] = 0;
    r = readable(comp) ? call(0x55d270u, rd32(g_mem, 0xae28f0u), 4, a) : 0;
    if (rt_unwinding) return "-";
    return r ? "0x55d270" : "none";
}

static int in_side(uint32_t side, uint32_t sp)
{
    for (uint32_t j = 0; j < 50; j++)
        if (rd32(g_mem, side + 0xd7 + 4 * j) == sp) return 1;
    return 0;
}

void squad_log_pick(uint32_t nation, int slot, uint32_t fixture, uint32_t teams)
{
    if (!squad_log_on() || slot != 0 || !readable(nation)) return;
    uint32_t side = senior_side(nation);
    if (!readable(side)) return;
    int set = 0;
    for (uint32_t j = 0; j < 50; j++) set += rd32(g_mem, side + 0xd7 + 4 * j) != 0;
    uint32_t key = readable(fixture) ? rd32(g_mem, fixture) : 0;
    const char *kind = !readable(fixture) ? "none" : key == rd32(g_mem, KEY_FRIENDLY) ? "friendly"
                     : key == rd32(g_mem, KEY_B) ? "B" : key == rd32(g_mem, KEY_U21) ? "u21" : "other";
    uint32_t comp = readable(fixture) ? rd32(g_mem, fixture + 0x14) : 0;
    int comp_rep = readable(comp) ? (int16_t)rd16(g_mem, comp + 0x69) : -1;
    uint32_t opp = 0;
    if (readable(fixture))
        opp = rd32(g_mem, fixture + 0x1c) == side ? rd32(g_mem, fixture + 0x20) : rd32(g_mem, fixture + 0x1c);
    int opp_rep = readable(opp) ? (int16_t)rd16(g_mem, opp + 0x80) : -1;
    uint32_t home_cont = rd32(g_mem, nation + 0x71), staff = rd32(g_mem, STAFF), count = rd32(g_mem, STAFF_COUNT);
    int players = 0, away = 0, refused = 0;
    for (uint32_t i = 0; readable(staff) && i < count; i++) {
        uint32_t sp = staff + i * 0x6eu;
        if (rd32(g_mem, sp + 0x1a) != nation || !readable(rd32(g_mem, sp + 0x61))) continue;
        uint32_t club = rd32(g_mem, sp + 0x39);
        if (!readable(club)) continue;
        players++;
        uint32_t c = continent(club);
        if (!c || c == home_cont) continue;
        away++;
        if (!strcmp(kind, "friendly")) refused += opp_rep < 0 || (int16_t)rd16(g_mem, club + 0x80) * 0.8 > opp_rep;
        else if (!strcmp(kind, "B") || !strcmp(kind, "u21")) refused++;
        else {
            uint32_t div = rd32(g_mem, club + 0x57);
            refused += readable(div) && (int16_t)rd16(g_mem, div + 0x69) * 0.7 > comp_rep;
        }
    }
    fprintf(stderr, "recomp: squad log: day %d/%d %.30s: %s, competition %.40s rep %d, opponent %.30s rep %d: %d "
                    "picked; %d players with a club, %d on another continent, %d of them refused\n",
            (int16_t)rd16(g_mem, DATE), (int16_t)rd16(g_mem, DATE + 2), win_str(nation + 4), kind,
            readable(comp) ? win_str(comp + 4) : "-", comp_rep, readable(opp) ? win_str(opp + 4) : "-", opp_rep, set,
            players, away, refused);
    const char *only = getenv("RECOMP_SQUAD_LOG_NATION");      /* the gates of one nation's players */
    if (!only || strcmp(only, win_str(nation + 4))) return;
    for (uint32_t i = 0; readable(staff) && i < count && !rt_unwinding; i++) {
        uint32_t sp = staff + i * 0x6eu, pr = rd32(g_mem, sp + 0x61);
        if (rd32(g_mem, sp + 0x1a) != nation || !readable(pr) || !readable(rd32(g_mem, sp + 0x39))) continue;
        if (in_side(side, sp)) continue;
        const char *g = gate_of(sp, side, teams, comp);
        fprintf(stderr, "recomp: squad log:   not picked: staff %u, ability %d, record +9 %d, club %.30s: %s\n",
                (sp - staff) / 0x6eu, (int16_t)rd16(g_mem, pr + 5), (int16_t)rd16(g_mem, pr + 9),
                win_str(rd32(g_mem, sp + 0x39) + 4), g);
    }
}

/* Each fixture that the daily pass 0x76b100 gives select_squads_for_fixture 0x76bab0 (before it): the two sides,
 * the key, the byte +0x40 and the flags +0x4d (word) and +0x4e (0x76bad1..0x76badc skips +0x40 = 0 with bit 0 of
 * +0x4e). */
static void record_line(uint32_t teams, uint32_t club)
{
    uint32_t nation = readable(club) ? rd32(g_mem, club + 0x53) : 0;
    if (!readable(nation) || !readable(teams)) return;
    uint32_t r = rd32(g_mem, teams + 4) + rd32(g_mem, nation) * 0x6eu;
    if (!readable(r)) return;
    fprintf(stderr, "recomp: squad log:   %.30s record: +0x3a %d/%d +0x3e %d/%d +0x43 %d; bytes", win_str(nation + 4),
            (int16_t)rd16(g_mem, r + 0x3a), (int16_t)rd16(g_mem, r + 0x3c), (int16_t)rd16(g_mem, r + 0x3e),
            (int16_t)rd16(g_mem, r + 0x40), g_mem[r + 0x43]);
    for (uint32_t k = 0x30; k < 0x6e; k++) fprintf(stderr, " %02x", g_mem[r + k]);
    fprintf(stderr, "\n");
}

void squad_log_fixture(uint32_t fixture, uint32_t teams)
{
    if (!squad_log_on() || !readable(fixture)) return;
    uint32_t h = rd32(g_mem, fixture + 0x1c), a = rd32(g_mem, fixture + 0x20);
    uint32_t key = rd32(g_mem, fixture), comp = rd32(g_mem, fixture + 0x14);
    fprintf(stderr, "recomp: squad log: fixture day %d/%d: %.30s v %.30s on %d/%d, key %s, competition %.40s, "
                    "+0x40 %d, +0x4d 0x%04x\n",
            (int16_t)rd16(g_mem, DATE), (int16_t)rd16(g_mem, DATE + 2), readable(h) ? win_str(h + 4) : "-",
            readable(a) ? win_str(a + 4) : "-", (int16_t)rd16(g_mem, fixture + 0x2a),
            (int16_t)rd16(g_mem, fixture + 0x28),
            key == rd32(g_mem, KEY_FRIENDLY) ? "friendly" : key == rd32(g_mem, KEY_B) ? "B"
            : key == rd32(g_mem, KEY_U21) ? "u21" : "other", readable(comp) ? win_str(comp + 4) : "-",
            g_mem[fixture + 0x40], rd16(g_mem, fixture + 0x4d));
    record_line(teams, h);
    record_line(teams, a);
}
