/* Cups of Britain and Ireland follow the divisions (0.7.2 league sizes, slice 4, D14 and D19;
 * src/recomp_rt/win/season_cup_sizes_uk.c). Each fake original below does what the original's collect_entrants
 * does with the club table, as read at its address, and marks a write past the end of its list or stack array:
 * - NirCup 0x78ac90: 32 places, the Premier and First Division clubs from place 12 with no bound (0x78ad9c),
 *   then 12 clubs of the lower pool (at most 30, 0x78ad57) at places 0 to 11 with byte4 2 (0x78ade6..0x78ae14);
 * - WelLgeCup 0x927010: 18 pointers (calloc at 0x92702a), every Welsh Premier club with no bound (0x9270cf), a box
 *   (wel_lge_cup.cpp 743, 0x92716b) when fewer than 18;
 * - EngFaCup 0x56fab0: the Conference clubs in a stack array of 22 (0x56fd98, no bound), 38 lower English clubs,
 *   then the 22 Conference places, the FA Trophy finalists, then the League clubs up to 152 (0x56fbc1).
 * The checks:
 * - rt_cup_clubs: the sum of the divisions in a season game, the original outside it and with sizes off;
 * - an Irish Cup with 20, 24 and 29 Premier and First Division clubs: 32 places, every such club in it with byte4 1,
 *   the rest from the lower pool, nothing past the list, the divisions given back;
 * - a Welsh League Cup with a Premier of 12: 18 clubs, the 12 and the 6 best Welsh lower clubs, no box;
 * - an FA Cup with a Conference of 24: 152 places, all 24 Conference clubs and the 92 League clubs once, nothing past
 *   the array, with FA Trophy finalists below the Conference, in its first 22 and hidden (after 22);
 * - sizes off (D19): each wrap is the original.
 * ctest season_cup_sizes_uk. */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

Cpu cpu;
uint8_t *g_mem;
int rt_unwinding;
uint32_t rt_callback(uint32_t target, int nargs, const uint32_t *args)
{
    (void)nargs, (void)args;
    fprintf(stderr, "unexpected call of 0x%x\n", target);
    exit(2);
}

static int sizes = 1;
int season_sizes_on(void) { return sizes; }

uint32_t rt_cup_clubs(uint32_t original, uint32_t key_a, uint32_t key_b, uint32_t key_c, uint32_t key_d);
void f_0078ac90(void);
void f_00927010(void);
void f_0056fab0(void);

static int failures;
#define CHECK(cond, ...) do { if (!(cond)) { failures++; fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
    fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } } while (0)

#define MEM 0xb00000u
#define CLUBS 0xae23b4u
#define CLUB_COUNT 0xae235cu
#define CLUB_LESS 0xae2350u
#define CLUB_SIZE 0x245u
#define NATION 0x53u
#define DIVISION 0x57u
#define REP 0x80u
#define EURO 0x1dbu
#define TABLE 0x100000u
#define RECORDS 0x80000u
#define CUP 0x60000u
#define LIST 0x200000u
#define GUARD 64u

/* Key globals of the original (docs/research/data-map/globals.tsv). */
#define K_ENG_PRM 0x9cf5bcu
#define K_ENG_FIRST 0x9cf5c0u
#define K_ENG_SECOND 0x9cf5c4u
#define K_ENG_THIRD 0x9cf5c8u
#define K_ENG_CONF 0x9cf69cu
#define K_ENG_LOWER 0x9cf764u
#define K_SCO_PRM 0x9cf5dcu
#define K_NIR_PRM 0x9cf8b0u
#define K_NIR_FIRST 0x9cf8b4u
#define K_NIR_LOWER 0x9cf8b8u
#define K_WEL_PRM 0x9cf94cu
#define K_WEL_LOWER 0x9cf960u
#define K_WALES 0x9cf510u
#define K_ENGLAND 0x9cf2e4u

static int nclubs;

/* A division record for a key: its id is the key's low bits, so each key has its own record. */
static uint32_t record_of(uint32_t key)
{
    uint32_t id = (key - 0x9cf000u) / 4u, rec = RECORDS + 0x10u * id;
    wr32(g_mem, key, id);
    wr32(g_mem, rec, id);
    return rec;
}

static uint32_t nation_of(uint32_t key)
{
    uint32_t id = 900u + (key - 0x9cf000u) / 4u, rec = RECORDS + 0x8000u + 0x10u * (key - 0x9cf000u) / 4u;
    wr32(g_mem, key, id);
    wr32(g_mem, rec, id);
    return rec;
}

static void reset(void)
{
    memset(g_mem, 0, MEM);
    nclubs = 0;
    wr32(g_mem, CLUBS, TABLE);
    wr32(g_mem, CLUB_COUNT, 0);
    wr32(g_mem, CLUB_LESS, 0);
}

/* n clubs of a division (key) and nation (key, 0 for none), reputations from rep down. */
static void add_clubs(uint32_t division, uint32_t nation, int n, int rep)
{
    uint32_t rec = record_of(division), nat = nation ? nation_of(nation) : 0;
    for (int i = 0; i < n; i++) {
        uint32_t c = TABLE + CLUB_SIZE * (uint32_t)nclubs++;
        wr32(g_mem, c, (uint32_t)nclubs);
        wr32(g_mem, c + DIVISION, rec);
        wr32(g_mem, c + NATION, nat);
        wr16(g_mem, c + REP, (uint32_t)(rep - i));
        wr32(g_mem, c + EURO, 0xffffffffu);
    }
    wr32(g_mem, CLUB_COUNT, (uint32_t)nclubs);
}

static int in_division(uint32_t club, uint32_t key)
{
    uint32_t d = rd32(g_mem, club + DIVISION);
    return d && rd32(g_mem, d) == rd32(g_mem, key);
}

static int guard_whole(uint32_t at)
{
    for (uint32_t i = 0; i < GUARD; i++)
        if (g_mem[at + i] != 0xee) return 0;
    return 1;
}

/* ---- Fake originals ---- */

static int overflow;

void o_0078ac90(void)
{
    uint32_t cup = cpu.ecx, list = LIST;
    memset(g_mem + list, 0, 32 * 6);
    memset(g_mem + list + 32 * 6, 0xee, GUARD);
    wr32(g_mem, cup + 0xa7u, list);
    wr16(g_mem, cup + 0x3eu, 32);
    uint32_t pool[30];
    int npool = 0, top = 0;
    int range = (int)rd32(g_mem, CLUB_COUNT) - 2 * (int)rd32(g_mem, CLUB_LESS);
    for (int i = 0; i < range; i++) {
        uint32_t c = TABLE + CLUB_SIZE * (uint32_t)i;
        if (!rd32(g_mem, c + DIVISION)) continue;
        if (npool < 30 && in_division(c, K_NIR_LOWER)) { pool[npool++] = c; continue; }
        if (in_division(c, K_NIR_PRM) || in_division(c, K_NIR_FIRST)) {
            uint32_t at = list + 6u * (uint32_t)(12 + top++);
            wr32(g_mem, at, c);
            g_mem[at + 4] = 1;
            g_mem[at + 5] = 0;
        }
    }
    for (int i = 0; i < 12; i++) {
        uint32_t at = list + 6u * (uint32_t)i;
        wr32(g_mem, at, i < npool ? pool[i] : 0);
        g_mem[at + 4] = 2;
        g_mem[at + 5] = 0;
    }
    overflow = !guard_whole(list + 32 * 6);
    cpu.eax = 1;
}

static int box;

void o_00927010(void)
{
    uint32_t cup = cpu.ecx, list = LIST;
    memset(g_mem + list, 0, 18 * 4);
    memset(g_mem + list + 18 * 4, 0xee, GUARD);
    wr32(g_mem, cup + 0xf2u, 18);
    wr32(g_mem, cup + 0xeeu, list);
    int n = 0;
    for (int i = 0; i < (int)rd32(g_mem, CLUB_COUNT); i++) {
        uint32_t c = TABLE + CLUB_SIZE * (uint32_t)i;
        if (in_division(c, K_WEL_PRM)) wr32(g_mem, list + 4u * (uint32_t)n++, c);
    }
    overflow = !guard_whole(list + 18 * 4);
    box = n < 18;
    cpu.eax = box ? 0 : 1;
}

static int league_key(uint32_t c)
{
    return in_division(c, K_ENG_PRM) || in_division(c, K_ENG_FIRST) || in_division(c, K_ENG_SECOND) ||
           in_division(c, K_ENG_THIRD);
}

/* The two FA Trophy finalists ([esp + 0x1c] and [esp + 0x30], 0x4afd80 at 0x56fcc7 and 0x56fcec), 0 for none. */
static uint32_t finalist[2];

static void drop(uint32_t *arr, int *n, uint32_t c)
{
    for (int i = 0; i < *n; i++)
        if (arr[i] == c) { arr[i] = arr[--*n]; return; }
}

void o_0056fab0(void)
{
    uint32_t cup = cpu.ecx, list = LIST;
    struct { uint32_t conf[22]; uint8_t guard[GUARD]; } stack;
    memset(stack.guard, 0xee, GUARD);
    uint32_t lower[300];
    int nconf = 0, nlower = 0, n = 0, conf_take = 22, lower_take = 38;
    wr16(g_mem, cup + 0x3eu, 0x98);
    wr32(g_mem, cup + 0xa7u, list);
    memset(g_mem + list, 0, 152 * 6);
    for (int i = 0; i < (int)rd32(g_mem, CLUB_COUNT); i++) {
        uint32_t c = TABLE + CLUB_SIZE * (uint32_t)i;
        if (!rd32(g_mem, c + DIVISION)) continue;
        if (in_division(c, K_ENG_CONF)) {
            if (nconf < 22) stack.conf[nconf] = c;
            else memset(stack.guard, 0, 4);        /* the store at 0x56fd98 runs past the array */
            nconf++;
        } else if (!league_key(c) && !in_division(c, K_ENG_LOWER) && nlower < 300) {
            uint32_t nat = rd32(g_mem, c + NATION);
            if (nat && rd32(g_mem, nat) == rd32(g_mem, K_ENGLAND)) lower[nlower++] = c;
        }
    }
    /* Each finalist leaves its array and takes one of its places (0x56fdbc..0x56ff9a); a club with no division
     * (+0x57 0) is skipped there and still added after the Conference places. */
    for (int k = 0; k < 2; k++) {
        uint32_t f = finalist[k];
        if (!f || !rd32(g_mem, f + DIVISION)) continue;
        if (in_division(f, K_ENG_CONF)) { drop(stack.conf, &nconf, f); conf_take--; }
        else if (!league_key(f) && !in_division(f, K_ENG_LOWER)) { drop(lower, &nlower, f); lower_take--; }
    }
    for (int i = 0; i < lower_take && i < nlower; i++, n++) wr32(g_mem, list + 6u * (uint32_t)n, lower[i]);
    for (int i = 0; i < conf_take && i < nconf && i < 22; i++, n++) wr32(g_mem, list + 6u * (uint32_t)n, stack.conf[i]);
    for (int k = 0; k < 2; k++)
        if (finalist[k]) wr32(g_mem, list + 6u * (uint32_t)n++, finalist[k]);
    for (int i = 0; i < (int)rd32(g_mem, CLUB_COUNT); i++) {
        uint32_t c = TABLE + CLUB_SIZE * (uint32_t)i;
        if (league_key(c) && n < 152) wr32(g_mem, list + 6u * (uint32_t)n++, c);
    }
    overflow = stack.guard[0] != 0xee;
    cpu.eax = 1;
}

/* ---- Checks ---- */

static int list_has(uint32_t list, int places, int size, uint32_t club)
{
    int k = 0;
    for (int i = 0; i < places; i++) k += rd32(g_mem, list + (uint32_t)(size * i)) == club;
    return k;
}

static void divisions_back(uint32_t key, int first, int n, const char *what)
{
    int k = 0;
    for (int i = first; i < first + n; i++) k += in_division(TABLE + CLUB_SIZE * (uint32_t)i, key);
    CHECK(k == n, "%s: %d of %d clubs have their division back", what, k, n);
}

static void test_counts(void)
{
    reset();
    add_clubs(K_ENG_PRM, 0, 20, 900);
    add_clubs(K_ENG_FIRST, 0, 24, 800);
    add_clubs(K_ENG_SECOND, 0, 23, 700);
    add_clubs(K_ENG_THIRD, 0, 24, 600);
    sizes = 1;
    CHECK(rt_cup_clubs(0x5c, K_ENG_PRM, K_ENG_FIRST, K_ENG_SECOND, K_ENG_THIRD) == 91, "League Cup of 91 clubs");
    CHECK(rt_cup_clubs(0x1e, K_ENG_FIRST, K_ENG_SECOND, K_ENG_THIRD, 0) == 71, "three divisions");
    wr32(g_mem, K_SCO_PRM, 0xffffffffu);                                 /* a nation that the game does not hold */
    CHECK(rt_cup_clubs(0x2a, K_SCO_PRM, K_ENG_FIRST, 0, 0) == 0x2a, "a division with no key keeps the original");
    sizes = 0;
    CHECK(rt_cup_clubs(0x5c, K_ENG_PRM, K_ENG_FIRST, K_ENG_SECOND, K_ENG_THIRD) == 0x5c, "sizes off: the original");
    sizes = 1;
}

static void nir_cup(int prem, int first, int lower, int on)
{
    reset();
    sizes = on;
    add_clubs(K_NIR_LOWER, 0, lower, 300);
    add_clubs(K_NIR_PRM, 0, prem, 900);
    add_clubs(K_NIR_FIRST, 0, first, 600);
    cpu.ecx = CUP;
    f_0078ac90();
    int top = prem + first, want_lower = top >= 20 ? 32 - top : 12;
    char what[64];
    snprintf(what, sizeof what, "Irish Cup %d + %d%s", prem, first, on ? "" : " (sizes off)");
    if (!on) {
        CHECK(overflow == (top > 20), "%s: the original's list", what);
        sizes = 1;
        return;
    }
    CHECK(!overflow, "%s: nothing past the 32 places", what);
    CHECK(rd16(g_mem, CUP + 0x3e) == 32, "%s: 32 places", what);
    uint32_t list = rd32(g_mem, CUP + 0xa7);
    int ones = 0, twos = 0, all = 1;
    for (int i = 0; i < 32; i++) {
        uint32_t at = list + 6u * (uint32_t)i;
        if (!rd32(g_mem, at)) continue;
        ones += g_mem[at + 4] == 1;
        twos += g_mem[at + 4] == 2;
    }
    for (int i = lower; i < lower + top; i++) all &= list_has(list, 32, 6, TABLE + CLUB_SIZE * (uint32_t)i) == 1;
    CHECK(all, "%s: every Premier and First Division club once", what);
    CHECK(ones == top && twos == want_lower, "%s: %d top clubs and %d lower, want %d and %d", what, ones, twos, top,
          want_lower);
    divisions_back(K_NIR_PRM, lower, prem, what);
    divisions_back(K_NIR_FIRST, lower + prem, first, what);
}

static void wel_lge_cup(int prem, int lower, int on)
{
    reset();
    sizes = on;
    add_clubs(K_WEL_LOWER, K_WALES, lower, 500);
    add_clubs(K_WEL_LOWER, K_ENGLAND, 4, 800);                          /* English clubs of the lower division */
    add_clubs(K_WEL_PRM, K_WALES, prem, 900);
    cpu.ecx = CUP;
    box = 0;
    f_00927010();
    char what[64];
    snprintf(what, sizeof what, "Welsh League Cup, Premier of %d%s", prem, on ? "" : " (sizes off)");
    if (!on) {
        CHECK(box == (prem < 18), "%s: the original's box", what);
        sizes = 1;
        return;
    }
    CHECK(!box && !overflow && cpu.eax == 1, "%s: 18 clubs and no box", what);
    uint32_t list = rd32(g_mem, CUP + 0xee);
    int all = 1;
    for (int i = lower + 4; i < lower + 4 + prem; i++) all &= list_has(list, 18, 4, TABLE + CLUB_SIZE * (uint32_t)i) == 1;
    for (int i = 0; i < 18 - prem; i++) all &= list_has(list, 18, 4, TABLE + CLUB_SIZE * (uint32_t)i) == 1;
    CHECK(all, "%s: the Premier clubs and the best %d Welsh lower clubs", what, 18 - prem);
    for (int i = lower; i < lower + 4; i++)
        CHECK(!list_has(list, 18, 4, TABLE + CLUB_SIZE * (uint32_t)i), "%s: no English club", what);
    divisions_back(K_WEL_LOWER, 0, lower + 4, what);
    divisions_back(K_WEL_PRM, lower + 4, prem, what);
}

/* finals: 0 none, 1 a club below the Conference and the last Conference club (hidden in the call), 2 the first
 * Conference club and a club below it. */
static void fa_cup(int conf, int on, int finals)
{
    reset();
    sizes = on;
    nation_of(K_ENGLAND);
    add_clubs(K_ENG_PRM, K_ENGLAND, 20, 900);
    add_clubs(K_ENG_FIRST, K_ENGLAND, 24, 800);
    add_clubs(K_ENG_SECOND, K_ENGLAND, 24, 700);
    add_clubs(K_ENG_THIRD, K_ENGLAND, 24, 600);
    int first_conf = nclubs;
    add_clubs(K_ENG_CONF, K_ENGLAND, conf, 500);
    int first_lower = nclubs;
    add_clubs(0x9cf6a4u, K_ENGLAND, 60, 300);                          /* the clubs below the Conference */
    finalist[0] = finalist[1] = 0;
    if (finals == 1) {
        finalist[0] = TABLE + CLUB_SIZE * (uint32_t)(first_lower + 50);
        finalist[1] = TABLE + CLUB_SIZE * (uint32_t)(first_conf + conf - 1);
    } else if (finals == 2) {
        finalist[0] = TABLE + CLUB_SIZE * (uint32_t)first_conf;
        finalist[1] = TABLE + CLUB_SIZE * (uint32_t)(first_lower + 50);
    }
    cpu.ecx = CUP;
    f_0056fab0();
    char what[80];
    snprintf(what, sizeof what, "FA Cup, Conference of %d, finalists %d%s", conf, finals, on ? "" : " (sizes off)");
    if (!on) {
        CHECK(overflow == (conf > 22), "%s: the original's array", what);
        sizes = 1;
        return;
    }
    CHECK(!overflow, "%s: nothing past the Conference array", what);
    uint32_t list = rd32(g_mem, CUP + 0xa7);
    int conf_once = 1, league_once = 1, places = 0, twice = 0;
    for (int i = first_conf; i < first_conf + conf; i++)
        conf_once &= list_has(list, 152, 6, TABLE + CLUB_SIZE * (uint32_t)i) == 1;
    for (int i = 0; i < first_conf; i++) league_once &= list_has(list, 152, 6, TABLE + CLUB_SIZE * (uint32_t)i) == 1;
    for (int i = 0; i < 152; i++) {
        uint32_t c = rd32(g_mem, list + 6u * (uint32_t)i);
        places += c != 0;
        twice += c && list_has(list, 152, 6, c) > 1;
    }
    CHECK(conf_once, "%s: every Conference club once", what);
    CHECK(league_once, "%s: every League club once", what);
    for (int k = 0; k < 2; k++)
        CHECK(!finalist[k] || list_has(list, 152, 6, finalist[k]) == 1, "%s: finalist %d once", what, k);
    CHECK(!twice, "%s: no club twice", what);
    CHECK(places == 152 && rd16(g_mem, CUP + 0x3e) == 152, "%s: 152 places, %d filled", what, places);
    divisions_back(K_ENG_CONF, first_conf, conf, what);
}

int main(void)
{
    g_mem = calloc(1, MEM);
    if (!g_mem) return 2;
    test_counts();
    nir_cup(10, 10, 40, 1);
    nir_cup(12, 12, 40, 1);
    nir_cup(12, 17, 40, 1);
    nir_cup(10, 8, 40, 1);
    nir_cup(12, 12, 40, 0);
    wel_lge_cup(12, 20, 1);
    wel_lge_cup(18, 20, 1);
    wel_lge_cup(12, 20, 0);
    fa_cup(22, 1, 0);
    fa_cup(22, 1, 2);
    fa_cup(24, 1, 0);
    fa_cup(24, 1, 1);
    fa_cup(24, 1, 2);
    fa_cup(24, 0, 0);
    if (failures) {
        fprintf(stderr, "season_cup_sizes_uk: %d failures\n", failures);
        return 1;
    }
    printf("season_cup_sizes_uk: ok\n");
    return 0;
}
