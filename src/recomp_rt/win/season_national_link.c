/* A cancelled national match takes its linked 'B' or Under 21 match with it (season games only).
 *
 * add_fixture 0x5aa680 adds, for a senior national match that is not a tournament match, a linked 'B' International
 * or Under 21 International between the same nations on the day before (ReserveComp::add_b_or_under21_fixture
 * 0x7e6720, called at 0x5aab09; the add at 0x7e6a48). The linked match takes its squad with the senior match's pick:
 * NationalTeams::select_squads_for_fixture 0x76bab0 skips a fixture with +0x40 = 0 and bit 1 of +0x4e
 * (0x76bad1..0x76badc), and the pick for the senior match picks the B or Under 21 squad too (from 0x76bc28).
 *
 * When a friendly is cancelled (Friendly::on_friendly_fixture_cancelled 0x5c6570, from add_fixture's clash code at
 * 0x5aa9d0), Friendly::remove_national_fixture 0x5cb9e0 removes the linked match: for flags +0x4d & 0x300 = 0x100
 * (B) the club of the senior side (+0x1c), for 0x200 (Under 21) the under-21 side of its nation (0x53b4f0), and only
 * the first fixture of that club on the day before (find_fixtures 0x5acbe0 at 0x5cba9b, remove at 0x5cbabc), then the
 * friendly itself (0x5cbacd). A move of the friendly breaks that: in gate (a) run 8 (2004 start, on 8e58c44a) the
 * friendly England v Germany was made for day 88 of 2008 with its 'B' International on day 87. The B match's own
 * add found the clash with the friendly the next day, and resolve_same_day_clash 0x5ae380 moved the friendly to day
 * 89 (0x5ae56f). On day 51 a new fixture cancelled the friendly (remove at 0x5cbacd). Day 88 held no England match,
 * so the B match stayed with no senior match. No pick came for it, and on day 87 England's side was the squad of
 * November 2007, every player on duty: the team pick 0x6db500 found fewer than 11 (box match_eng.cpp 612, 0x6b8973).
 *
 * So in a season game, when a cancelled friendly has a linked match and the day before holds none, the linked match
 * of the same two nations (the B or Under 21 record) within a week before or after is removed with it. The base game
 * keeps the original (Q152). */
#include "win.h"
#include "season.h"
#include "season_names.h"

#include <stdio.h>

#define FIXTURES 0xae2a58u
#define FIXTURE_SEARCH 0x5acbe0u        /* thiscall FIXTURES (start day, start year, end day, end year, club index,
                                         * LLIST *out, comp -1, type -2, side), returns the count */
#define LIST_FREE 0x68d040u             /* LLIST::~LLIST */
#define REMOVE_FIXTURE 0x5abc30u        /* thiscall FIXTURES (fixture), ret 4 */
#define U21_SIDE 0x53b4f0u              /* cdecl (nation) returns the nation's under-21 side */
#define KEY_B 0x9cf794u
#define KEY_U21 0x9cf798u
#define CLUB_NATION 0x53u
#define WINDOW 7

static int readable(uint32_t p) { return p >= 0x10000u && p < 0xfff00000u; }

static uint32_t call_this(uint32_t fn, uint32_t self, int n, const uint32_t *args)
{
    Cpu saved = cpu;
    cpu.ecx = self;
    uint32_t r = rt_callback(fn, n, args);
    if (!rt_unwinding) cpu = saved;
    return r;
}

static int days_in(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0 ? 366 : 365; }

static void step(int *y, int *d, int by)
{
    *d += by;
    while (*d < 0) *d += days_in(--*y);
    while (*d >= days_in(*y)) *d -= days_in((*y)++);
}

static uint32_t nation_of(uint32_t club) { return readable(club) ? rd32(g_mem, club + CLUB_NATION) : 0; }

/* The fixture of club index idx on day d of year y in record key with the two nations of the friendly, or 0.
 * *any gets 1 when the club has any fixture that day (the original removes the first of them). */
static uint32_t linked_on(int idx, int y, int d, uint32_t key, uint32_t n1, uint32_t n2, int *any, uint32_t *list)
{
    *any = 0;
    *list = rt_alloc_zero(16);
    uint32_t a[9] = {(uint32_t)d, (uint32_t)y, (uint32_t)d, (uint32_t)y, (uint32_t)idx, *list, 0xffffffffu,
                     0xfffffffeu, 0xffffffffu};
    call_this(FIXTURE_SEARCH, rd32(g_mem, FIXTURES), 9, a);
    if (rt_unwinding) return 0;
    for (uint32_t node = rd32(g_mem, *list + 2), n = 0; readable(node) && n < 64; node = rd32(g_mem, node + 6), n++) {
        uint32_t f = rd32(g_mem, node), rec = readable(f) ? rd32(g_mem, f + 0x14) : 0;
        if (!readable(f)) continue;
        *any = 1;
        if (!readable(rec) || rd32(g_mem, rec) != rd32(g_mem, key)) continue;
        uint32_t a1 = nation_of(rd32(g_mem, f + 0x1c)), a2 = nation_of(rd32(g_mem, f + 0x20));
        if ((a1 == n1 && a2 == n2) || (a1 == n2 && a2 == n1)) return f;
    }
    return 0;
}

static void free_list(uint32_t list)
{
    if (!list) return;
    call_this(LIST_FREE, list, 0, NULL);
    rt_free(list);
}

void o_005cb9e0(void);

RT_WRAP(005cb9e0, national_fixture_remove)
{
    uint32_t f = rt_arg(0);
    int kind = readable(f) ? (rd16(g_mem, f + 0x4d) & 0x300) : 0;
    if (!season_game_changes() || (kind != 0x100 && kind != 0x200)) {
        o_005cb9e0();
        return;
    }
    Cpu saved = cpu;
    uint32_t home = rd32(g_mem, f + 0x1c), n1 = nation_of(home), n2 = nation_of(rd32(g_mem, f + 0x20));
    uint32_t a[1] = {n1};
    uint32_t club = kind == 0x100 ? home : rt_callback(U21_SIDE, 1, a);
    if (rt_unwinding) return;
    cpu = saved;
    uint32_t key = kind == 0x100 ? KEY_B : KEY_U21;
    if (readable(club) && readable(n1) && readable(n2)) {
        int idx = (int)rd32(g_mem, club), y0 = (int16_t)rd16(g_mem, f + 0x28), d0 = (int16_t)rd16(g_mem, f + 0x2a);
        int y = y0, d = d0, any = 0;
        uint32_t list = 0;
        step(&y, &d, -1);
        uint32_t found = linked_on(idx, y, d, key, n1, n2, &any, &list);    /* the original's day */
        free_list(list);
        if (rt_unwinding) return;
        for (int off = 1; !found && !any && off <= WINDOW && !rt_unwinding; off++) {
            for (int sign = -1; sign <= 1 && !found; sign += 2) {
                if (sign < 0 && off == 1) continue;                         /* the day before: done above */
                int yy = y0, dd = d0, other = 0;
                step(&yy, &dd, sign * off);
                found = linked_on(idx, yy, dd, key, n1, n2, &other, &list);
                if (found && !rt_unwinding) {
                    fprintf(stderr, "recomp: national link: the %.30s %.25s v %.25s on day %d of %d goes with its "
                                    "cancelled match of day %d of %d\n", win_str(rd32(g_mem, found + 0x14) + 4),
                            win_str(rd32(g_mem, found + 0x1c) + 4), win_str(rd32(g_mem, found + 0x20) + 4), dd, yy, d0,
                            y0);
                    uint32_t r[1] = {found};
                    call_this(REMOVE_FIXTURE, rd32(g_mem, FIXTURES), 1, r);
                }
                free_list(list);
                list = 0;
                if (rt_unwinding) return;
            }
        }
    }
    cpu = saved;
    o_005cb9e0();
}
