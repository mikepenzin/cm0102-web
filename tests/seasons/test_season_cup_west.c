/* The cups of western Europe take the real divisions (0.7.2 league sizes, slice 5;
 * src/recomp_rt/win/season_cup_sizes_west.c). A made-up club table in a made-up memory, with grown and shrunk
 * divisions:
 * - Belgian Cup: 112 places less the five divisions go to pool clubs (a First Division of 16, a Second Division of
 *   19), at most the pool; the original's 44 in the base game; the third tier from the place after the pool;
 * - Dutch Cup: 86 less the Eredivisie and an Eerste Divisie of 20 (48, the original's 50 would write past the
 *   list), at most the pool; 50 in the base game;
 * - Danish Cup: a First Division of 12 and too few lower clubs: Danish clubs with no division take the empty
 *   places after the lower clubs, by reputation, never a club with fewer than 15 players, a club of another nation
 *   or a national side; the divisions keep the end of the list; the cup returns 1;
 * - Coppa Italia Serie C: C2 groups of 17, 16 and 16 leave 5 of 90 places empty: Italian clubs below Serie C fill
 *   them, a division before no division, then reputation;
 * - Coppa Italia: a Serie A of 20 whose clubs placed 15 to 17 the original leaves out: each takes the pool place of
 *   the Serie C club with the highest last position;
 * - French League Cup: a National of 17 with two Ligues of 18 leaves a place: the best French club from below the
 *   National with 15 players takes it, first in the list, and the cup returns 1;
 * - Danish Cup with too few clubs of 15 players: the clubs with the most players fill the rest;
 * - CFA: 66 of 72 places: French clubs from below the National fill the array, a lower division first;
 * - French Lower Division: a National of 17 and 20 lower clubs: 3 French lower clubs with no zone fill it to 40,
 *   before a club of the CFA zone;
 * - French Cup: a list left short takes French clubs with 15 players or more before Ligue 2 and Ligue 1, and the
 *   cup returns 1.
 * ctest season_cup_west. */
#include "win.h"
#include "season.h"
#include "season_cup_sizes_west.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
static int game = 1;
int season_game_changes(void) { return game; }
uint32_t rt_cup_add(uint32_t original, uint32_t base, uint32_t entrants, uint32_t key_a, uint32_t key_b)
{
    (void)entrants, (void)key_a, (void)key_b;
    return game ? 100u - base : original;
}
uint32_t rt_bel_cup_lower(uint32_t original, uint32_t pool);
uint32_t rt_bel_cup_place(uint32_t original, uint32_t taken);
uint32_t rt_hol_cup_lower(uint32_t original, uint32_t esp);
uint32_t rt_ita_c_cup_fill(uint32_t original, uint32_t cup, uint32_t placed);
uint32_t rt_fra_cfa_fill(uint32_t array, uint32_t count);
uint32_t rt_fra_lower_fill(uint32_t array, uint32_t count);
void f_00551240(void);
void f_005baa50(void);

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define MEM 0xc00000u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define NATION_COUNT 0xae2350u
#define NATIONS 0xae23a8u
#define TABLE 0x100000u
#define SIZE 0x245u
#define RECORDS 0x80000u
#define NATION_TABLE 0x90000u
#define LIST 0xa0000u
#define CUP 0xb0000u
#define LOWER 0x9cf764u

static int nclubs;

/* A division record with id `id` and level `level`, its key global set. */
static uint32_t division(uint32_t key, int32_t id, int level)
{
    uint32_t rec = RECORDS + (uint32_t)id * 0x100u;
    wr32(g_mem, rec, (uint32_t)id);
    wr16(g_mem, rec + 0x69, (uint16_t)level);
    if (key) wr32(g_mem, key, (uint32_t)id);
    return rec;
}

static uint32_t nation(uint32_t key, int n)
{
    wr32(g_mem, key, (uint32_t)n);
    return NATION_TABLE + (uint32_t)n * 0x122u;
}

/* A club of a nation in a division (0: none), with a reputation and a squad of `players`. */
static uint32_t club(uint32_t nat, uint32_t div, int rep, int players)
{
    uint32_t c = TABLE + (uint32_t)nclubs++ * SIZE;
    memset(g_mem + c, 0, SIZE);
    wr32(g_mem, c + 0x53, nat);
    wr32(g_mem, c + 0x57, div);
    wr16(g_mem, c + 0x80, (uint16_t)rep);
    for (int i = 0; i < players && i < 50; i++) wr32(g_mem, c + 0xd7 + 4u * (uint32_t)i, 0x1000u + (uint32_t)i);
    wr32(g_mem, CLUB_COUNT, (uint32_t)nclubs);
    return c;
}

static void reset(void)
{
    memset(g_mem, 0, MEM);
    nclubs = 0;
    wr32(g_mem, CLUBS, TABLE);
    wr32(g_mem, NATIONS, NATION_TABLE);
    wr32(g_mem, NATION_COUNT, 0);
    game = 1;
}

static void belgium(void)
{
    reset();
    static const uint32_t keys[] = {0x9cf7c4u, 0x9cf5a4u, 0x9cf5a8u, 0x9cf5a0u, 0x9cf59cu};
    static const int sizes[] = {16, 16, 16, 19, 16};          /* third tier, a Second Division of 19, First 16 */
    uint32_t bel = nation(0x9cf2a0u, 3);
    for (int k = 0; k < 5; k++) {
        uint32_t d = division(keys[k], 10 + k, 10 - k);
        for (int i = 0; i < sizes[k]; i++) club(bel, d, 100, 20);
    }
    CHECK(rt_bel_cup_lower(0x2cu, 150u) == 112u - 83u, "Belgian Cup: %u pool clubs, want 29",
          rt_bel_cup_lower(0x2cu, 150u));
    CHECK(rt_bel_cup_lower(0x2cu, 20u) == 20u, "Belgian Cup: at most the pool");
    CHECK(rt_bel_cup_place(0x108u, 29u) == 29u * 6u, "Belgian Cup: the third tier after the pool");
    game = 0;
    CHECK(rt_bel_cup_lower(0x2cu, 150u) == 0x2cu && rt_bel_cup_place(0x108u, 29u) == 0x108u,
          "Belgian Cup: the base game keeps 44 and place 44");
}

static void holland(void)
{
    reset();
    uint32_t hol = nation(0x9cf338u, 4), ere = division(0x9cf56cu, 20, 10), eer = division(0x9cf568u, 21, 9);
    for (int i = 0; i < 18; i++) club(hol, ere, 100, 20);
    for (int i = 0; i < 20; i++) club(hol, eer, 80, 20);          /* a grown Eerste Divisie */
    uint32_t esp = 0xc0000u;
    wr16(g_mem, esp + 0x10, 120);
    CHECK(rt_hol_cup_lower(0x32u, esp) == 48u, "Dutch Cup: %u pool clubs, want 48", rt_hol_cup_lower(0x32u, esp));
    wr16(g_mem, esp + 0x10, 30);
    CHECK(rt_hol_cup_lower(0x32u, esp) == 30u, "Dutch Cup: at most the pool");
    game = 0;
    CHECK(rt_hol_cup_lower(0x32u, esp) == 0x32u, "Dutch Cup: the base game keeps 50");
}

/* ---- Denmark: the original's collect_entrants, as 0x551240 does it, with a room of 52 lower clubs ---- */
static uint32_t den_lower[60], den_div[40];
static int nden_lower, nden_div;

void o_00551240(void)
{
    uint32_t cup = cpu.ecx, list = LIST;
    wr16(g_mem, cup + 0x3e, 92);
    wr32(g_mem, cup + 0xa7, list);
    int n = 0;
    for (int i = 0; i < nden_lower && n < 52; i++, n++) wr32(g_mem, list + 6u * (uint32_t)n, den_lower[i]);
    for (int i = 0; i < nden_div && n < 92; i++, n++) {
        wr32(g_mem, list + 6u * (uint32_t)n, den_div[i]);
        g_mem[list + 6u * (uint32_t)n + 4] = 0;
    }
    cpu.eax = n == 92;
}

static void denmark(void)
{
    reset();
    uint32_t den = nation(0x9cf2c0u, 5), other = nation(0x9cf2a0u, 6);
    uint32_t lower = division(LOWER, 30, 1), second = division(0x9cf5b4u, 31, 3), first = division(0x9cf5b0u, 32, 4),
             prm = division(0x9cf5acu, 33, 5);
    nden_lower = nden_div = 0;
    for (int i = 0; i < 40; i++) den_lower[nden_lower++] = club(den, lower, 50, 20);
    for (int i = 0; i < 16; i++) den_div[nden_div++] = club(den, second, 70, 20);
    for (int i = 0; i < 12; i++) den_div[nden_div++] = club(den, first, 80, 20);   /* a First Division of 12 */
    for (int i = 0; i < 12; i++) den_div[nden_div++] = club(den, prm, 90, 20);
    uint32_t none[14];
    for (int i = 0; i < 14; i++) none[i] = club(den, 0, 30 + i, 20);        /* the clubs that the moves cut */
    uint32_t squadless = club(den, 0, 99, 3), foreign = club(other, 0, 98, 20);
    cpu.ecx = CUP;
    f_00551240();
    CHECK(cpu.eax == 1, "Danish Cup: 92 clubs, the cup returns 1");
    int fine = 1;
    for (int i = 0; i < 40; i++) fine &= rd32(g_mem, LIST + 6u * (uint32_t)i) == den_lower[i];
    for (int i = 0; i < 12; i++) fine &= rd32(g_mem, LIST + 6u * (uint32_t)(40 + i)) == none[13 - i];
    for (int i = 0; i < 40; i++) fine &= rd32(g_mem, LIST + 6u * (uint32_t)(52 + i)) == den_div[i];
    CHECK(fine, "Danish Cup: lower clubs, then 12 clubs with no division by reputation, then the divisions");
    for (int i = 0; i < 92; i++) {
        uint32_t c = rd32(g_mem, LIST + 6u * (uint32_t)i);
        CHECK(c != squadless && c != foreign, "Danish Cup: place %d holds a club with no squad or of another nation", i);
    }
    /* the base game: the original's short list stays short */
    game = 0;
    memset(g_mem + LIST, 0, 6 * 92);
    f_00551240();
    CHECK(cpu.eax == 0 && !rd32(g_mem, LIST + 6u * 80u), "Danish Cup: the base game keeps the original");
}

static void italy_c(void)
{
    reset();
    uint32_t ita = nation(0x9cf360u, 7);
    static const uint32_t ckeys[] = {0x9cf578u, 0x9cf57cu, 0x9cf580u, 0x9cf584u, 0x9cf588u};
    static const int csize[] = {18, 18, 17, 16, 16};
    division(0x9cf570u, 40, 10);
    division(0x9cf574u, 41, 9);
    uint32_t list = LIST;
    int n = 0;
    for (int k = 0; k < 5; k++) {
        uint32_t d = division(ckeys[k], 42 + k, 8 - k / 2);
        for (int i = 0; i < csize[k]; i++) wr32(g_mem, list + 4u * (uint32_t)n++, club(ita, d, 60, 20));
    }
    for (int i = n; i < 90; i++) wr32(g_mem, list + 4u * (uint32_t)i, 0xdeadbeefu);    /* the block is not cleared */
    uint32_t low = division(LOWER, 50, 2);
    uint32_t serie_d[3] = {club(ita, low, 20, 20), club(ita, low, 40, 20), club(ita, low, 30, 20)};
    uint32_t none_high = club(ita, 0, 90, 20), none_low = club(ita, 0, 10, 20);
    club(ita, low, 99, 2);                                       /* no squad */
    club(ita, RECORDS + 40u * 0x100u, 95, 20);                   /* a Serie A club */
    wr32(g_mem, CUP + 0xee, list);
    wr32(g_mem, CUP + 0xf2, 90);
    CHECK(rt_ita_c_cup_fill(0x50u, CUP, (uint32_t)n) == 0x50u, "Coppa Italia Serie C: the second pass from 80");
    uint32_t want[5] = {serie_d[1], serie_d[2], serie_d[0], none_high, none_low};
    for (int i = 0; i < 5; i++)
        CHECK(rd32(g_mem, list + 4u * (uint32_t)(85 + i)) == want[i], "Coppa Italia Serie C: place %d", 85 + i);
}

static void italy_cup(void)
{
    reset();
    uint32_t ita = nation(0x9cf360u, 7);
    uint32_t a = division(0x9cf570u, 40, 10), b = division(0x9cf574u, 41, 9), c1 = division(0x9cf578u, 42, 8);
    uint32_t list = LIST, as[20], cs[6];
    for (int i = 0; i < 20; i++) as[i] = club(ita, a, 100, 20), g_mem[as[i] + 0x5f] = (uint8_t)(i + 1);
    int n = 0;
    for (int i = 0; i < 6; i++) {
        cs[i] = club(ita, c1, 40 + i, 20);
        g_mem[cs[i] + 0x5f] = (uint8_t)(i < 3 ? 3 + i : 6 + i);    /* positions 3, 4, 5, 9, 10, 11 */
        wr32(g_mem, list + 4u * (uint32_t)n++, cs[i]);
    }
    for (int i = 0; i < 22; i++) wr32(g_mem, list + 4u * (uint32_t)n++, club(ita, b, 70, 20));
    for (int i = 17; i < 20; i++) wr32(g_mem, list + 4u * (uint32_t)n++, club(ita, b, 75, 20));   /* last A 18-20 */
    while (n < 32) wr32(g_mem, list + 4u * (uint32_t)n++, club(ita, c1, 45, 20));
    for (int i = 0; i < 14; i++) wr32(g_mem, list + 4u * (uint32_t)n++, as[i]);                     /* A 1 to 14 */
    wr32(g_mem, list + 4u * (uint32_t)n++, as[17]);                                                 /* fill to 48 */
    wr32(g_mem, list + 4u * (uint32_t)n++, as[18]);
    wr32(g_mem, CUP + 0xee, list);
    wr32(g_mem, CUP + 0xf2, 48);
    season_cup_sizes_ita_cup(CUP);
    for (int i = 14; i < 20; i++) {
        int in = 0;
        for (int j = 0; j < 48 && !in; j++) in = rd32(g_mem, list + 4u * (uint32_t)j) == as[i];
        CHECK(in, "Coppa Italia: the club placed %d of a Serie A of 20 takes part", i + 1);
    }
    int kept = 0;
    for (int j = 0; j < 48; j++) kept += rd32(g_mem, list + 4u * (uint32_t)j) == cs[0];
    CHECK(kept == 1, "Coppa Italia: the C1 club placed 3 keeps its place");
    for (int i = 3; i < 6; i++)
        CHECK(rd32(g_mem, list + 4u * (uint32_t)i) != cs[i], "Coppa Italia: the C1 club placed %d gives its place",
              g_mem[cs[i] + 0x5f]);
}

/* FraCup::collect_entrants, as 0x5baa50 does it: 200 lower places (some left empty), then Ligue 2 and Ligue 1. */
static uint32_t fra_lower[200], fra_top[36];
static int nfra_lower;

void o_005baa50(void)
{
    uint32_t cup = cpu.ecx, list = LIST;
    wr16(g_mem, cup + 0x3e, 222);
    wr32(g_mem, cup + 0xa7, list);
    int n = 0;
    for (int i = 0; i < nfra_lower; i++) wr32(g_mem, list + 6u * (uint32_t)n++, fra_lower[i]);
    for (int i = 0; i < 36; i++) wr32(g_mem, list + 6u * (uint32_t)n++, fra_top[i]);
    cpu.eax = n == 222;
}

static void france_short(void)
{
    reset();
    uint32_t fra = nation(0x9cf300u, 8), l2 = division(0x9cf5d0u, 60, 9), l1 = division(0x9cf5ccu, 61, 10),
             low = division(LOWER, 62, 2);
    nfra_lower = 0;
    for (int i = 0; i < 183; i++) fra_lower[nfra_lower++] = club(fra, low, 20, 20);
    for (int i = 0; i < 18; i++) fra_top[i] = club(fra, l2, 60, 20);
    for (int i = 0; i < 18; i++) fra_top[18 + i] = club(fra, l1, 80, 20);
    uint32_t spare[3] = {club(fra, 0, 15, 20), club(fra, low, 5, 20), club(fra, 0, 12, 20)};
    club(fra, 0, 90, 4);                                          /* no squad */
    cpu.ecx = CUP;
    f_005baa50();
    CHECK(cpu.eax == 1, "French Cup: a short list is filled to 222");
    uint32_t want[3] = {spare[1], spare[0], spare[2]};             /* a division first, then reputation */
    for (int i = 0; i < 3; i++)
        CHECK(rd32(g_mem, LIST + 6u * (uint32_t)(183 + i)) == want[i], "French Cup: place %d", 183 + i);
    CHECK(rd32(g_mem, LIST + 6u * 186u) == fra_top[0] && rd32(g_mem, LIST + 6u * 221u) == fra_top[35],
          "French Cup: Ligue 2 and Ligue 1 keep the end of the list");
}

static void france_league_cup(void)
{
    reset();
    uint32_t fra = nation(0x9cf300u, 8), nat = division(0x9cf5d4u, 63, 8), l2 = division(0x9cf5d0u, 60, 9),
             l1 = division(0x9cf5ccu, 61, 10), cfa = division(0, 64, 7), low = division(LOWER, 62, 2);
    int n = 0;
    for (int i = 0; i < 17; i++) wr32(g_mem, LIST + 6u * (uint32_t)n++, club(fra, nat, 50, 20));
    for (int i = 0; i < 18; i++) wr32(g_mem, LIST + 6u * (uint32_t)n++, club(fra, l2, 60, 20));
    for (int i = 0; i < 18; i++) wr32(g_mem, LIST + 6u * (uint32_t)n++, club(fra, l1, 80, 20));
    club(fra, low, 99, 20);
    uint32_t best = club(fra, cfa, 30, 20);
    club(fra, cfa, 95, 5);                                         /* no squad */
    wr16(g_mem, CUP + 0x3e, 54);
    wr32(g_mem, CUP + 0xa7, LIST);
    CHECK(season_cup_sizes_fra_lge(CUP) == 1, "French League Cup: 54 clubs");
    CHECK(rd32(g_mem, LIST) == best, "French League Cup: the CFA club with a squad comes first");
    CHECK(rd32(g_mem, LIST + 6u * 53u) != 0 && rd32(g_mem, LIST + 6u * 1u) != best, "French League Cup: one club in");
    game = 0;
    CHECK(season_cup_sizes_fra_lge(CUP) == 0, "French League Cup: the base game keeps the original");
}

static void denmark_few(void)
{
    reset();
    uint32_t den = nation(0x9cf2c0u, 5);
    uint32_t lower = division(LOWER, 30, 1), second = division(0x9cf5b4u, 31, 3), first = division(0x9cf5b0u, 32, 4),
             prm = division(0x9cf5acu, 33, 5);
    nden_lower = nden_div = 0;
    for (int i = 0; i < 48; i++) den_lower[nden_lower++] = club(den, lower, 50, 20);
    for (int i = 0; i < 16; i++) den_div[nden_div++] = club(den, second, 70, 20);
    for (int i = 0; i < 12; i++) den_div[nden_div++] = club(den, first, 80, 20);
    for (int i = 0; i < 12; i++) den_div[nden_div++] = club(den, prm, 90, 20);
    uint32_t few[5] = {club(den, 0, 10, 12), club(den, 0, 10, 2), club(den, 0, 10, 14), club(den, 0, 10, 9),
                       club(den, 0, 10, 0)};
    cpu.ecx = CUP;
    f_00551240();
    CHECK(cpu.eax == 1, "Danish Cup: filled with the clubs that have the most players");
    uint32_t want[4] = {few[2], few[0], few[3], few[1]};
    for (int i = 0; i < 4; i++) CHECK(rd32(g_mem, LIST + 6u * (uint32_t)(48 + i)) == want[i], "Danish Cup: place %d", 48 + i);
}

static void cfa(void)
{
    reset();
    uint32_t fra = nation(0x9cf300u, 8), nat = division(0x9cf5d4u, 63, 8), l2 = division(0x9cf5d0u, 60, 9),
             l1 = division(0x9cf5ccu, 61, 10), low = division(LOWER, 62, 2);
    uint32_t array = 0xc0000u;
    int n = 0;
    for (int i = 0; i < 18; i++) wr32(g_mem, array + 4u * (uint32_t)n++, club(fra, l1, 80, 20));
    for (int i = 0; i < 18; i++) wr32(g_mem, array + 4u * (uint32_t)n++, club(fra, l2, 60, 20));
    for (int i = 0; i < 30; i++) wr32(g_mem, array + 4u * (uint32_t)n++, club(fra, low, 20, 20));
    for (int i = 0; i < 20; i++) club(fra, nat, 50, 20);
    uint32_t more[6];
    for (int i = 0; i < 4; i++) more[i] = club(fra, low, 30 + i, 20);
    more[4] = club(fra, 0, 90, 20);
    more[5] = club(fra, 0, 80, 20);
    CHECK(rt_fra_cfa_fill(array, 0x10000u | 66u) == (0x10000u | 72u), "CFA: 72 for the shuffle");
    uint32_t want[6] = {more[3], more[2], more[1], more[0], more[4], more[5]};
    for (int i = 0; i < 6; i++) CHECK(rd32(g_mem, array + 4u * (uint32_t)(66 + i)) == want[i], "CFA: place %d", 66 + i);
    game = 0;
    CHECK(rt_fra_cfa_fill(array, 66u) == 66u, "CFA: the base game keeps the count");
}

static void french_lower(void)
{
    reset();
    uint32_t fra = nation(0x9cf300u, 8), nat = division(0x9cf5d4u, 63, 8), low = division(LOWER, 62, 2);
    uint32_t cfa = 0xd0000u;
    wr32(g_mem, cfa, 77);
    wr32(g_mem, 0x9cf5d8u, 77);
    uint32_t array = 0xc0000u;
    int n = 0;
    for (int i = 0; i < 17; i++) wr32(g_mem, array + 4u * (uint32_t)n++, club(fra, nat, 50, 20));
    for (int i = 0; i < 20; i++) wr32(g_mem, array + 4u * (uint32_t)n++, club(fra, low, 20, 20));
    uint32_t zoned = club(fra, low, 90, 20);
    wr32(g_mem, zoned + 0x60, cfa);
    uint32_t plain[3] = {club(fra, low, 30, 20), club(fra, low, 25, 20), club(fra, low, 22, 20)};
    CHECK(rt_fra_lower_fill(array, (uint32_t)n) == 40u, "French Lower Division: 40 for the shuffle");
    for (int i = 0; i < 3; i++)
        CHECK(rd32(g_mem, array + 4u * (uint32_t)(37 + i)) == plain[i], "French Lower Division: place %d", 37 + i);
}

int main(void)
{
    g_mem = calloc(1, MEM);
    if (!g_mem) return 2;
    belgium();
    holland();
    denmark();
    italy_c();
    italy_cup();
    france_short();
    france_league_cup();
    denmark_few();
    cfa();
    french_lower();
    printf("%s season_cup_west\n", failures ? "FAIL" : "PASS");
    return failures ? 1 : 0;
}
